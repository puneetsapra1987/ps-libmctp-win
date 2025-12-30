/* SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later */

#ifndef _LIBMCTP_MMBI_H
#define _LIBMCTP_MMBI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libmctp.h>

#ifdef _WIN32
#include <windows.h>
#endif

struct mctp_binding_mmbi {
	struct mctp_binding binding;
	void *rx_storage;
	void *tx_storage;
	size_t memory_size;
	void *device_handle; /* HANDLE on Windows */
#ifdef _WIN32
	CRITICAL_SECTION lock;
#endif
};

struct mctp_binding_mmbi *mctp_mmbi_init(void);
void mctp_mmbi_destroy(struct mctp_binding_mmbi *mmbi);

/* Initialize memory regions for TX and RX.
 * @tx_addr: Pointer to memory mapped region for transmitting packets.
 * @rx_addr: Pointer to memory mapped region for receiving packets.
 * @size: Size of the memory regions (assumed same for now, or use max).
 */
int mctp_mmbi_init_mem(struct mctp_binding_mmbi *mmbi, 
		       void *tx_addr, void *rx_addr, size_t size);

/* File/Device based initialization */
int mctp_mmbi_init_file(struct mctp_binding_mmbi *mmbi, int fd);
/* For Windows HANDLE or Device Path initialization */
int mctp_mmbi_init_device(struct mctp_binding_mmbi *mmbi, const char *device_path);

/* Function to be called when data is available in the RX memory region */
int mctp_mmbi_rx(struct mctp_binding_mmbi *mmbi, size_t len);

/* Poll for incoming data from the device */
int mctp_mmbi_poll(struct mctp_binding_mmbi *mmbi);

/* 
 * --------------------------------------------------------------------------
 * High-Level "User Defined" API
 * --------------------------------------------------------------------------
 * Use these functions to interact with MMBI without managing the core libmctp 
 * structures directly.
 */

// Opaque context for the high-level user
typedef struct mctp_mmbi_context mctp_mmbi_context_t;
typedef void (*mctp_mmbi_rx_cb)(uint8_t src_eid, void *data, size_t len, void *user_context);

/* Initialize the MMBI context, core MCTP, and bindings in one shot */
mctp_mmbi_context_t* mctp_mmbi_context_init(const char *device_path, uint8_t local_eid);
void mctp_mmbi_context_destroy(mctp_mmbi_context_t *ctx);

/* Send user data to a destination EID */
int mctp_mmbi_send(mctp_mmbi_context_t *ctx, uint8_t dst_eid, const void *data, size_t len);

/* Register callback for received data */
void mctp_mmbi_set_rx_callback(mctp_mmbi_context_t *ctx, mctp_mmbi_rx_cb cb, void *user_context);

/* Enable or disable debug logging */
void mctp_mmbi_set_debug(mctp_mmbi_context_t *ctx, bool enable);

/* Poll for activity (calls rx_callback if data arrives) */
int mctp_mmbi_context_poll(mctp_mmbi_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* _LIBMCTP_MMBI_H */

