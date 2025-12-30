/* SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later */

#define _GNU_SOURCE

#include "libmctp-mmbi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#endif

// Minimal Mock Driver Logic (Server End of Pipe)
HANDLE hPipe;
DWORD WINAPI mock_pipe_server(LPVOID param) {
    (void)param;
    // Create ONE instance for MMBI0 test
    hPipe = CreateNamedPipeA("\\\\.\\pipe\\MMBI_LOG_TEST", 
        PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 
        1, 1024, 1024, 0, NULL);
        
    if (hPipe == INVALID_HANDLE_VALUE) return 1;

    // Wait for connection
    ConnectNamedPipe(hPipe, NULL);
    
    // Just discard read data
    uint8_t buf[1024];
    DWORD bytesRead;
    while (ReadFile(hPipe, buf, sizeof(buf), &bytesRead, NULL)) {
        // Drain
    }
    
    CloseHandle(hPipe);
    return 0;
}

int main(void) {
    printf("=== Test: MMBI Logging ===\n");

    // 1. Start Mock Server
    HANDLE hThread = CreateThread(NULL, 0, mock_pipe_server, NULL, 0, NULL);
    Sleep(500); // Wait for pipe

    // 2. Init Client
    mctp_mmbi_context_t *ctx = mctp_mmbi_context_init("\\\\.\\pipe\\MMBI_LOG_TEST", 8);
    if (!ctx) {
        fprintf(stderr, "FAIL: Init failed\n");
        return 1;
    }

    const char *data = "Ping";

    // 3. Test Debug ENABLED
    printf("--- Phase 1: Debug ENABLED (Expect Logs below) ---\n");
    mctp_mmbi_set_debug(ctx, true);
    mctp_mmbi_send(ctx, 9, data, 4);
    
    // 4. Test Debug DISABLED
    printf("--- Phase 2: Debug DISABLED (Expect NO Logs below) ---\n");
    mctp_mmbi_set_debug(ctx, false);
    mctp_mmbi_send(ctx, 9, data, 4);
    
    printf("--- Test Complete ---\n");

    mctp_mmbi_context_destroy(ctx);
    
    // Cleanup Thread
    TerminateThread(hThread, 0); // Force kill mock
    CloseHandle(hThread);

    return 0;
}
