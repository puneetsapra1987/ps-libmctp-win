
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "libmctp-mmbi.h"
#include "libmctp-alloc.h"
#include "libmctp-log.h"
#include "container_of.h"

#define BINDING_NAME "mmbi"

/*
 * MMBI binding implementation.
 * 
 * This binding assumes we have two memory mapped regions:
 * 1. TX Region: Where libmctp writes packets to be sent.
 * 2. RX Region: Where incoming packets are found.
 */

struct mctp_binding_mmbi *mctp_mmbi_init(void)
{
	struct mctp_binding_mmbi *mmbi;

	mmbi = __mctp_alloc(sizeof(*mmbi));
	if (!mmbi)
		return NULL;

	memset(mmbi, 0, sizeof(*mmbi));
	mmbi->binding.name = BINDING_NAME;
	mmbi->binding.version = 1;
	mmbi->binding.tx = NULL; /* Set in init_mem */
	mmbi->binding.start = NULL;
	mmbi->binding.pkt_size = MCTP_PACKET_SIZE(65536);
	mmbi->binding.pkt_header = 0;
	mmbi->binding.pkt_trailer = 0;

#ifdef _WIN32
	InitializeCriticalSection(&mmbi->lock);
#endif

	return mmbi;
}

void mctp_mmbi_destroy(struct mctp_binding_mmbi *mmbi)
{
	if (!mmbi) return;
#ifdef _WIN32
	DeleteCriticalSection(&mmbi->lock);
#endif
	__mctp_free(mmbi);
}

#ifdef _WIN32
#include <windows.h>
#else
#define HANDLE void*
#define INVALID_HANDLE_VALUE NULL
#endif

// ... existing includes ...

static int mctp_mmbi_start(struct mctp_binding *b);

static int mctp_mmbi_tx(struct mctp_binding *b, struct mctp_pktbuf *pkt)
{
	struct mctp_binding_mmbi *mmbi = container_of(b, struct mctp_binding_mmbi, binding);
	size_t len;
	void *buf;

	len = mctp_pktbuf_size(pkt);
	buf = (void *)mctp_pktbuf_hdr(pkt);

#ifdef _WIN32
	if (mmbi->device_handle && mmbi->device_handle != INVALID_HANDLE_VALUE) {
		DWORD bytesWritten;
		BOOL res = FALSE;
		int retries = 3;

	while (retries--) {
		// DO NOT hold lock during blocking WriteFile
		res = WriteFile((HANDLE)mmbi->device_handle, buf, (DWORD)len, &bytesWritten, NULL);
		if (res && bytesWritten == len) break;
		
		mctp_prerr("MMBI WriteFile failed, retrying... (%d left)", retries);
		Sleep(1);
	}

		if (!res || bytesWritten != len) {
			mctp_prerr("MMBI WriteFile failed permanently: last_err=%d", GetLastError());
			return -EIO;
		}
		return 0;
	}
#endif

	if (!mmbi->tx_storage)
		return -1;
	
	if (len > mmbi->memory_size) {
		mctp_prerr("Packet too large for MMBI: %zu > %zu", len, mmbi->memory_size);
		return -EMSGSIZE;
	}

	/* Copy packet data to the TX memory mapped region */
	memcpy(mmbi->tx_storage, buf, len);

	return 0;
}

// ... start function ...

static int mctp_mmbi_start(struct mctp_binding *b)
{
	mctp_binding_set_tx_enabled(b, true);
	return 0;
}

int mctp_mmbi_init_mem(struct mctp_binding_mmbi *mmbi, 
		       void *tx_addr, void *rx_addr, size_t size)
{
	if (!mmbi)
		return -EINVAL;

	mmbi->tx_storage = tx_addr;
	mmbi->rx_storage = rx_addr;
	mmbi->memory_size = size;
	mmbi->binding.tx = mctp_mmbi_tx;
	mmbi->binding.start = mctp_mmbi_start;
	
	mmbi->binding.tx_storage = __mctp_alloc(MCTP_PKTBUF_SIZE(65536));
    if (!mmbi->binding.tx_storage)
        return -ENOMEM;

	return 0;
}

int mctp_mmbi_init_device(struct mctp_binding_mmbi *mmbi, const char *device_path)
{
#ifdef _WIN32
	HANDLE hDevice = CreateFileA(device_path, 
								GENERIC_READ | GENERIC_WRITE,
								0, // No sharing for now? Or FILE_SHARE_READ | FILE_SHARE_WRITE
								NULL,
								OPEN_EXISTING,
								0, // FILE_ATTRIBUTE_NORMAL,
								NULL);
	
	if (hDevice == INVALID_HANDLE_VALUE) {
		mctp_prerr("Failed to open MMBI device %s: %d", device_path, GetLastError());
		return -1;
	}

	mmbi->device_handle = (void*)hDevice;

#ifdef _WIN32
    if (GetFileType(hDevice) == FILE_TYPE_PIPE) {
        DWORD mode = PIPE_READMODE_MESSAGE;
        SetNamedPipeHandleState(hDevice, &mode, NULL, NULL);
    }
#endif

	mmbi->binding.tx = mctp_mmbi_tx;
	mmbi->binding.start = mctp_mmbi_start;

	mmbi->binding.tx_storage = __mctp_alloc(MCTP_PKTBUF_SIZE(65536));
    if (!mmbi->binding.tx_storage)
        return -ENOMEM;

	return 0;
#else
	(void)mmbi; (void)device_path;
	return -ENOSYS;
#endif
}

int mctp_mmbi_poll(struct mctp_binding_mmbi *mmbi)
{
#ifdef _WIN32
	if (mmbi->device_handle && mmbi->device_handle != INVALID_HANDLE_VALUE) {
		// Fixed buffer to support full MTU (64KB) plus header
		static uint8_t rx_buf[65536 + 10];
		DWORD bytesRead = 0;
		DWORD bytesAvail = 0;

		if (!PeekNamedPipe((HANDLE)mmbi->device_handle, NULL, 0, NULL, &bytesAvail, NULL)) {
			bytesAvail = 1; 
		}
		
		if (bytesAvail == 0) {
            Sleep(1);
			return 0;
		}

		// Read exactly one packet at a time to prevent merging in simulation
		DWORD max_to_read = (DWORD)mmbi->binding.pkt_size + 4;
		if (max_to_read > sizeof(rx_buf)) max_to_read = sizeof(rx_buf);

		BOOL res = ReadFile((HANDLE)mmbi->device_handle, rx_buf, max_to_read, &bytesRead, NULL);
		
		if (res && bytesRead > 0) {
#ifdef _WIN32
			EnterCriticalSection(&mmbi->lock);
#endif
			struct mctp_pktbuf *pkt = mctp_pktbuf_alloc(&mmbi->binding, bytesRead);
			if (!pkt) {
#ifdef _WIN32
				LeaveCriticalSection(&mmbi->lock);
#endif
				mctp_prerr("MMBI: Failed to allocate pktbuf for %u bytes", bytesRead);
				return -ENOMEM;
			}
			
            memcpy(mctp_pktbuf_hdr(pkt), rx_buf, bytesRead);
            mctp_bus_rx(&mmbi->binding, pkt);
#ifdef _WIN32
			LeaveCriticalSection(&mmbi->lock);
#endif
            return 0;
		} else if (!res) {
			DWORD err = GetLastError();
			if (err == ERROR_BROKEN_PIPE) return -EPIPE;
			if (err != ERROR_IO_PENDING && err != ERROR_NO_DATA) {
				mctp_prerr("MMBI ReadFile error: %u", err);
			}
		}
		return 0; 
	}
#endif
	return 0;
}

int mctp_mmbi_rx(struct mctp_binding_mmbi *mmbi, size_t len)
{
	// Legacy memory-map RX logic
	// ... (keep as is or adapt)
	struct mctp_pktbuf *pkt;

	if (!mmbi || !mmbi->rx_storage)
		return -EINVAL;

	if (len > mmbi->memory_size)
		return -EMSGSIZE;

	pkt = mctp_pktbuf_alloc(&mmbi->binding, len);
	if (!pkt)
		return -ENOMEM;

	if (mctp_pktbuf_push(pkt, mmbi->rx_storage, len) != 0) {
		mctp_pktbuf_free(pkt);
		return -1;
	}

	mctp_bus_rx(&mmbi->binding, pkt);

	return 0;
}

/* -------------------------------------------------------------------------- */
/* High-Level User API Implementation                                         */
/* -------------------------------------------------------------------------- */

struct mctp_mmbi_context {
	struct mctp *mctp;
	struct mctp_binding_mmbi *mmbi;
	mctp_mmbi_rx_cb rx_cb;
	void *user_ctx;
	bool debug;
};

#define MMBI_DBG(ctx, fmt, ...) do { \
	if (ctx && ctx->debug) { \
		fprintf(stderr, "[MMBI DBG] " fmt "\n", ##__VA_ARGS__); \
	} \
} while(0)

static void mmbi_internal_rx(uint8_t eid, bool tag_owner, uint8_t msg_tag,
			    void *data, void *msg, size_t len)
{
	struct mctp_mmbi_context *ctx = (struct mctp_mmbi_context *)data;
	
	MMBI_DBG(ctx, "RX: SourceEID=%d TagOwner=%d MsgTag=%d Len=%zu", eid, tag_owner, msg_tag, len);
	
	if (ctx && ctx->rx_cb) {
		ctx->rx_cb(eid, msg, len, ctx->user_ctx);
	}
}

mctp_mmbi_context_t* mctp_mmbi_context_init(const char *device_path, uint8_t local_eid)
{
	mctp_mmbi_context_t *ctx = __mctp_alloc(sizeof(mctp_mmbi_context_t));
	if (!ctx) return NULL;
	memset(ctx, 0, sizeof(*ctx));
	// debug is false by default

	ctx->mctp = mctp_init();
	if (!ctx->mctp) {
		__mctp_free(ctx);
		return NULL;
	}

	ctx->mmbi = mctp_mmbi_init();
	if (!ctx->mmbi) {
		mctp_destroy(ctx->mctp);
		__mctp_free(ctx);
		return NULL;
	}

	// Retry loop for device open (Mock Driver might be slow to pipe)
	int retries = 5;
	while (retries--) {
		if (mctp_mmbi_init_device(ctx->mmbi, device_path) == 0) break;
		#ifdef _WIN32
		Sleep(100);
		#endif
	}

	if (!ctx->mmbi->device_handle) { // Check if handle set
		mctp_mmbi_destroy(ctx->mmbi);
		mctp_destroy(ctx->mctp);
		__mctp_free(ctx);
		return NULL;
	}

	mctp_register_bus(ctx->mctp, &ctx->mmbi->binding, local_eid);
	mctp_binding_set_tx_enabled(&ctx->mmbi->binding, true);
	
	/* Route all RX to our specific handler */
	mctp_set_rx_all(ctx->mctp, mmbi_internal_rx, ctx);

	return ctx;
}

void mctp_mmbi_context_destroy(mctp_mmbi_context_t *ctx)
{
	if (!ctx) return;
	MMBI_DBG(ctx, "Destroying context");
	
	if (ctx->mctp && ctx->mmbi) {
		mctp_unregister_bus(ctx->mctp, &ctx->mmbi->binding);
	}
	if (ctx->mmbi) mctp_mmbi_destroy(ctx->mmbi);
	if (ctx->mctp) mctp_destroy(ctx->mctp);
	
	__mctp_free(ctx);
}

int mctp_mmbi_send(mctp_mmbi_context_t *ctx, uint8_t dst_eid, const void *data, size_t len)
{
	if (!ctx || !ctx->mctp) return -1;
	MMBI_DBG(ctx, "TX: DestinationEID=%d Len=%zu", dst_eid, len);
#ifdef _WIN32
    EnterCriticalSection(&ctx->mmbi->lock);
#endif
	int rc = mctp_message_tx(ctx->mctp, dst_eid, false, 0, (void *)data, len);
#ifdef _WIN32
    LeaveCriticalSection(&ctx->mmbi->lock);
#endif
    return rc;
}

void mctp_mmbi_set_rx_callback(mctp_mmbi_context_t *ctx, mctp_mmbi_rx_cb cb, void *user_context)
{
	if (ctx) {
		ctx->rx_cb = cb;
		ctx->user_ctx = user_context;
	}
}

void mctp_mmbi_set_debug(mctp_mmbi_context_t *ctx, bool enable)
{
	if (ctx) {
		ctx->debug = enable;
		if (enable) { 
			fprintf(stderr, "[MMBI DBG] Debug enabled\n");
		}
	}
}

int mctp_mmbi_context_poll(mctp_mmbi_context_t *ctx)
{
	if (!ctx || !ctx->mmbi) return -1;
	// MMBI_DBG(ctx, "Poll..."); // Too noisy for poll
	return mctp_mmbi_poll(ctx->mmbi);
}
