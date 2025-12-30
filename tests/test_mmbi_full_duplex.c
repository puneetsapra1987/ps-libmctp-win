/* SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif

#include "libmctp.h"
#include "libmctp-mmbi.h"
#include "test-utils.h"

#define HOST_EID 8
#define BMC_EID 9
#define LARGE_MSG_SIZE 4096
#define SMALL_MSG_SIZE 128
#define TEST_MTU 1024

static volatile bool host_rx_done = false;
static volatile bool bmc_rx_done = false;
static volatile bool thread_exit = false;
static CRITICAL_SECTION mctp_lock;

struct poll_args {
    struct mctp_binding_mmbi *mmbi;
};

struct relay_ctx {
    HANDLE hIn;
    HANDLE hOut;
    const char *name;
};

static void rx_message(uint8_t eid, bool tag_owner, uint8_t msg_tag,
		       void *data, void *msg, size_t len)
{
	const char *role = (const char *)data;
    printf("[%s] RX: %zu bytes from %d (tag %d)\n", role, len, eid, msg_tag);
    if (len == LARGE_MSG_SIZE) {
        uint8_t *p = (uint8_t *)msg;
        if (p[0] == 0xAA && p[len-1] == 0xBB) {
            if (strcmp(role, "HOST") == 0) host_rx_done = true;
            else bmc_rx_done = true;
            printf("[%s] Received ALL fragments successfully!\n", role);
        }
    }
}

DWORD WINAPI poll_thread(LPVOID param) {
    struct poll_args *args = (struct poll_args *)param;
    while (!thread_exit) {
        EnterCriticalSection(&mctp_lock);
        int rc = mctp_mmbi_poll(args->mmbi);
        LeaveCriticalSection(&mctp_lock);
        if (rc == -EPIPE) break;
        Sleep(1);
    }
    return 0;
}

DWORD WINAPI relay_worker(LPVOID param) {
    struct relay_ctx *ctx = (struct relay_ctx *)param;
    uint8_t buf[65536];
    DWORD br, bw;
    
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    ConnectNamedPipe(ctx->hIn, NULL);
    
    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(ctx->hIn, &mode, NULL, NULL);

    while (1) {
        if (ReadFile(ctx->hIn, buf, sizeof(buf), &br, NULL)) {
            if (br > 0) {
                if (!WriteFile(ctx->hOut, buf, br, &bw, NULL)) {
                    printf("[%s] WriteFile FAILED: %d\n", ctx->name, GetLastError());
                }
            }
        } else {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE) {
                // printf("[%s] Pipe broken\n", ctx->name);
                break;
            }
            if (err != ERROR_NO_DATA) {
                printf("[%s] ReadFile FAILED: %d\n", ctx->name, err);
            }
            Sleep(1);
        }
    }
    return 0;
}

static HANDLE spawn_child(const char *prog, const char *role) {
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    char cmd[MAX_PATH + 32];
    sprintf(cmd, "\"%s\" %s", prog, role);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        return NULL;
    }
    CloseHandle(pi.hThread);
    return pi.hProcess;
}

int run_endpoint(const char *role, bool is_host) {
    struct mctp *mctp;
    struct mctp_binding_mmbi *mmbi;
    const char *pipe_name = is_host ? "\\\\.\\pipe\\FD_MMBI0" : "\\\\.\\pipe\\FD_MMBI1";
    uint8_t target_eid = is_host ? BMC_EID : HOST_EID;
    InitializeCriticalSection(&mctp_lock);

    mctp = mctp_init();
    mmbi = mctp_mmbi_init();
    mmbi->binding.pkt_size = TEST_MTU;
    // mctp_set_log_stdio(MCTP_LOG_DEBUG);
    
    int tries = 30;
    while (tries--) {
        if (mctp_mmbi_init_device(mmbi, pipe_name) == 0) break;
        Sleep(100);
    }
    if (tries < 0) return -1;

    mctp_register_bus(mctp, &mmbi->binding, is_host ? HOST_EID : BMC_EID);
    mctp_set_rx_all(mctp, rx_message, (void*)role);

    struct poll_args *p_args = malloc(sizeof(struct poll_args));
    p_args->mmbi = mmbi;
    HANDLE hThread = CreateThread(NULL, 0, poll_thread, p_args, 0, NULL);

    uint8_t *buf = malloc(LARGE_MSG_SIZE);
    memset(buf, 0, LARGE_MSG_SIZE);
    buf[0] = 0xAA;
    buf[LARGE_MSG_SIZE-1] = 0xBB;

    Sleep(1000); // Wait for both sides to be ready
    printf("[%s] Sending %d bytes to %d...\n", role, LARGE_MSG_SIZE, target_eid);
    EnterCriticalSection(&mctp_lock);
    int rc = mctp_message_tx(mctp, target_eid, true, is_host ? 1 : 2, buf, LARGE_MSG_SIZE);
    LeaveCriticalSection(&mctp_lock);
    if (rc < 0) printf("[%s] TX FAILED: %d\n", role, rc);

    time_t start = time(NULL);
    bool i_am_done = false;
    while (time(NULL) - start < 180) {
        if ((is_host && host_rx_done) || (!is_host && bmc_rx_done)) {
            i_am_done = true;
            break;
        }
        Sleep(10);
    }

    printf("[%s] Test finished, i_am_done=%d\n", role, i_am_done);

    thread_exit = true;
    WaitForSingleObject(hThread, 2000);
    CloseHandle(hThread);
    free(p_args);

    free(buf);
    mctp_mmbi_destroy(mmbi);
    mctp_destroy(mctp);

    if (i_am_done) Sleep(2000);
    return i_am_done ? 0 : 1;
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        return run_endpoint(argv[1], strcmp(argv[1], "HOST") == 0);
    }

    printf("=== Full-Duplex Simultaneous Stress Test ===\n");
    
    HANDLE hSrv0 = CreateNamedPipeA("\\\\.\\pipe\\FD_MMBI0", 
                            PIPE_ACCESS_DUPLEX,
                            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                            1, 1024*128, 1024*128, 0, NULL);
    HANDLE hSrv1 = CreateNamedPipeA("\\\\.\\pipe\\FD_MMBI1", 
                            PIPE_ACCESS_DUPLEX,
                            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                            1, 1024*128, 1024*128, 0, NULL);

    if (hSrv0 == INVALID_HANDLE_VALUE || hSrv1 == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Failed to create named pipes (%d)\n", GetLastError());
        return 1;
    }

    char prog[MAX_PATH];
    GetModuleFileNameA(NULL, prog, sizeof(prog));

    struct relay_ctx ctx0 = { hSrv0, hSrv1, "0->1" };
    struct relay_ctx ctx1 = { hSrv1, hSrv0, "1->0" };
    
    HANDLE hThreads[2];
    hThreads[0] = CreateThread(NULL, 0, relay_worker, &ctx0, 0, NULL);
    hThreads[1] = CreateThread(NULL, 0, relay_worker, &ctx1, 0, NULL);

    HANDLE hPs[2];
    hPs[0] = spawn_child(prog, "HOST");
    hPs[1] = spawn_child(prog, "BMC");
    
    if (!hPs[0] || !hPs[1]) {
        fprintf(stderr, "Failed to spawn child processes\n");
        return 1;
    }

    DWORD wr = WaitForMultipleObjects(2, hPs, TRUE, 200000);
    
    DWORD code0, code1;
    GetExitCodeProcess(hPs[0], &code0);
    GetExitCodeProcess(hPs[1], &code1);

    CloseHandle(hPs[0]);
    CloseHandle(hPs[1]);

    if (wr == WAIT_OBJECT_0 && code0 == 0 && code1 == 0) {
        printf("=== PASS: Full-Duplex Simultaneous Transfer Successful! ===\n");
        return 0;
    } else {
        printf("=== FAIL: Full-Duplex Failed (Exit Codes: %lu, %lu) ===\n", code0, code1);
        return 1;
    }
}
