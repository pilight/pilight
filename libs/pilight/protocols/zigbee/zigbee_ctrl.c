/*
 * zigbee_ctrl.c
 *
 *  Created on: Nov 10, 2016
 *      Author: ma-ca
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/pilight.h"
#include "../../core/common.h"
#include "../../core/dso.h"
#include "../../core/log.h"
#include "../../hardware/zigbee.h"
#include "../protocol.h"
#include "zigbee_ctrl.h"


static int createCode(struct JsonNode *code, char **message) {
	logprintf(LOG_DEBUG, "[ZigBee] zigbee_ctrl createCode code = %s\n", json_stringify(code, NULL));
	printf("createCode code = %s\n", json_stringify(code, NULL));
	int srcEp = 0, dstEp = 0, dobind = 0, dorep = 0, shortAddr = -1;
	int attrtype = -1, reportchange = -1;
	double itmp = 0.0;
	char *srcExtAddrIn = NULL, *dstExtAddrIn = NULL, *command = NULL;
	char *stmp = NULL;
	char clusterIn[7], attributeid[7];
	char sendCmd[200], attrid[5];
	char minimum[5], maximum[5];

	memset(attributeid, '\0', sizeof(attributeid));

	if ((json_find_number(code, "autodiscover", &itmp)) == 0) {
		logprintf(LOG_DEBUG, "[ZigBee] autodiscover all devices in network");
		printf("[ZigBee] autodiscover all devices in network\n");
		if(pilight.process == PROCESS_DAEMON) {
			if (json_find_string(code, "shortAddr", &stmp) == 0) {
				sscanf(stmp, "%x", &shortAddr);
			}
			if (json_find_number(code, "shortAddr", &itmp) == 0) {
				shortAddr = (int) itmp;
			}
			if (shortAddr == -1) {
				autoDiscoverDevices();
			} else {
				if ((json_find_number(code, "dstEp", &itmp)) == 0) {
					dstEp = (int) itmp;
				}
				discoverSingleDevice(shortAddr, dstEp);
			}
		}
		return EXIT_SUCCESS;
	}
	if ((json_find_string(code, "NWK_addr_req", &dstExtAddrIn)) == 0) {
		logprintf(LOG_DEBUG, "[ZigBee] NWK_addr_req get short address for %s", dstExtAddrIn);
		printf("[ZigBee] NWK_addr_req get short address for %s\n", dstExtAddrIn);
		if(pilight.process == PROCESS_DAEMON) {
			char dstExtAddrRev[17];
			memset(dstExtAddrRev, '\0', sizeof(dstExtAddrRev));
			reverseStr(dstExtAddrIn, dstExtAddrRev, 16);
			sprintf(sendCmd, "zdpcmd 0xFFFD 0x0000 %s0000", dstExtAddrRev);  // broadcast
			zigbeeSendCommand(sendCmd);
		}
		return EXIT_SUCCESS;
	}
	if ((json_find_number(code, "list", &itmp)) == 0) {
		logprintf(LOG_DEBUG, "[ZigBee] list lqi entries (neighbor table) in network");
		printf("[ZigBee] list lqi entries (neighbor table) in network\n");
		showDiscoveredDevices();
		return EXIT_SUCCESS;
	}
	if ((json_find_number(code, "getrep", &itmp)) == 0) {
		sprintf(clusterIn, "0x%04X", (int) itmp);
		dorep = 0x08; // read reporting configuration
	}
	if ((json_find_number(code, "setrep", &itmp)) == 0) {
		sprintf(clusterIn, "0x%04X", (int) itmp);
		dorep = 0x06; // configure reporting
	}
	if ((json_find_number(code, "attributeid", &itmp)) == 0) {
		sprintf(attributeid, "0x%04X", (int) itmp);
	}
	if ((json_find_number(code, "bind", &itmp)) == 0) {
		sprintf(clusterIn, "0x%04X", (int) itmp);
		dobind = 0x0021; // bind
	}
	if ((json_find_number(code, "unbind", &itmp)) == 0) {
		sprintf(clusterIn, "0x%04X", (int) itmp);
		dobind = 0x0022; // unbind
	}
	if (json_find_string(code, "shortAddr", &stmp) == 0) {
		sscanf(stmp, "%x", &shortAddr);
	}
	if (json_find_number(code, "shortAddr", &itmp) == 0) {
		shortAddr = (int) itmp;
	}
	if ((json_find_number(code, "dstEp", &itmp)) == 0) {
		dstEp = (int) itmp;
	}
	if (json_find_number(code, "listbind", &itmp) == 0) {
		if (shortAddr == -1) {
			printf("[ZigBee] missing or invalid shortAddr parameter\n");
			return EXIT_FAILURE;
		}
		printf("[ZigBee] show bindings on 0x%04X\n", shortAddr);
		if (pilight.process == PROCESS_DAEMON) {
			sprintf(sendCmd, "zdpcmd 0x%04X 0x0033 00", shortAddr);
			zigbeeSendCommand(sendCmd);
			logprintf(LOG_DEBUG, "[ZigBee] zigbee_ctrl finished send getbind command %s", sendCmd);
		}
		return EXIT_SUCCESS;
	}
	if (dobind != 0 ) {
		if (strlen(clusterIn) < 4) {
			printf("[ZigBee] invalid cluster parameter = %s\n", clusterIn);
			return EXIT_FAILURE;
		}
		if (shortAddr == -1) {
			printf("[ZigBee] missing or invalid shortAddr parameter\n");
			return EXIT_FAILURE;
		}
		if ((json_find_string(code, "srcExtAddr", &srcExtAddrIn)) != 0 || strlen(srcExtAddrIn) < 16) {
			printf("[ZigBee] missing or invalid srcExtAddr parameter\n");
			return EXIT_FAILURE;
		}
		if ((json_find_number(code, "srcEp", &itmp)) != 0) {
			printf("[ZigBee] missing srcEp parameter\n");
			return EXIT_FAILURE;
		}
		srcEp = (int) itmp;
		if ((json_find_string(code, "dstExtAddr", &dstExtAddrIn)) != 0 || strlen(dstExtAddrIn) < 16) {
			printf("[ZigBee] missing or invalid dstExtAddr parameter\n");
			return EXIT_FAILURE;
		}
		if (dstEp == 0) {
			printf("[ZigBee] missing dstEp parameter\n");
			return EXIT_FAILURE;
		}

		sendBindCmd(shortAddr, srcEp, dstEp, dobind, clusterIn, srcExtAddrIn, dstExtAddrIn, 0);

		return EXIT_SUCCESS;

	}
	if (dorep == 0x08) {  // read reporting configuration
		if (shortAddr == -1) {
			printf("[ZigBee] missing or invalid shortAddr parameter\n");
			return EXIT_FAILURE;
		}
		if (strlen(attributeid) == 0) {
			printf("[ZigBee] missing attributeid parameter\n");
			return EXIT_FAILURE;
		}
		printf("[ZigBee] read reporting: 0x%04X %d %s %s\n", shortAddr, dstEp, clusterIn, attributeid);
		reverseStr(attributeid, attrid, 4);
		if (pilight.process == PROCESS_DAEMON) {
			// 08 = read reporting configuration command, 00 = direction, attribute id
			sprintf(sendCmd, "zclattr 0x%04X %d %s 0800%s", shortAddr, dstEp, clusterIn, attrid);
			zigbeeSendCommand(sendCmd);
			logprintf(LOG_DEBUG, "[ZigBee] read reporting sendCmd = %s", sendCmd);
		}
		return EXIT_SUCCESS;
	}
	if (dorep == 0x06) {  // configure reporting
		if (shortAddr == -1) {
			printf("[ZigBee] missing or invalid shortAddr parameter\n");
			return EXIT_FAILURE;
		}
		if (strlen(attributeid) == 0) {
			printf("[ZigBee] missing attributeid parameter\n");
			return EXIT_FAILURE;
		}
		if ((json_find_number(code, "attrtype", &itmp)) != 0) {
			printf("[ZigBee] missing attribute_type parameter\n");
			return EXIT_FAILURE;
		}
		attrtype = (int) itmp;
		if ((json_find_number(code, "minimum", &itmp)) != 0) {
			printf("[ZigBee] missing minimum parameter\n");
			return EXIT_FAILURE;
		}
		sprintf(minimum, "%04X", (int) itmp);
		if ((json_find_number(code, "maximum", &itmp)) != 0) {
			printf("[ZigBee] missing minimum parameter\n");
			return EXIT_FAILURE;
		}
		sprintf(maximum, "%04X", (int) itmp);
		if (json_find_number(code, "reportablechange", &itmp) == 0) {
			reportchange = (int) itmp;
		}

		sendRepoCmd(shortAddr, dstEp, clusterIn, attributeid, minimum, maximum, attrtype, reportchange);

		return EXIT_SUCCESS;
	}
	if ((json_find_number(code, "discoverattributes", &itmp)) == 0) {
		sprintf(clusterIn, "0x%04X", (int) itmp);
		if (shortAddr == -1) {
			printf("[ZigBee] missing or invalid shortAddr parameter\n");
			return EXIT_FAILURE;
		}
		if (dstEp == 0) {
			printf("[ZigBee] missing dstEp parameter\n");
			return EXIT_FAILURE;
		}
		printf("[ZigBee] discoverattributes on 0x%04X %d %s\n", shortAddr, dstEp, clusterIn);
		sprintf(sendCmd, "zclattr 0x%04X %d %s 0C00000D", shortAddr, dstEp, clusterIn);
		if (pilight.process == PROCESS_DAEMON) {
			zigbeeSendCommand(sendCmd);
			logprintf(LOG_DEBUG, "[ZigBee] configure reporting sendCmd = %s", sendCmd);
		}
		return EXIT_SUCCESS;
	}
	if ((json_find_number(code, "identify", &itmp)) == 0) {
		if (shortAddr == -1) {
			printf("[ZigBee] missing or invalid shortAddr parameter\n");
			return EXIT_FAILURE;
		}
		if (dstEp == 0) {
			printf("[ZigBee] missing dstEp parameter\n");
			return EXIT_FAILURE;
		}
		sendIdentifyCmd(shortAddr, dstEp, 5);   // identify 5 seconds
		return EXIT_SUCCESS;
	}
	if ((json_find_number(code, "permitjoin", &itmp)) == 0) {
		logprintf(LOG_DEBUG, "[ZigBee] permitjoin devices to network");
		printf("[ZigBee] permitjoin devices to network\n");
		if(pilight.process == PROCESS_DAEMON) {
			sprintf(sendCmd, "p 0xFFFF");
			zigbeeSendCommand(sendCmd);
			logprintf(LOG_DEBUG, "[ZigBee] permitjoin command = %s", sendCmd);
		}
		return EXIT_SUCCESS;
	}
	if ((json_find_string(code, "command", &command)) != 0) {
		logprintf(LOG_ERR, "[ZigBee] missing command parameter");
		printf("[ZigBee] missing command parameter\n");
		return EXIT_FAILURE;
	}

	if (pilight.process == PROCESS_DAEMON) {
		zigbeeSendCommand(command);
		logprintf(LOG_DEBUG, "[ZigBee] zigbee_ctrl finished send command %s", command);
	} else {
		printf("[ZigBee] send command finished %s\n", command);
	}

	return EXIT_SUCCESS;
}

static void printHelp(void) {
	printf("\t -c --command\t\tsend zigbee command directly. Use -c help for list of commands\n");
	printf("\t -j --permitjoin\topen ZigBee network and permit joining for 3 minutes\n");
	printf("\t -k --identify\t\tidentify device (requires -s <shortaddr> -e <endpoint>) \n");
	printf("\t -a --autodiscover\tget all devices on network\n");
	printf("\t\t (optional on one device) -s <shortaddr> -e <endpoint> \n");
	printf("\t -l --list\t\tlist lqi entries (neighbor table)\n");
	printf("\t -f --discoverattributes <cluster_id> (requires -s <shortaddr> -e <endpoint>) \n");
	printf("\t -r --setrep <cluster_id> \tconfigure reporting on target device\n");
	printf("\t\t -t --attributeid <attribute_id> -w attrtype <attribute_type> -s <shortaddr> -e <endpoint>\n");
	printf("\t\t -m --minimum_reporting_interval <minimum>  -n --maximum_reporting_interval <maximum>\n");
	printf("\t\t -o --reportablechange <change_value>\n");
	printf("\t -g --getrep <cluster_id> \tget reporting configuration on target device\n");
	printf("\t\t -t --attributeid <attribute_id> -s <shortaddr> -e <endpoint>\n");
	printf("\t -b|-u --bind|unbind <clusterid> -s <shortaddr> -x <source_extaddr> -y <source_ep> \n");
	printf("\t\t-d <destination_extaddr> -e <destination_ep>\tset binding for cluster id on target endpoint to destination\n");
	printf("\t -v --listbind\t -s <shortaddr> \tlist bindings on target \n");
	printf("\t -q --NWK_addr_req\t <extaddr> \tget device NWK short address \n");
}

#if !defined(MODULE) && !defined(_WIN32)
__attribute__((weak))
#endif
void zigbeeCtrlInit(void) {

	protocol_register(&zigbee_ctrl);
	protocol_set_id(zigbee_ctrl, "zigbee_ctrl");
	protocol_device_add(zigbee_ctrl, "zigbee_ctrl", "ZigBee Controller");
	zigbee_ctrl->devtype = SWITCH;
	zigbee_ctrl->hwtype = ZIGBEE;
	zigbee_ctrl->config = 0;

	options_add(&zigbee_ctrl->options, 'c', "command", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_ctrl->options, 'l', "list", OPTION_NO_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&zigbee_ctrl->options, 'j', "permitjoin", OPTION_NO_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&zigbee_ctrl->options, 'k', "identify", OPTION_NO_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&zigbee_ctrl->options, 'a', "autodiscover", OPTION_NO_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&zigbee_ctrl->options, 'f', "discoverattributes", OPTION_HAS_VALUE, 0, JSON_STRING, NULL, NULL);
	options_add(&zigbee_ctrl->options, 'q', "NWK_addr_req", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_ctrl->options, 'b', "bind", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_ctrl->options, 'u', "unbind", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_ctrl->options, 'v', "listbind", OPTION_NO_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_ctrl->options, 'r', "setrep", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_ctrl->options, 'g', "getrep", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_ctrl->options, 't', "attributeid", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_ctrl->options, 'w', "attrtype", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_ctrl->options, 'm', "minimum", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_ctrl->options, 'n', "maximum", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_ctrl->options, 'o', "reportablechange", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_ctrl->options, 's', "shortAddr", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_ctrl->options, 'x', "srcExtAddr", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_ctrl->options, 'y', "srcEp", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);
	options_add(&zigbee_ctrl->options, 'd', "dstExtAddr", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_STRING, NULL, NULL);
	options_add(&zigbee_ctrl->options, 'e', "dstEp", OPTION_HAS_VALUE, DEVICES_VALUE, JSON_NUMBER, NULL, NULL);

	zigbee_ctrl->createCode=&createCode;
	zigbee_ctrl->printHelp=&printHelp;
}

#if defined(MODULE) && !defined(_WIN32)
void compatibility(struct module_t *module) {
	module->name = "zigbee_ctrl";
	module->version = "0.1";
	module->reqversion = "7.0";
	module->reqcommit = NULL;
}

void init(void) {
	zigbeeCtrlInit();
}
#endif
