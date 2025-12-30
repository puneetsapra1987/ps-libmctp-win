/* Host Application using MMBI User-Defined API */
#include "libmctp-mmbi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define HOST_DEVICE "\\\\.\\pipe\\MMBI0" // Use Mock Pipe
#define HOST_EID 8
#define BMC_EID 9

void host_rx_cb(uint8_t src_eid, void *data, size_t len, void *user_ctx) {
    (void)user_ctx;
    printf("[HOST] RX from %d: Length=%zu\n", src_eid, len);
    char *msg = (char*)data;
    if (len > 0) {
        printf("[HOST] Message Content: %.20s...\n", msg);
    }
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        printf("Usage: host_transport (No arguments supported in automation)\n");
        return 0;
    }
    printf("=== Host App (MMBI User API) ===\n");
    
    // 1. Initialize Context
    mctp_mmbi_context_t *ctx = mctp_mmbi_context_init(HOST_DEVICE, HOST_EID);
    if (!ctx) {
        printf("[HOST] Transport %s not available. Skipping.\n", HOST_DEVICE);
        return 0;
    }
    
    // Enable Debug Logging
    mctp_mmbi_set_debug(ctx, true);
    
    // 2. Set Callback
    mctp_mmbi_set_rx_callback(ctx, host_rx_cb, NULL);
    
    // 3. Send Message
    const char *msg = "Hello BMC from Host MMBI API!";
    printf("[HOST] Sending: %s\n", msg);
    mctp_mmbi_send(ctx, BMC_EID, msg, strlen(msg));
    
    // 4. Poll Loop
    printf("[HOST] Polling for response (Ctrl+C to stop)...\n");
    while (1) {
        mctp_mmbi_context_poll(ctx);
        Sleep(10);
    }
    
    mctp_mmbi_context_destroy(ctx);
    return 0;
}
