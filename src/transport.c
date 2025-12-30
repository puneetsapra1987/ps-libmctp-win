/* SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later */

#define _GNU_SOURCE

#include "mctp_transport.h"
#include "libmctp.h"
#include "libmctp-mmbi.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct mctp_transport {
    struct mctp *mctp;
    struct mctp_binding_mmbi *mmbi;
    mctp_transport_rx_cb rx_cb;
    void *user_context;
};

/* Internal libmctp callback */
static void transport_rx_handler(uint8_t eid, bool tag_owner, uint8_t msg_tag,
                                 void *data, void *msg, size_t len)
{
    struct mctp_transport *ctx = (struct mctp_transport *)data;
    if (ctx && ctx->rx_cb) {
        ctx->rx_cb(eid, msg, len, ctx->user_context);
    }
}

mctp_transport_t* mctp_transport_init(const char *device_path, uint8_t local_eid)
{
    mctp_transport_t *ctx = calloc(1, sizeof(mctp_transport_t));
    if (!ctx) return NULL;

    ctx->mctp = mctp_init();
    if (!ctx->mctp) {
        free(ctx);
        return NULL;
    }

    ctx->mmbi = mctp_mmbi_init();
    if (!ctx->mmbi) {
        mctp_destroy(ctx->mctp);
        free(ctx);
        return NULL;
    }

    // Attempt to open device
    if (mctp_mmbi_init_device(ctx->mmbi, device_path) != 0) {
        // Cleanup handled by destroy usually, but manual here
        mctp_mmbi_destroy(ctx->mmbi);
        mctp_destroy(ctx->mctp);
        free(ctx);
        return NULL;
    }

    // Register bus and enable TX
    mctp_register_bus(ctx->mctp, &ctx->mmbi->binding, local_eid);
    mctp_binding_set_tx_enabled(&ctx->mmbi->binding, true);
    
    // Set internal RX handler to route to user callback
    mctp_set_rx_all(ctx->mctp, transport_rx_handler, ctx);

    return ctx;
}

void mctp_transport_destroy(mctp_transport_t *ctx)
{
    if (!ctx) return;
    
    if (ctx->mctp && ctx->mmbi) {
        mctp_unregister_bus(ctx->mctp, &ctx->mmbi->binding);
    }
    
    if (ctx->mmbi) mctp_mmbi_destroy(ctx->mmbi);
    if (ctx->mctp) mctp_destroy(ctx->mctp);
    
    free(ctx);
}

void mctp_transport_set_rx_callback(mctp_transport_t *ctx, mctp_transport_rx_cb cb, void *user_context)
{
    if (ctx) {
        ctx->rx_cb = cb;
        ctx->user_context = user_context;
    }
}

int mctp_transport_send(mctp_transport_t *ctx, uint8_t dst_eid, const void *data, size_t len)
{
    if (!ctx || !ctx->mctp) return -1;
    
    // Tag handling is internal or dummy for now (tag 0)
    // mctp_message_tx takes (mctp, eid, tag_owner, tag, msg, len)
    // We'll use request (tag_owner=false)? Or true? 
    // Usually requester is owner=true. Responder is owner=false.
    // For simplicity, we'll set tag_owner=false (like previous tests) or true depending on usage.
    // Let's default to false for generic messages (like previous test_file_transfer).
    
    return mctp_message_tx(ctx->mctp, dst_eid, false, 0, (void*)data, len);
}

int mctp_transport_poll(mctp_transport_t *ctx)
{
    if (!ctx || !ctx->mmbi) return -1;
    
    return mctp_mmbi_poll(ctx->mmbi);
}
