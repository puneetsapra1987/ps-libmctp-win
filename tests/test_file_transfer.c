/* SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later */

#define _GNU_SOURCE

#include "libmctp-mmbi.h"
#include "libmctp-log.h"
#include "test-utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

// Host EID: 8, BMC EID: 9
#define HOST_EID 8
#define BMC_EID 9
#define TRANSFER_SIZE (25 * 1024 * 1024)

/* Global state for receiving */
static uint8_t *rx_buf = NULL;
static size_t rx_len = 0;
static bool rx_complete = false;

static void rx_message(uint8_t eid, bool tag_owner, uint8_t msg_tag,
		       void *data, void *msg, size_t len)
{
	printf("RX: Received message from EID %d, Length: %zu\n", eid, len);
	
	if (len == TRANSFER_SIZE) {
		printf("RX: Size matches expected 25MB.\n");
		// Verify first/last bytes or checksum
		uint8_t *p = (uint8_t *)msg;
		if (p[0] == 0xAA && p[len-1] == 0xBB) {
			printf("RX: Content verification passed (simple).\n");
		} else {
			printf("RX: Content verification FAILED. Expected 0xAA...0xBB, got 0x%02x...0x%02x\n", p[0], p[len-1]);
		}
		rx_complete = true;
	} else {
		printf("RX: Size mismatch. Expected %d, got %zu\n", TRANSFER_SIZE, len);
	}
}

int main(int argc, char *argv[])
{
	struct mctp *mctp;
	struct mctp_binding_mmbi *mmbi;
	int rc;
	const char *dev = NULL;
	int eid, other_eid;
	bool is_sender = false;
	
	if (argc < 3) {
		fprintf(stderr, "Usage: %s [host|bmc] [send|recv]\n", argv[0]);
		return 1;
	}

	if (strcmp(argv[1], "host") == 0) {
		dev = "\\\\.\\MMBI0";
		eid = HOST_EID;
		other_eid = BMC_EID;
	} else if (strcmp(argv[1], "bmc") == 0) {
		dev = "\\\\.\\MMBI1";
		eid = BMC_EID;
		other_eid = HOST_EID;
	} else {
		fprintf(stderr, "Invalid role. Use 'host' or 'bmc'.\n");
		return 1;
	}

	if (strcmp(argv[2], "send") == 0) {
		is_sender = true;
	}

	printf("%s: Initializing on device %s (EID %d)\n", argv[1], dev, eid);

	mctp = mctp_init();
	// Must ensure max_message_size is large enough or set via API if available (simulated via compile option)
	
	mmbi = mctp_mmbi_init();
	rc = mctp_mmbi_init_device(mmbi, dev);
	if (rc != 0) {
		fprintf(stderr, "Failed to init device: %d\n", rc);
		return 1;
	}

	mctp_register_bus(mctp, &mmbi->binding, eid);
	mctp_set_rx_all(mctp, rx_message, NULL);
    mctp_binding_set_tx_enabled(&mmbi->binding, true);

	if (is_sender) {
		printf("SENDER: Allocating 25MB buffer...\n");
		uint8_t *tx_buf = malloc(TRANSFER_SIZE);
		if (!tx_buf) {
			fprintf(stderr, "Failed to allocate 25MB\n");
			return 1;
		}
		
		// Fill pattern
		memset(tx_buf, 0xCC, TRANSFER_SIZE);
		tx_buf[0] = 0xAA;
		tx_buf[TRANSFER_SIZE-1] = 0xBB;

		printf("SENDER: Sending 25MB message to EID %d...\n", other_eid);
		
		uint8_t tag = 0;
		rc = mctp_message_tx(mctp, other_eid, false, tag, tx_buf, TRANSFER_SIZE);
		
		if (rc == 0) {
			printf("SENDER: Message submitted to core. Transmission driven by poll/core.\n");
		} else {
			printf("SENDER: Failed to send message: %d\n", rc);
		}
		
		// In a real loop we might need to pump if the core relied on callbacks, 
		// but standard libmctp tx is synchronous if binding tx is synchronous.
		// Our binding tx is blocking-ish (WriteFile).
		
		free(tx_buf);
	} else {
		printf("RECEIVER: Waiting for 25MB message...\n");
		
		time_t start = time(NULL);
		while (!rx_complete && (time(NULL) - start < 30)) { // 30s timeout
			mctp_mmbi_poll(mmbi);
            #ifdef _WIN32
			Sleep(10);
            #endif
		}
		
		if (rx_complete) {
			printf("RECEIVER: Transfer complete!\n");
		} else {
			printf("RECEIVER: Timed out.\n");
		}
	}
	
	mctp_unregister_bus(mctp, &mmbi->binding);
	mctp_mmbi_destroy(mmbi);
	mctp_destroy(mctp);

	return 0;
}
