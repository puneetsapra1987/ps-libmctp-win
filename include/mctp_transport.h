/* SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later */

#ifndef _MCTP_TRANSPORT_H
#define _MCTP_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mctp_transport mctp_transport_t;

/* Callback function prototype for received messages */
typedef void (*mctp_transport_rx_cb)(uint8_t src_eid, void *data, size_t len, void *user_context);

/**
 * @brief Initialize the MCTP transport on a specific MMBI device.
 * 
 * @param device_path Path to the device (e.g., "\\\\.\\MMBI0" or "\\\\.\\pipe\\MMBI0").
 * @param local_eid The EID to assign to this endpoint.
 * @return mctp_transport_t* Opaque handle, or NULL on failure.
 */
mctp_transport_t* mctp_transport_init(const char *device_path, uint8_t local_eid);

/**
 * @brief Destroy the transport context and free resources.
 * 
 * @param ctx The transport context.
 */
void mctp_transport_destroy(mctp_transport_t *ctx);

/**
 * @brief Register a callback for incoming messages.
 * 
 * @param ctx The transport context.
 * @param cb The callback function.
 * @param user_context User data to pass to the callback.
 */
void mctp_transport_set_rx_callback(mctp_transport_t *ctx, mctp_transport_rx_cb cb, void *user_context);

/**
 * @brief Send a message to a destination EID.
 * 
 * @param ctx The transport context.
 * @param dst_eid The destination Endpoint ID.
 * @param data Pointer to the message payload.
 * @param len Length of the message payload.
 * @return int 0 on success, negative error code on failure.
 */
int mctp_transport_send(mctp_transport_t *ctx, uint8_t dst_eid, const void *data, size_t len);

/**
 * @brief Poll the transport for incoming data.
 * This function should be called periodically or in a loop.
 * It reads from the underlying device and triggers the Rx callback if a message is reassembled.
 * 
 * @param ctx The transport context.
 * @return int 0 on success (even if no data), negative on device error.
 */
int mctp_transport_poll(mctp_transport_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* _MCTP_TRANSPORT_H */
