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

static void rx_message(uint8_t eid, bool tag_owner, uint8_t msg_tag,
		       void *data, void *msg, size_t len)
{
	printf("HOST: Received message from EID %d\n", eid);
	printf("HOST: Message content: ");
	uint8_t *p = (uint8_t *)msg;
	for (size_t i = 0; i < len; i++) {
		printf("%02x ", p[i]);
	}
	printf("\n");
}

int main(int argc, char *argv[])
{
	struct mctp *mctp;
	struct mctp_binding_mmbi *mmbi;
	int rc;
	const char *dev = "\\\\.\\MMBI0"; // Default to MMBI0

	if (argc > 1) dev = argv[1];

	printf("HOST: Initializing on device %s\n", dev);

	mctp = mctp_init();
	mmbi = mctp_mmbi_init();
	
	rc = mctp_mmbi_init_device(mmbi, dev);
	if (rc != 0) {
		printf("HOST: Hardware device %s not found. Skipping test (Simulation Mode).\n", dev);
		return 0; 
	}

	rc = mctp_register_bus(mctp, &mmbi->binding, HOST_EID);
	if (rc != 0) {
        fprintf(stderr, "HOST: Failed to register bus\n");
        return 1;
    }

	mctp_set_rx_all(mctp, rx_message, NULL);
    mctp_binding_set_tx_enabled(&mmbi->binding, true);

	printf("HOST: Sending 'PING' to BMC (EID %d)...\n", BMC_EID);
	
	uint8_t payload[] = { 0xAA, 0xBB, 0xCC, 0xDD }; // Dummy payload
	rc = mctp_message_tx(mctp, BMC_EID, false, 0, payload, sizeof(payload));
	if (rc == 0) {
		printf("HOST: Message sent successfully.\n");
	} else {
		printf("HOST: Failed to send message: %d\n", rc);
	}

	// Poll loop to wait for response (if any)
	// In a real app we might use overlapped IO or threads
	printf("HOST: Polling for response...\n");
	for (int i = 0; i < 10; i++) {
		mctp_mmbi_poll(mmbi);
		Sleep(100); 
	}
	
	mctp_unregister_bus(mctp, &mmbi->binding);
	mctp_mmbi_destroy(mmbi);
	mctp_destroy(mctp);

	return 0;
}
