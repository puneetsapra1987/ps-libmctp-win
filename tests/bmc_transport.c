/* BMC Application using MMBI User-Defined API */
#include "libmctp-mmbi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define BMC_DEVICE "\\\\.\\pipe\\MMBI1" // Use Mock Pipe
#define BMC_EID 9
#define HOST_EID 8

void bmc_rx_cb(uint8_t src_eid, void *data, size_t len, void *user_ctx) {
    mctp_mmbi_context_t *ctx = (mctp_mmbi_context_t*)user_ctx;
    
    printf("[BMC] RX from %d: Length=%zu\n", src_eid, len);
    char *msg = (char*)data;
    if (len > 0) {
        printf("[BMC] Message Content: %.20s...\n", msg);
    }
    
    // Echo back / Vice Versa communication
    const char *reply = "Hello Host, this is BMC replying via MMBI API!";
    printf("[BMC] Sending Reply: %s\n", reply);
    mctp_mmbi_send(ctx, HOST_EID, reply, strlen(reply));
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        printf("Usage: bmc_transport (No arguments supported in automation)\n");
        return 0;
    }
    printf("=== BMC App (MMBI User API) ===\n");
    
    // 1. Initialize Context
    mctp_mmbi_context_t *ctx = mctp_mmbi_context_init(BMC_DEVICE, BMC_EID);
    if (!ctx) {
        printf("[BMC] Transport %s not available. Skipping.\n", BMC_DEVICE);
        return 0;
    }
    
    // 2. Set Callback
    mctp_mmbi_set_rx_callback(ctx, bmc_rx_cb, ctx);
    
    // 3. Poll Loop
    printf("[BMC] Listening... (Ctrl+C to stop)\n");
    while (1) {
        mctp_mmbi_context_poll(ctx);
        Sleep(10);
    }
    
    mctp_mmbi_context_destroy(ctx);
    return 0;
}
