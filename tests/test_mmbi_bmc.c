/* SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later */

#define _GNU_SOURCE

#include "libmctp-mmbi.h"
#include "libmctp-log.h"
#include "test-utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

// Host EID: 8, BMC EID: 9
#define HOST_EID 8
#define BMC_EID 9

static void rx_message(uint8_t eid, bool tag_owner, uint8_t msg_tag,
		       void *data, void *msg, size_t len)
{
	printf("BMC: Received message from EID %d\n", eid);
	printf("BMC: Message content: ");
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
	const char *dev = "\\\\.\\MMBI1"; // Default to MMBI1

	if (argc > 1) dev = argv[1];

	printf("BMC: Initializing on device %s\n", dev);

	mctp = mctp_init();
	mmbi = mctp_mmbi_init();
	
	rc = mctp_mmbi_init_device(mmbi, dev);
	if (rc != 0) {
		printf("BMC: Hardware device %s not found. Skipping test (Simulation Mode).\n", dev);
		return 0;
	}

	rc = mctp_register_bus(mctp, &mmbi->binding, BMC_EID);
    if (rc != 0) {
        fprintf(stderr, "BMC: Failed to register bus\n");
        return 1;
    }

	mctp_set_rx_all(mctp, rx_message, NULL);
    mctp_binding_set_tx_enabled(&mmbi->binding, true);

	printf("BMC: Listening for messages...\n");
	
	// Continuous poll loop
	while (1) {
		mctp_mmbi_poll(mmbi);
        // Reduce CPU usage slightly
        #ifdef _WIN32
		Sleep(10);
        #endif
	}
	
	mctp_unregister_bus(mctp, &mmbi->binding);
	mctp_mmbi_destroy(mmbi);
	mctp_destroy(mctp);

	return 0;
}
