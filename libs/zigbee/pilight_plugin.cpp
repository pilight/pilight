/*
 * pilight_plugin.ccp
 *
 *  Created on: Dec 18, 2016
 *      Author: ma-ca
 */

#include <QtPlugin>
#include <QtNetwork>
#include "pilight_plugin.h"

#define MAX_STR 1000
#define TMP_STR 100
#define MAX_ATTR 0x0006
#define TCP_PORT 5008

/*! Plugin constructor.
    \param parent - the parent object
 */
PilightApsPlugin::PilightApsPlugin(QObject *parent) : QObject(parent) {
    // keep a pointer to the ApsController
    m_apsCtrl = deCONZ::ApsController::instance();
    DBG_Assert(m_apsCtrl != 0);

    // APSDE-DATA.confirm handler
    connect(m_apsCtrl, SIGNAL(apsdeDataConfirm(const deCONZ::ApsDataConfirm&)),
            this, SLOT(apsdeDataConfirm(const deCONZ::ApsDataConfirm&)));

    // APSDE-DATA.indication handler
    connect(m_apsCtrl, SIGNAL(apsdeDataIndication(const deCONZ::ApsDataIndication&)),
            this, SLOT(apsdeDataIndication(const deCONZ::ApsDataIndication&)));

    m_isconnected = false;
    m_readattr = false;
    m_readclust = 0;
    m_zdpmatchreq = false;
    m_shortaddr = 0;
    m_attrid = -1;
    m_cluster = 0;
    m_profile = 0;
    m_zclSeq = 0;
    m_ep = 0;
    tcpServer = new QTcpServer(this);
    if (!tcpServer->listen(QHostAddress::Any, TCP_PORT)) {  // hardcoded listen port 5008
    	DBG_Printf(DBG_INFO, "Unable to start the server: %s\n", tcpServer->errorString().toStdString().data());
    }
    QString ipAddress;
    QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();
    // use the first non-localhost IPv4 address
    for (int i = 0; i < ipAddressesList.size(); ++i) {
        if (ipAddressesList.at(i) != QHostAddress::LocalHost &&
            ipAddressesList.at(i).toIPv4Address()) {
            ipAddress = ipAddressesList.at(i).toString();
            break;
        }
    }
    // if we did not find one, use IPv4 localhost
    if (ipAddress.isEmpty())
        ipAddress = QHostAddress(QHostAddress::LocalHost).toString();

    DBG_Printf(DBG_INFO, "The server is running on\n\nIP: %s  port: %d\n\n",
    		ipAddress.toStdString().data(), tcpServer->serverPort());

    connect(tcpServer, SIGNAL(newConnection()), this, SLOT(receiveCommand()));
}

/*! Deconstructor for plugin.
 */
PilightApsPlugin::~PilightApsPlugin() {
    m_apsCtrl = 0;
}

/*! APSDE-DATA.indication callback.
    \param ind - the indication primitive
    \note Will be called from the main application for every incoming indication.
    Any filtering for nodes, profiles, clusters must be handled by this plugin.
 */
void PilightApsPlugin::apsdeDataIndication(const deCONZ::ApsDataIndication &ind) {
    DBG_Printf(DBG_INFO,
    		"profileid %04X, clusterid %04X, srcEndpoint %02X, dstEndpoint %02X, status 0x%02X, securityStatus %02X\n",
    		ind.profileId(), ind.clusterId(), ind.srcEndpoint(), ind.dstEndpoint(), ind.status(), ind.securityStatus());

    unsigned int i;
    unsigned int length = ind.asdu().length();
    int strlen = length * 3;  // print each byte in hex (2 bytes) and one white space
    char rawstr[strlen+1];
	char strout[MAX_STR];
	char strtmp[TMP_STR];

    memset(rawstr, '\0', strlen + 1);
	memset(strout, '\0', MAX_STR);
	memset(strtmp, '\0', TMP_STR);

    const char *raw = ind.asdu().data();
	for (i = 0; i < length; i++) {
		char rawtmp[4];
		memset(rawtmp, '\0', 4);
		snprintf(rawtmp, 4, "%02X ", *raw++);
		strcat(rawstr, rawtmp);
	}
	strcat(rawstr, "\n");

	DBG_Printf(DBG_INFO, "APS Ind %d, %s: %s", length, getApsIndSrcAddr(ind).c_str(), rawstr);
	if (ind.status() != deCONZ::ApsSuccessStatus) {
		DBG_Printf(DBG_INFO, "APS Ind ERROR status 0x%02X\n", ind.status());
		return;
	}

	if (ind.clusterId() == 0x000A) {
		DBG_Printf(DBG_INFO, "<-=======> handle Time Cluster <=================== \n");

	}

	if (ind.profileId() == ZDP_PROFILE_ID) {  // handle ZDP response
		switch(ind.clusterId()) {
		case ZDP_SIMPLE_DESCRIPTOR_RSP_CLID:
			handleZdpSimpleResponse(ind);
			break;
		case ZDP_ACTIVE_ENDPOINTS_RSP_CLID:
			handleZdpActiveEpResponse(ind);
			break;
		case ZDP_MATCH_DESCRIPTOR_RSP_CLID:
			handleZdpMatchResponse(ind);
			break;
		case ZDP_MGMT_LQI_RSP_CLID:
			handleZdpLqiResponse(ind);
			break;
		case ZDP_MGMT_BIND_RSP_CLID:
			handleZdpBindResponse(ind);
			break;
		case ZDP_POWER_DESCRIPTOR_RSP_CLID:
			handleZdpPowerResponse(ind);
			break;
		case ZDP_NWK_ADDR_RSP_CLID:
			handleZdpNwkAddrResponse(ind);
			break;
		default:
			DBG_Printf(DBG_INFO, "ZDP clusterId = 0x%04X \n", ind.clusterId());
			break;
		}
		return; // end ZDP response
	}

	// handle ZCL response
	QDataStream stream(ind.asdu());
    stream.setByteOrder(QDataStream::LittleEndian);
    deCONZ::ZclFrame zclframe;
    zclframe.readFromStream(stream);

	if ((zclframe.frameControl() & 0x09) == 0x09 /*== 0x19 or 0x1D*/) { // ZclFCClusterCommand and ZclFCDirectionServerToClient or ZclFCManufacturerSpecific
		handleZclServerToClientResponse(ind, zclframe);
		return;
	}
	if ((zclframe.frameControl() & 0x08) == 0x08 /*== 0x18 or 0x1C*/) { // ZclFCProfileCommand or ZclFCManufacturerSpecific
		switch(zclframe.commandId()) {
		case deCONZ::ZclReadAttributesResponseId: // 0x01
			handleZclReadAttrbuteResponse(ind, zclframe);
			break;
		case deCONZ::ZclReadReportingConfigResponseId: // 0x09
			handleZclReportConfigResponse(ind, zclframe);
			break;
		case deCONZ::ZclReportAttributesId: // 0x0A
			handleZclReportAttributeResponse(ind, zclframe);
			break;
		case deCONZ::ZclDiscoverAttributesResponseId: // 0x0D
			handleZclDiscoverAttributesResponse(ind, zclframe);
			break;
		default:
			DBG_Printf(DBG_INFO, "ZCL commandId = 0x%02X \n", zclframe.commandId());
			break;
		}
		return; // end ZclFCProfileCommand
	}
}

/*! APSDE-DATA.confirm callback.
    \param conf - the confirm primitive
    \note Will be called from the main application for each incoming confirmation,
    even if the APSDE-DATA.request was not issued by this plugin.
 */
void PilightApsPlugin::apsdeDataConfirm(const deCONZ::ApsDataConfirm &conf) {
    std::list<deCONZ::ApsDataRequest>::iterator i = m_apsReqQueue.begin();
    std::list<deCONZ::ApsDataRequest>::iterator end = m_apsReqQueue.end();

	deCONZ::Address srcAddress = conf.dstAddress();
	const deCONZ::ApsAddressMode srcAddressMode = conf.dstAddressMode();

	char strtmp[TMP_STR], data[MAX_STR];
	if (srcAddressMode == 0x3) {        //!< 64-bit extended IEEE address mode
		snprintf(strtmp, TMP_STR, "0x%016llX", srcAddress.ext());
	} else if (srcAddressMode == 0x2) { //!< 16-bit short network address mode
		snprintf(strtmp, TMP_STR, "0x%04X", srcAddress.nwk());
	}

    // search the list of currently active requests
    // and check if the confirmation belongs to one of them
    for (; i != end; ++i) {
        if (i->id() == conf.id()) {
            m_apsReqQueue.erase(i);

            snprintf(data, MAX_STR,
            		"APS-DATA.confirm status 0x%02X, id = 0x%02X, srcEp = 0x%02X, dstcEp = 0x%02X, dstAddr = %s\n",
            		conf.status(), conf.id(), conf.srcEndpoint(), conf.dstEndpoint(), strtmp);

            if (conf.status() != deCONZ::ApsSuccessStatus) {
                snprintf(data, MAX_STR,
                		"<-APS-DATA.confirm FAILED status 0x%02X, id = 0x%02X, srcEp = 0x%02X, dstcEp = 0x%02X, dstAddr = %s\n",
						conf.status(), conf.id(), conf.srcEndpoint(), conf.dstEndpoint(), strtmp);
                writeToConnectedClients(data);
            }
            DBG_Printf(DBG_INFO, "%s", data);

            return;
        }
    }
}

/*! get APS Ind source address in 64-Bit or 16-Bit string
 */
std::string PilightApsPlugin::getApsIndSrcAddr(const deCONZ::ApsDataIndication &ind) {
	char straddr[19];
	if (ind.srcAddressMode() == 0x3) {        //!< 64-bit extended IEEE address mode
		sprintf(straddr, "0x%016llX", ind.srcAddress().ext());
		return std::string(straddr);
	} else if (ind.srcAddressMode() == 0x2) { //!< 16-bit short network address mode
		sprintf(straddr, "0x%04X", ind.srcAddress().nwk());
		return std::string(straddr);
	}

	return std::string();  // empty string
}

/*! get Cluster ID name
 */
const char *PilightApsPlugin::getClusterName(uint16_t clusterid) {
	switch (clusterid) {
		case 0x0000: return "BASIC_CLUSTER_ID";                                     //!<Basic cluster Id
		case 0x0001: return "POWER_CONFIGURATION_CLUSTER_ID";         //!<Power configuration cluster Id
		case 0x0003: return "IDENTIFY_CLUSTER_ID";                               //!<Identify cluster Id
		case 0x0004: return "GROUPS_CLUSTER_ID";                                   //!<Groups cluster Id
		case 0x0005: return "SCENES_CLUSTER_ID";                                   //!<Scenes cluster Id
		case 0x0006: return "ONOFF_CLUSTER_ID";                                     //!<OnOff cluster id
		case 0x0007: return "ONOFF_SWITCH_CONFIGURATION_CLUSTER_ID";          //!<OnOff Switch Configuration cluster id
		case 0x0008: return "LEVEL_CONTROL_CLUSTER_ID";                     //!<Level Control cluster id
		case 0x0009: return "ALARMS_CLUSTER_ID";                                   //!<Alarm cluster id
		case 0x000a: return "TIME_CLUSTER_ID";                                       //!<Time cluster Id
		case 0x0019: return "OTAU_CLUSTER_ID";                                       //!<OTAU cluster Id
		case 0x0201: return "THERMOSTAT_CLUSTER_ID";                           //!<Thermostat cluster Id
		case 0x0202: return "FAN_CONTROL_CLUSTER_ID";                         //!<Fan control cluster Id
		case 0x0204: return "THERMOSTAT_UI_CONF_CLUSTER_ID";           //!<Thermostat ui conf cluster Id
		  /* Lighting */
		case 0x0300: return "COLOR_CONTROL_CLUSTER_ID";                     //!<Color Control cluster id
		case 0x0400: return "ILLUMINANCE_MEASUREMENT_CLUSTER_ID"; //!<Illuminance Sensing cluster id
		case 0x0402: return "TEMPERATURE_MEASUREMENT_CLUSTER_ID"; //!<Temperature measurement cluster id
		case 0x0405: return "HUMIDITY_MEASUREMENT_CLUSTER_ID";       //!<Humidity measurement cluster id
		case 0x0406: return "OCCUPANCY_SENSING_CLUSTER_ID";             //!<Occupancy Sensing cluster id
		  /* Security & Safety */
		case 0x0500: return "IAS_ZONE_CLUSTER_ID";                               //!<IAS Zone Cluster id
		case 0x0501: return "IAS_ACE_CLUSTER_ID";                                 //!<IAS ACE Cluster id
		case 0x0600: return "GENERIC_TUNNEL_CLUSTER_ID";                   //!<Generic tunnel cluster Id
		case 0x0601: return "BACNET_PROTOCOL_TUNNEL_CLUSTER_ID";   //!<BACnet protocol tunnel cluster Id
		  /* Smart Energy Profile specific clusters */
		case 0x0700: return "PRICE_CLUSTER_ID";                                     //!<Price cluster Id
		case 0x0701: return "DEMAND_RESPONSE_AND_LOAD_CONTROL_CLUSTER_ID";    //!<Demand Response and Load Control cluster Id
		case 0x0702: return "SIMPLE_METERING_CLUSTER_ID";                 //!<Simple Metering cluster Id
		case 0x0703: return "MESSAGE_CLUSTER_ID";                                 //!<Message Cluster Id
		case 0x0704: return "ZCL_SE_TUNNEL_CLUSTER_ID";                     //!<Smart Energy Tunneling (Complex Metering)
		case 0x0800: return "ZCL_KEY_ESTABLISHMENT_CLUSTER_ID";     //!<ZCL Key Establishment Cluster Id
		case 0x0b05: return "DIAGNOSTICS_CLUSTER_ID";                         //!<Diagnostics cluster Id
		  /* Light Link Profile clusters */
		case 0x1000: return "ZLL_COMMISSIONING_CLUSTER_ID";             //!<ZLL Commissioning Cluster Id
		  /* Manufacturer specific clusters */
		case 0xFF00: return "LINK_INFO_CLUSTER_ID";                             //!<Link Info cluster id

		default: return "unknown";
	}
}

/*! get Attribute Type ID name
 */
const char *PilightApsPlugin::getAttributeTypeIdName(uint8_t attrtypeid) {
	switch(attrtypeid) {
		case 0x00: return "NoData";
		case 0x08: return "8BitData";
		case 0x09: return "16BitData";
		case 0x0a: return "24BitData";
		case 0x0b: return "32BitData";
		case 0x0c: return "40BitData";
		case 0x0d: return "48BitData";
		case 0x0e: return "56BitData";
		case 0x0f: return "64BitData";
		case 0x10: return "Boolean";
		case 0x18: return "8BitBitMap";
		case 0x19: return "16BitBitMap";
		case 0x1a: return "24BitBitMap";
		case 0x1b: return "32BitBitMap";
		case 0x1c: return "40BitBitMap";
		case 0x1d: return "48BitBitMap";
		case 0x1e: return "56BitBitMap";
		case 0x1f: return "64BitBitMap";
		case 0x20: return "8BitUint";
		case 0x21: return "16BitUint";
		case 0x22: return "24BitUint";
		case 0x23: return "32BitUint";
		case 0x24: return "40BitUint";
		case 0x25: return "48BitUint";
		case 0x26: return "56BitUint";
		case 0x27: return "64BitUint";
		case 0x28: return "8BitInt";
		case 0x29: return "16BitInt";
		case 0x2a: return "24BitInt";
		case 0x2b: return "32BitInt";
		case 0x2c: return "40BitInt";
		case 0x2d: return "48BitInt";
		case 0x2e: return "56BitInt";
		case 0x2f: return "64BitInt";
		case 0x30: return "8BitEnum";
		case 0x31: return "16BitEnum";
		case 0x41: return "OctedString";
		case 0x42: return "CharacterString";
		case 0x43: return "LongOctedString";
		case 0x44: return "LongCharacterString";
		case 0x48: return "Array";
		case 0xe0: return "TimeOfDay";
		case 0xe1: return "Date";
		case 0xe2: return "UtcTime";
		case 0xe8: return "ClusterId";
		case 0xe9: return "AttributeId";
		case 0xea: return "BACNetOId";
		case 0xf0: return "IeeeAddress";
		case 0xf1: return "128BitSecurityKey";

		default: return "unknown";

	}

}

/*! Handles a simple descriptor request response. (ClusterID = 0x8004)
    \param ind a Simple_Desc_rsp ZigBee specification 2.4.4.1.5
 */
void PilightApsPlugin::handleZdpSimpleResponse(const deCONZ::ApsDataIndication &ind) {
	unsigned int i;
	char strout[MAX_STR];
	memset(strout, '\0', MAX_STR);
	char strtmp[TMP_STR];
	memset(strtmp, '\0', TMP_STR);
	const char *simpleraw = ind.asdu().data();
	if (ind.asdu().length() > 2) {
		simpleraw++;  // skip sequence number
		uint8_t zdpstatus = *simpleraw++; //!< Result of a ZDP request.
		uint16_t *nwkAddr = (uint16_t *) simpleraw; //!< NWK address of the simple descriptor request
		simpleraw += 2;
		uint8_t length = *simpleraw; //!< Length
		if (length > 0) {
			simpleraw++;
			uint8_t endpoint = *simpleraw++; //!<
			uint16_t *AppProfileId = (uint16_t *) simpleraw; //!< The application profile identifier field
			simpleraw += 2;
			uint16_t *AppDeviceId = (uint16_t *) simpleraw; //!< The application device identifier
			simpleraw += 2;
			uint8_t AppDeviceVersion = *simpleraw++;    //!< The application device identifier field
			uint8_t AppInClustersCount = *simpleraw++;  //!< The application input cluster count
			snprintf(strtmp, TMP_STR, "<-CLUSTER %s 0x%04X ep 0x%02X profile 0x%04X deviceid 0x%04X deviceversion 0x%02X\n",
					getApsIndSrcAddr(ind).c_str(), *nwkAddr, endpoint, *AppProfileId, *AppDeviceId, AppDeviceVersion);
			int x = snprintf(strout, MAX_STR, strtmp);
			for (i = 0; i < AppInClustersCount; i++) {
				uint16_t *clusterid = (uint16_t *) simpleraw;
				snprintf(strtmp, TMP_STR, "<-CLUSTER %s 0x%04X 0x%02X In  0x%04X %s\n",
						getApsIndSrcAddr(ind).c_str(), *nwkAddr, endpoint, *clusterid, getClusterName(*clusterid));
				x += snprintf(&strout[x], MAX_STR-x, strtmp);
				simpleraw += 2;
			}
			uint8_t AppOutClustersCount = *simpleraw++; //!< The application output cluster count
			for (i = 0; i < AppOutClustersCount; i++) {
				uint16_t *clusterid = (uint16_t *) simpleraw;
				snprintf(strtmp, TMP_STR, "<-CLUSTER %s 0x%04X 0x%02X Out 0x%04X %s\n",
						getApsIndSrcAddr(ind).c_str(), *nwkAddr, endpoint, *clusterid, getClusterName(*clusterid));
				x += snprintf(&strout[x], MAX_STR-x, strtmp);
				simpleraw += 2;
			}
		} else {
			snprintf(strtmp, TMP_STR, "<-CLUSTER %s response error 0x%04X length = %d ", getApsIndSrcAddr(ind).c_str(), *nwkAddr, length);
			strcat(strout, strtmp);
			switch(zdpstatus) {
			case 0x00: snprintf(strtmp, TMP_STR, "SUCCESS\n"); break;
			case 0x80: snprintf(strtmp, TMP_STR, "INVALID_REQUEST\n"); break;
			case 0x81: snprintf(strtmp, TMP_STR, "DEVICE_NOT_FOUND\n"); break;
			case 0x82: snprintf(strtmp, TMP_STR, "INVALID_EP\n"); break;
			case 0x83: snprintf(strtmp, TMP_STR, "NOT_ACTIVE\n"); break;
			case 0x89: snprintf(strtmp, TMP_STR, "NO_DESCRIPTOR\n"); break;
			default: snprintf(strtmp, TMP_STR, "status = 0x%02X\n", zdpstatus);
			}
			strcat(strout, strtmp);
		}
		DBG_Printf(DBG_INFO, "%s", strout);
		if (m_isconnected) {
			writeToConnectedClients(strout);
		}
	} else {
		snprintf(strtmp, TMP_STR, "<-CLUSTER %s response error\n", getApsIndSrcAddr(ind).c_str());
		strcat(strout, strtmp);
	}
}

/*! Handles an active endpoint request response.
    \param ind a ZDP Active_EP_rsp ZigBee specification 2.4.4.1.6
 */
void PilightApsPlugin::handleZdpActiveEpResponse(const deCONZ::ApsDataIndication &ind) {
	unsigned int i;
	char strout[MAX_STR];
	memset(strout, '\0', MAX_STR);
	char strtmp[TMP_STR];
	memset(strtmp, '\0', TMP_STR);
	const char *epraw = ind.asdu().data();
	if (ind.asdu().length() > 2) {
		epraw += 2;
		uint16_t *nwkAddr = (uint16_t *) epraw; //!< NWK address of the active endpoints request
		epraw += 2;
		uint8_t activeEPCount = *epraw; //!< Count of active endpoints on the remote device.
		for (i = 0; i < activeEPCount; i++) {
			epraw++;
			snprintf(strtmp, TMP_STR, "<-EP %s 0x%04X %3d (%02X)\n", getApsIndSrcAddr(ind).c_str(), *nwkAddr, *epraw, *epraw);
			strcat(strout, strtmp);
		}
		DBG_Printf(DBG_INFO, "%s", strout);
		if (m_isconnected) {
			writeToConnectedClients(strout);
		}
	}
}

/*! Handles an match request response.
    \param ind a ZDP Match_Desc_rsp ZigBee specification 2.4.4.1.7
 */
void PilightApsPlugin::handleZdpMatchResponse(const deCONZ::ApsDataIndication &ind) {
	unsigned int i;
	char strout[MAX_STR];
	memset(strout, '\0', MAX_STR);
	char strtmp[TMP_STR];
	memset(strtmp, '\0', TMP_STR);
	const char *matchraw = ind.asdu().data();
	if (ind.asdu().length() >= 2) {
		if (ind.asdu().length() >= 6) {
			matchraw += 2; // skip 2 bytes
			uint16_t *nwkAddr = (uint16_t *) matchraw;
			matchraw += 2;
			uint8_t matchLength = *matchraw;
			for (i = 0; i < matchLength; i++) {
				matchraw++;
				snprintf(strtmp, TMP_STR, "<-ZDP match %s 0x%04X %3d 0x%04X 0x%04X\n",
						getApsIndSrcAddr(ind).c_str(), *nwkAddr, *matchraw, m_cluster, m_profile);
				strcat(strout, strtmp);
			}
		} else {
			uint8_t seqNum = *matchraw++;  //!< Sequence number of a ZDP command
			uint8_t zdpstatus = *matchraw; //!< Result of a ZDP request.
			snprintf(strout, MAX_STR, "<-ZDP match %s seqNum = 0x%02X zdpstatus = 0x%02X\n",
					getApsIndSrcAddr(ind).c_str(), seqNum, zdpstatus);
		}
		DBG_Printf(DBG_INFO, "%s", strout);
		if (m_isconnected) {
			writeToConnectedClients(strout);
		}
	}
}

/*! Handles an LQI request response.
    \param ind a ZDP Mgmt_Lqi_rsp ZigBee specification 2.4.4.3.2
 */
void PilightApsPlugin::handleZdpLqiResponse(const deCONZ::ApsDataIndication &ind) {
	unsigned int i;
	char strout[MAX_STR];
	memset(strout, '\0', MAX_STR);
	char strtmp[TMP_STR];
	memset(strtmp, '\0', TMP_STR);
	snprintf(strout, MAX_STR, "<-LQI %s %3d", getApsIndSrcAddr(ind).c_str(), ind.srcEndpoint());
	const char *lqiraw = ind.asdu().data();
	if (ind.asdu().length() >= 27) {
		lqiraw += 2 ; // skip 2 bytes
		uint8_t neighborTableEntries = *lqiraw++;
		uint8_t startIndex = *lqiraw++;
		uint8_t neighborTableListCount = *lqiraw++;
		lqiraw += 8; // skip Ext PANID
		uint64_t *extAddr = (uint64_t *) lqiraw;
		lqiraw += 8;
		uint16_t *networkAddr = (uint16_t *) lqiraw;
		lqiraw += 2;
		snprintf(strtmp, TMP_STR, "%d %d %d 0x%016llX 0x%04X ",
				neighborTableEntries, startIndex, neighborTableListCount, *extAddr, *networkAddr);
		strcat(strout, strtmp);

		uint8_t deviceType = *lqiraw & 0x03;			// 0000 00xx 0=CO, 1=RD, 2=ED, 3=unknown
		uint8_t rxOnWhenIdle = (*lqiraw >> 2) & 0x03;	// 0000 xx00 0=no, 1=yes, 2=unknown
		uint8_t relationship = (*lqiraw >> 4) & 0x07;	// 0xxx 0000 parent/child/sibling/none/previous child
		snprintf(strtmp, TMP_STR, "%u %u %u ", deviceType, rxOnWhenIdle, relationship);
		strcat(strout, strtmp);

		lqiraw++;
		for (i = 0; i < 3; i++) {  // depth lqi
			snprintf(strtmp, TMP_STR, "%02X ", *lqiraw++); // print hex
			strcat(strout, strtmp);
		}

		strcat(strout, "\n");
		DBG_Printf(DBG_INFO, "%s", strout);
		if (m_isconnected) {
			writeToConnectedClients(strout);
		}
	}
}

/*! Handles an Bind request response.
    \param ind a ZDP Mgmt_Bind_rsp ZigBee specification 2.4.4.3.4
 */
void PilightApsPlugin::handleZdpBindResponse(const deCONZ::ApsDataIndication &ind) {
	unsigned int i;
	char strout[MAX_STR];
	memset(strout, '\0', MAX_STR);
	char strtmp[TMP_STR];
	memset(strtmp, '\0', TMP_STR);
	const char *bindraw = ind.asdu().data();
	if (ind.asdu().length() >= 18) {
		bindraw += 2 ; // skip 2 bytes
		uint8_t bindingTableEntries = *bindraw++;
		uint8_t startIndex = *bindraw++;
		uint8_t bindingTableListCount = *bindraw;

		for (i = 0; i < bindingTableListCount; i++) {  // read bind table entries
			snprintf(strout, MAX_STR, "<-BIND %s ", getApsIndSrcAddr(ind).c_str());
			bindraw++;
			uint64_t *srcAddr = (uint64_t *) bindraw;
			bindraw += 8;
			uint8_t srcEndpoint = *bindraw++;
			uint16_t *clusterId = (uint16_t *) bindraw;
			bindraw += 2;
			uint8_t dstAddrMode = *bindraw++;
			snprintf(strtmp, TMP_STR, "%d %d %d 0x%016llX 0x%02X 0x%04X ",
					bindingTableEntries, startIndex, bindingTableListCount, *srcAddr, srcEndpoint, *clusterId);
			strcat(strout, strtmp);
			if (dstAddrMode == 0x03) { // 0x03 = 64-bit extended address for dstAddr and dstEndpoint present
				uint64_t *dstExtAddr = (uint64_t *) bindraw;
				bindraw += 8;
				uint8_t dstEndpoint = *bindraw;
				snprintf(strtmp, TMP_STR, "0x%016llX 0x%02X", *dstExtAddr, dstEndpoint);
				strcat(strout, strtmp);
			} else if (dstAddrMode == 0x01) { //0x01 = 16-bit group address for dstAddr and dstEndoint not present
				uint16_t *dstGroupAddr = (uint16_t *) bindraw;
				snprintf(strtmp, TMP_STR, "0x%04X", *dstGroupAddr);
				strcat(strout, strtmp);
				bindraw++; // move pointer to end
			}

			strcat(strout, "\n");
			DBG_Printf(DBG_INFO, "%s", strout);
			if (m_isconnected) {
				writeToConnectedClients(strout);
			}
		}
	} else if (ind.asdu().length() > 2) {
		bindraw += 2;
		unsigned int bindrawlen = ind.asdu().length() - 2;
		snprintf(strout, MAX_STR, "BIND %s ", getApsIndSrcAddr(ind).c_str());
		for (i = 0; i < bindrawlen; i++) {
			bindraw++;
			snprintf(strtmp, TMP_STR, "%02X ", *bindraw);
			strcat(strout, strtmp);
		}
		strcat(strout, "\n");
		DBG_Printf(DBG_INFO, "%s", strout);
		if (m_isconnected) {
			writeToConnectedClients(strout);
		}
	}
}

/*! Handles an power descriptor request response.
    \param ind a ZDP Power_Desc_rsp ZigBee specification 2.4.4.2.4
 */
void PilightApsPlugin::handleZdpPowerResponse(const deCONZ::ApsDataIndication &ind) {
	char strout[MAX_STR];
	memset(strout, '\0', MAX_STR);
	char strtmp[TMP_STR];
	memset(strtmp, '\0', TMP_STR);
	snprintf(strout, MAX_STR, "<-POWER %s ", getApsIndSrcAddr(ind).c_str());
	const char *powraw = ind.asdu().data();
	if (ind.asdu().length() > 2) {
		powraw++;  // skip sequence number
		uint8_t zdpstatus = *powraw++; //!< Result of a ZDP request.
		uint16_t *nwkAddr = (uint16_t *) powraw; //!< NWK address of the active endpoints request
		powraw += 2;
		snprintf(strtmp, TMP_STR, "0x%04X", *nwkAddr);
		strcat(strout, strtmp);
		if (zdpstatus != 0) {
			// SUCCESS, DEVICE_NOT_FOUND, INV_REQUESTTYPE or NO_DESCRIPTOR
			switch(zdpstatus) {
				case 0x80:
					snprintf(strtmp, TMP_STR, " INV_REQUESTTYPE");
					break;
				case 0x81:
					snprintf(strtmp, TMP_STR, " DEVICE_NOT_FOUND");
					break;
				case 0x89:
					snprintf(strtmp, TMP_STR, " NO_DESCRIPTOR");
					break;
				default:
					break;
			}
		} else {
			// Power descriptor 4 x 4 bits
			int powmode = *powraw & 0x0F;             // Current power mode
			int availsrc = (*(powraw++) & 0xF0) >> 4; // Available power sources
			int cursrc = *powraw & 0x0F;              // Current power source
			int powlevel= (*powraw & 0xF0) >> 4;      // Current power source level
			snprintf(strtmp, TMP_STR, " %X %X %X", powmode, availsrc, cursrc);
			strcat(strout, strtmp);
			// 0, 33%, 66%, 100%
			switch (powlevel) {
				case 0x00:
					snprintf(strtmp, TMP_STR, " 0");
					break;
				case 0x04:
					snprintf(strtmp, TMP_STR, " 33");
					break;
				case 0x08:
					snprintf(strtmp, TMP_STR, " 66");
					break;
				case 0x0C:
					snprintf(strtmp, TMP_STR, " 100");
					break;
				default:
					break;
			}
		}
		strcat(strout, strtmp);
		snprintf(strtmp, TMP_STR, "\n");
		strcat(strout, strtmp);
		DBG_Printf(DBG_INFO, "%s", strout);
		if (m_isconnected) {
			writeToConnectedClients(strout);
		}
	}
}

/*! Handles a NWK_addr_req (ClusterID=0x0000) request response.
    \param ind a ZDP NWK_addr_resp ZigBee specification 2.4.4.2.1
 */
void PilightApsPlugin::handleZdpNwkAddrResponse(const deCONZ::ApsDataIndication &ind) {
	char strout[MAX_STR];
	memset(strout, '\0', MAX_STR);
	char strtmp[TMP_STR];
	memset(strtmp, '\0', TMP_STR);
	snprintf(strout, MAX_STR, "<-NWK %s ", getApsIndSrcAddr(ind).c_str());
	const char *nwkraw = ind.asdu().data();
	if (ind.asdu().length() > 2) {
		nwkraw++;  // skip sequence number
		uint8_t zdpstatus = *nwkraw++; //!< Result of a ZDP request.
		if (zdpstatus != 0) {
			// SUCCESS, INV_REQUESTTYPE, or DEVICE_NOT_FOUND
			switch(zdpstatus) {
				case 0x80:
					snprintf(strtmp, TMP_STR, "INV_REQUESTTYPE ");
					break;
				case 0x81:
					snprintf(strtmp, TMP_STR, "DEVICE_NOT_FOUND ");
					break;
				default:
					break;
			}
			strcat(strout, strtmp);
		}
		uint64_t *extAddr = (uint64_t *) nwkraw; // IEEEAddr RemoteDev
		nwkraw += 8;
		uint16_t *nwkAddr = (uint16_t *) nwkraw; //!< NWK address of the remote device
		snprintf(strtmp, TMP_STR, "0x%016llX 0x%04X", *extAddr, *nwkAddr);
		strcat(strout, strtmp);
		snprintf(strtmp, TMP_STR, "\n");
		strcat(strout, strtmp);
		DBG_Printf(DBG_INFO, "%s", strout);
		if (m_isconnected) {
			writeToConnectedClients(strout);
		}
	}
}

/*! Handles a ZCL Read Attribute Response
    \param ind a ZDP Active_EP_rsp
 */
void PilightApsPlugin::handleZclReadAttrbuteResponse(const deCONZ::ApsDataIndication &ind,
		const deCONZ::ZclFrame &zclframe) {
	char strout[MAX_STR];
	memset(strout, '\0', MAX_STR);
	char strtmp[TMP_STR];
	memset(strtmp, '\0', TMP_STR);
	const char *respvalue = zclframe.payload().data();
	const char *endvalue = respvalue + zclframe.payload().length();
	uint16_t *attrid = (uint16_t *) respvalue;
	respvalue += 3;  // skip attribute id and status
	bool writestrout = true;
	if (zclframe.payload().length() > 3) {
		uint8_t type = *respvalue++; // ZCL attribute typeid
		if (m_readattr && ind.clusterId() == 0x0000 && m_zclSeq == zclframe.sequenceNumber()) {
			snprintf(strout, MAX_STR, "<-ZCL attr 0x%04X %d 0x%04X 0x%04X ",
					m_shortaddr, ind.srcEndpoint(), m_readclust, *attrid);
		} else {
			snprintf(strout, MAX_STR, "<-APS attr %s %d 0x%04X 0x%04X 0x%02X ",
					getApsIndSrcAddr(ind).c_str(), ind.srcEndpoint(), ind.clusterId(), *attrid, type);
		}
		if (0x42 == type) {  // type 0x42 string
			uint8_t stringlength = *respvalue++;
			const char *endstring = respvalue + stringlength;
			while (respvalue < endstring) {
				snprintf(strtmp, TMP_STR, "%c", *respvalue++);    // print char
				strcat(strout, strtmp);
			}
		} else {
			while (respvalue < endvalue) {
				snprintf(strtmp, TMP_STR, "%02X ", *respvalue++); // print hex
				strcat(strout, strtmp);
			}
		}
	} else { // error
		if (m_readattr && ind.clusterId() == 0x0000 && m_zclSeq == zclframe.sequenceNumber()) {
			snprintf(strout, MAX_STR, "<-ZCL attr 0x%04X %d 0x%04X 0x%04X unknown",
					m_shortaddr, ind.srcEndpoint(), m_readclust, *attrid);
		} else {
			writestrout = false;
			snprintf(strout, MAX_STR, "APS attr %s %d 0x%04X 0x%04X error ",
					strtmp, ind.srcEndpoint(), ind.clusterId(), *attrid);
		}
	}
	strcat(strout, "\n");
	DBG_Printf(DBG_INFO, "%s", strout);
	if (m_isconnected && writestrout) {
		writeToConnectedClients(strout);
	}

	// start next read attribute request if Basic Cluster
	if (m_readattr
			&& ind.clusterId() == 0x0000
			&& *attrid == m_attrid
			&& m_zclSeq == zclframe.sequenceNumber()) {
		if (m_attrid < MAX_ATTR) {
			m_attrid++;
			sendZclReadAttributeRequest(m_shortaddr, m_ep, 0x0000, m_attrid);
		} else if (m_attrid != 0x4000) {
			m_attrid = 0x4000;
			sendZclReadAttributeRequest(m_shortaddr, m_ep, 0x0000, m_attrid);
		} else {
			m_readattr = false;
			m_readclust = 0;
		}
	}

}

/*! Handles a ZCL Report Config request response.
    \param ind a ZDP Active_EP_rsp
 */
void PilightApsPlugin::handleZclReportConfigResponse(const deCONZ::ApsDataIndication &ind,
		const deCONZ::ZclFrame &zclframe) {
	char strout[MAX_STR];
	memset(strout, '\0', MAX_STR);
	char strtmp[TMP_STR];
	memset(strtmp, '\0', TMP_STR);
	const char *respvalue = zclframe.payload().data();
	const char *endvalue = respvalue + zclframe.payload().length();
	snprintf(strout, MAX_STR, "<-REP config %s %d 0x%04X ",
			getApsIndSrcAddr(ind).c_str(), ind.srcEndpoint(), ind.clusterId());
	uint8_t zclstatus = *respvalue++;
	if (zclstatus != 0x00) {
		switch(zclstatus) {
		case 0x86:
			snprintf(strtmp, TMP_STR, "UNSUPPORTED_ATTRIBUTE ");
			break;
		case 0x8C:
			snprintf(strtmp, TMP_STR, "UNREPORTABLE_ATTRIBUTE ");
			break;
		default:
			snprintf(strtmp, TMP_STR, "zcl error status = 0x%02X: ", zclstatus);
			break;
		}
		strcat(strout, strtmp);
	} else  {
		uint8_t direction = *respvalue++;
		uint16_t *attributeId = (uint16_t *) respvalue;
		respvalue += 2;
		uint8_t attributeType = *respvalue++;
		uint16_t *minReportingInterval = (uint16_t *) respvalue;
		respvalue += 2;
		uint16_t *maxReportingInterval = (uint16_t *) respvalue;
		respvalue += 2;
		snprintf(strtmp, TMP_STR, "dir %02X min %d max %d id 0x%04X type 0x%02X ",
				direction, *minReportingInterval, *maxReportingInterval, *attributeId, attributeType);
		strcat(strout, strtmp);
	}
	while (respvalue < endvalue) {
		snprintf(strtmp, TMP_STR, "%02X ", *respvalue++); // print hex
		strcat(strout, strtmp);
	}
	strcat(strout, "\n");
	DBG_Printf(DBG_INFO, "%s", strout);
	if (m_isconnected) {
		writeToConnectedClients(strout);
	}
}

/*! Handles a ZCL Report Attribute response.
    \param ind a ZDP Active_EP_rsp
 */
void PilightApsPlugin::handleZclReportAttributeResponse(const deCONZ::ApsDataIndication &ind,
			const deCONZ::ZclFrame &zclframe) {
	char strout[MAX_STR];
	memset(strout, '\0', MAX_STR);
	char strtmp[TMP_STR];
	memset(strtmp, '\0', TMP_STR);
	const char *respvalue = zclframe.payload().data();
	const char *endvalue = respvalue + zclframe.payload().length();
	snprintf(strout, MAX_STR,
			"<-ZCL attribute report %s 0x%04X %d ", getApsIndSrcAddr(ind).c_str(), ind.clusterId(), ind.srcEndpoint());
	while (respvalue < endvalue) {
		snprintf(strtmp, TMP_STR, "%02X ", *respvalue++); // print hex
		strcat(strout, strtmp);
	}
	strcat(strout, "\n");
	DBG_Printf(DBG_INFO, "%s", strout);
	if (m_isconnected) {
		writeToConnectedClients(strout);
	}
	if (!(zclframe.frameControl() & deCONZ::ZclFCDisableDefaultResponse)) {
		sendZclDefaultResponse(ind, zclframe, deCONZ::ZclSuccessStatus);
	}
}

/*! Handles a ZCL Discover Attributes response.
    \param ind ZCL
 */
void PilightApsPlugin::handleZclDiscoverAttributesResponse(const deCONZ::ApsDataIndication &ind,
		const deCONZ::ZclFrame &zclframe) {
	char strout[MAX_STR];
	memset(strout, '\0', MAX_STR);
	char strtmp[TMP_STR];
	memset(strtmp, '\0', TMP_STR);
	const char *respvalue = zclframe.payload().data();
	const char *endvalue = respvalue + zclframe.payload().length();

	uint8_t discoveryComplete = *respvalue; //!< A value of 0 indicates that there are some more attributes to be discovered
	int x = snprintf(strout, MAX_STR, "<-ZCL discover attr %s for cluster 0x%04X discoveryComplete = %s\n ",
			getApsIndSrcAddr(ind).c_str(), ind.clusterId(), discoveryComplete ? "Yes" : "No");
	while (respvalue + 3 < endvalue) {
		respvalue++;
		uint16_t *attributeId = (uint16_t *) respvalue;
		respvalue += 2;
		uint8_t typeId = *respvalue;
		snprintf(strtmp, TMP_STR, "<-ZCL discover attr %s 0x%04X 0x%04X 0x%02X %s\n ",
				getApsIndSrcAddr(ind).c_str(), ind.clusterId(), *attributeId, typeId, getAttributeTypeIdName(typeId));
		x += snprintf(&strout[x], MAX_STR-x, strtmp);
	}
	DBG_Printf(DBG_INFO, "%s", strout);
	if (m_isconnected) {
		writeToConnectedClients(strout);
	}
}

/*! Handles a ZCL Server To Client response.
    \param ind ZCL
 */
void PilightApsPlugin::handleZclServerToClientResponse(const deCONZ::ApsDataIndication &ind,
		const deCONZ::ZclFrame &zclframe) {
	char strout[MAX_STR];
	memset(strout, '\0', MAX_STR);
	char strtmp[TMP_STR];
	memset(strtmp, '\0', TMP_STR);
	const char *respvalue = zclframe.payload().data();
	const char *endvalue = respvalue + zclframe.payload().length();

	snprintf(strout, MAX_STR,
			"<-ZCL serverToClient %s %d for cluster 0x%04X ",
			getApsIndSrcAddr(ind).c_str(), ind.srcEndpoint(), ind.clusterId());
	if (zclframe.frameControl() & 0x04) { // manufacturer specific
		snprintf(strtmp, TMP_STR, "manufacturer 0x%04X ", zclframe.manufacturerCode());
		strcat(strout, strtmp);
	}
	while (respvalue < endvalue) {
		snprintf(strtmp, TMP_STR, "%02X ", *respvalue++); // print hex
		strcat(strout, strtmp);
	}
	strcat(strout, "\n");
	DBG_Printf(DBG_INFO, "%s", strout);
	if (m_isconnected) {
		writeToConnectedClients(strout);
	}
}

/*! deCONZ will ask this plugin which features are supported.
    \param feature - feature to be checked
    \return true if supported
 */
bool PilightApsPlugin::hasFeature(Features feature) {
    switch (feature)
    {
    default:
        break;
    }

    return false;
}

/*! Returns the name of this plugin.
 */
const char *PilightApsPlugin::name() {
    return "Pilight APS Plugin";
}

/*! Process incoming TCP request
 */
void PilightApsPlugin::receiveCommand() {
	DBG_Printf(DBG_INFO, "receiveCommand\n");

	QTcpSocket *socket = tcpServer->nextPendingConnection();
	clientConnection.append(socket);

	connect(socket, SIGNAL(readyRead()),
			this, SLOT(readReceivedBytes()));
	connect(socket, SIGNAL(disconnected()),
			this, SLOT(clientDisconnected()));
	connect(socket, SIGNAL(disconnected()),
			socket, SLOT(deleteLater()));

	m_isconnected = true;
}

/*! Client has disconnected
 */
void PilightApsPlugin::clientDisconnected() {
	for (int i = 0; i < clientConnection.size(); i++) {
		if (clientConnection.at(i)->state() != QAbstractSocket::ConnectedState) {
			DBG_Printf(DBG_INFO, "clientDisconnected %d\n", i);
			clientConnection.takeAt(i);
		}
	}
	if (clientConnection.isEmpty()) {
		m_isconnected = false;
	}
}

/*! Read from data from socket
 */
void PilightApsPlugin::readReceivedBytes() {
	deCONZ::Address addr;
	bool result = false;
	char data[MAX_STR];
	memset(data, '\0', MAX_STR);
	DBG_Printf(DBG_INFO, "readReceivedBytes clientConnection.size() = %d \n", clientConnection.size());
	QTcpSocket *socket = static_cast<QTcpSocket *>(sender());
	if (socket->bytesAvailable() > 0) {
		int len = socket->read(data, MAX_STR);
		DBG_Printf(DBG_INFO, "readReceivedBytes %d bytes: %s\n", len, data);
	}
	writeToConnectedClients(data);

	char command[MAX_STR];
	memset(command, '\0', MAX_STR);
	int shortaddr = -1, profile = -1, ep = -1, cluster = -1, attrid = -1, manu = 0;

	if (sscanf(data, "r %x %d %x %x", &shortaddr, &ep, &cluster, &attrid)  == 4) {
		DBG_Printf(DBG_INFO, "read attribute on  %x %d %x %x\n", shortaddr, ep, cluster, attrid);
		result = sendZclReadAttributeRequest(shortaddr, ep, cluster, attrid);
	}
	if (sscanf(data, "zclattr %x %d %x %s", &shortaddr, &ep, &cluster, command)  == 4) {
		DBG_Printf(DBG_INFO, "zclattr on  %x %d %x %s\n", shortaddr, ep, cluster, command);
		addr.setNwk(shortaddr);
		result = sendZclAttributeRequest(shortaddr, ep, cluster, QByteArray::fromHex(command));
	}
	if (sscanf(data, "zclattrmanu %x %d %x %x %s", &shortaddr, &ep, &cluster, &manu, command)  == 5) {
		DBG_Printf(DBG_INFO, "zclattrmanu on  %x %d %x %x %s\n", shortaddr, ep, cluster, manu, command);
		addr.setNwk(shortaddr);
		result = sendZclAttributeManuSpecRequest(shortaddr, ep, cluster, QByteArray::fromHex(command), manu);
	}
	if (sscanf(data, "zclcmd %x %d %x %s", &shortaddr, &ep, &cluster, command)  == 4) {
		DBG_Printf(DBG_INFO, "zclcmd on  %x %d %x %s\n", shortaddr, ep, cluster, command);
		addr.setNwk(shortaddr);
		result = sendZclCmdRequest(addr, ep, cluster, QByteArray::fromHex(command));
	}
	if (sscanf(data, "zclcmdgrp %x %d %x %s", &shortaddr, &ep, &cluster, command)  == 4) {
		DBG_Printf(DBG_INFO, "zclcmdgrp on  %x %d %x %s\n", shortaddr, ep, cluster, command);
		addr.setGroup(shortaddr);
		result = sendZclCmdRequest(addr, ep, cluster, QByteArray::fromHex(command));
	}
	if (sscanf(data, "zclcmdmanu %x %d %x %x %s", &shortaddr, &ep, &cluster, &manu, command)  == 5) {
		DBG_Printf(DBG_INFO, "zclcmdmanu on  %x %d %x %s\n", shortaddr, ep, cluster, command, manu);
		addr.setNwk(shortaddr);
		result = sendZclCmdManuSpecRequest(addr, ep, cluster, QByteArray::fromHex(command), manu);
	}
	if (sscanf(data, "b %x %d %x", &shortaddr, &ep, &cluster)  == 3) {
		DBG_Printf(DBG_INFO, "read basic attributes on %x %d 0x%04X\n", shortaddr, ep, cluster);
		m_readclust = cluster;
	}
	if (sscanf(data, "b %x %d", &shortaddr, &ep)  == 2) {
		DBG_Printf(DBG_INFO, "read basic attributes on %x %d\n", shortaddr, ep);
		m_readattr = true;
		m_shortaddr = shortaddr;
		m_ep = ep;
		m_attrid = 0x0004; // start attribute id 0x0004
		cluster = 0x0000;  // Basic Cluster Id
		attrid = m_attrid;
		result = sendZclReadAttributeRequest(shortaddr, ep, cluster, attrid);
	}
	if (sscanf(data, "m %x %x", &profile, &cluster)  == 2) {
		DBG_Printf(DBG_INFO, "zdp match request on cluster  %x\n", cluster);
		result = sendZdpMatchCmdRequest(profile, cluster);
	}
	if (sscanf(data, "p %x", &shortaddr)  == 1) {
		DBG_Printf(DBG_INFO, "permit join on %x\n", shortaddr);
		result = sendZdpPermitJoinCmdRequest(shortaddr);
	}
	if (sscanf(data, "zdpcmd %x %x %s", &shortaddr, &cluster, command)  == 3) {
		DBG_Printf(DBG_INFO, "ZDP request on 0x%04X, commandId = 0x%04X, payload = %s\n", shortaddr, cluster, command);
		addr.setNwk(shortaddr);
		result = sendZdpCmdRequest(addr, (uint16_t) cluster, QByteArray::fromHex(command));
	}
	if (sscanf(data, "sendtime %x %d", &shortaddr, &ep)  == 2) {
		DBG_Printf(DBG_INFO, "send time to shortaddr = %x, ep = %d\n", shortaddr, ep);
		result = sendZclTimeAttributes(shortaddr, ep);
	}
	if (strstr(data, "help") != NULL) {
		printhelp();
		result = true;
	}

	if (result) {
		sprintf(data, " --> send OK\n");
	} else {
		sprintf(data, " --> send Failed\n");
	}
	writeToConnectedClients(data);
}

/*! Write to connected clients
 */
void PilightApsPlugin::writeToConnectedClients(char *data) {
	for (int i = 0; i < clientConnection.size(); i++) {
		clientConnection.at(i)->write(data);
	}
}

/*! Send ZCL Read Attribute Request
   \return true if request was added to queue
 */
bool PilightApsPlugin::sendZclReadAttributeRequest(int shortaddr, int ep, int cluster, int attrid) {
	QByteArray payload;
	QDataStream zclstream(&payload, QIODevice::WriteOnly);
	zclstream.setByteOrder(QDataStream::LittleEndian);
	zclstream << (qint8) deCONZ::ZclReadAttributesId;
	zclstream << (quint16) attrid; // Attribute ID
	return sendZclAttributeRequest(shortaddr, ep, cluster, payload);
}

//     General ZCL command ids every cluster shall support.
//    enum ZclGeneralCommandId
//    {
//        ZclReadAttributesId               = 0x00,
//        ZclReadAttributesResponseId       = 0x01,
//        ZclWriteAttributesId              = 0x02,
//        ZclWriteAttributesUndividedId     = 0x03,
//        ZclWriteAttributesResponseId      = 0x04,
//        ZclWriteAttributesNoResponseId    = 0x05,
//        ZclConfigureReportingId           = 0x06,
//        ZclConfigureReportingResponseId   = 0x07,
//        ZclReadReportingConfigId          = 0x08,
//        ZclReadReportingConfigResponseId  = 0x09,
//        ZclReportAttributesId             = 0x0a,
//        ZclDefaultResponseId              = 0x0b,
//        ZclDiscoverAttributesId           = 0x0c,
//        ZclDiscoverAttributesResponseId   = 0x0d
//    };

/*! Send ZCL Attribute Request ProfileCommand (Read / Write / Report configuration)
 *  with or without Manufacturer Specific Attributes
   \return true if request was added to queue
 */
bool PilightApsPlugin::sendZclAttributeGenericRequest(int shortaddr, int ep, int cluster, QByteArray payload, int manu) {
    char strtmp[TMP_STR];
    int rc = -1;

	if (m_apsCtrl->networkState() != deCONZ::InNetwork) {
		return false;
	}
	DBG_Printf(DBG_INFO, "-------------> sendZclReadAttributeRequest\n");

	// generate and remember a new ZDP transaction sequence number
	m_zclSeq = (uint8_t) qrand();

	uint8_t zclattrcmd = payload.at(0); // first byte is commandId
	payload.remove(0,1);                // remaining bytes are payload

	deCONZ::ZclFrame zclReq;
	uint8_t framecontrol = deCONZ::ZclFCProfileCommand;
	if (manu > 0) {
		framecontrol |= deCONZ::ZclFCManufacturerSpecific;
		DBG_Printf(DBG_INFO, "framecontrol = %02x\n", framecontrol);
		zclReq.setManufacturerCode(manu);

	}
	zclReq.setFrameControl(framecontrol);
	zclReq.setSequenceNumber(m_zclSeq);
	zclReq.setCommandId(zclattrcmd); // for example deCONZ::ZclReadAttributesId

	// prepare ZCL payload
	zclReq.setPayload(payload);

	deCONZ::ApsDataRequest apsReq;

	// set destination addressing
	apsReq.setDstAddressMode(deCONZ::ApsNwkAddress);
	apsReq.dstAddress().setNwk(shortaddr);
	apsReq.setDstEndpoint(ep);
	apsReq.setSrcEndpoint(0x01);  // Source Endpoint 0x01
	apsReq.setProfileId(HA_PROFILE_ID);
	apsReq.setClusterId(cluster);

	// prepare APS payload
	QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
	stream.setByteOrder(QDataStream::LittleEndian);

	zclReq.writeToStream(stream);

	unsigned int length = apsReq.asdu().length();
	int strlen = length * 3;  // print each byte in hex (2 bytes) and white space
	char rawstr[strlen+1];
	memset(rawstr, '\0', strlen + 1);

	QDataStream asdustream(apsReq.asdu());
	asdustream.setByteOrder(QDataStream::LittleEndian);
	uint8_t asdudata;
	while (!asdustream.atEnd()) {
		asdustream >> asdudata;
		char rawtmp[4];
		memset(rawtmp, '\0', 4);
		snprintf(rawtmp, 4, "%02X ", asdudata);
		strcat(rawstr, rawtmp);
	}
	DBG_Printf(DBG_INFO, "APS Req id 0x%02X %d profile 0x%04X %s, shortaddr = 0x%04X, ep = %d\n",
			apsReq.id(), length, apsReq.profileId(), rawstr, shortaddr, ep);

	if (m_apsCtrl && ((rc = m_apsCtrl->apsdeDataRequest(apsReq)) == deCONZ::Success)) {
		// remember request
		m_apsReqQueue.push_back(apsReq);
		return true;
    } else {
    	//    Success = 0,       //!< Success
    	//    ErrorNotConnected, //!< Not connected (device or network)
    	//    ErrorQueueIsFull,  //!< Queue is full
    	//    ErrorNodeIsZombie, //!< Node is not reachable
    	//    ErrorNotFound      //!< Not found
    	switch(rc) {
    		case -1: snprintf(strtmp, TMP_STR, "controller not connected "); break;
    		case deCONZ::ErrorNotConnected: snprintf(strtmp, TMP_STR, " ErrorNotConnected "); break;
    		case deCONZ::ErrorQueueIsFull: snprintf(strtmp, TMP_STR, " ErrorQueueIsFull "); break;
    		case deCONZ::ErrorNodeIsZombie: snprintf(strtmp, TMP_STR, " ErrorNodeIsZombie "); break;
    		case deCONZ::ErrorNotFound: snprintf(strtmp, TMP_STR, " ErrorNotFound "); break;
    		default: snprintf(strtmp, TMP_STR, " Error %d ", rc);

    	}
    	writeToConnectedClients(strtmp);
    }

	return false;
}

/*! Send ZCL Attribute Request ProfileCommand (Read / Write / Report configuration)
   \return true if request was added to queue
 */
bool PilightApsPlugin::sendZclAttributeRequest(int shortaddr, int ep, int cluster, QByteArray payload) {
	return sendZclAttributeGenericRequest(shortaddr, ep, cluster, payload, 0);
}

/*! Send ZCL Attribute Manufacturer Specific Request ProfileCommand (Read / Write / Report configuration)
   \return true if request was added to queue
 */
bool PilightApsPlugin::sendZclAttributeManuSpecRequest(int shortaddr, int ep, int cluster, QByteArray payload, int manu) {
	return sendZclAttributeGenericRequest(shortaddr, ep, cluster, payload, manu);
}

/*! Send ZCL Request ClusterCommand (Send Command)
   \return true if request was added to queue
 */
bool PilightApsPlugin::sendZclCmdGenericRequest(deCONZ::Address dstAddress,
		uint8_t ep, uint16_t cluster, QByteArray payload, int manu) {
    char strtmp[TMP_STR];
    int rc = -1;

	if (m_apsCtrl->networkState() != deCONZ::InNetwork) {
		return false;
	}
	DBG_Printf(DBG_INFO, "-------------> sendZclCmdRequest\n");

	uint8_t commandId = payload.at(0); // zcl command
	payload.remove(0,1);

	deCONZ::ZclFrame zclReq;
	uint8_t framecontrol = deCONZ::ZclFCClusterCommand|deCONZ::ZclFCEnableDefaultResponse;
	if (manu > 0) {
		framecontrol |= deCONZ::ZclFCManufacturerSpecific;
		DBG_Printf(DBG_INFO, "framecontrol = %02x\n", framecontrol);
		zclReq.setManufacturerCode(manu);
	}
	zclReq.setFrameControl(framecontrol);
	zclReq.setSequenceNumber((uint8_t) qrand());
	zclReq.setCommandId(commandId);
	zclReq.setPayload(payload);

	deCONZ::ApsDataRequest apsReq;

	// set destination addressing
    if (dstAddress.hasNwk()) {
    	apsReq.setDstAddressMode(deCONZ::ApsNwkAddress);
    	apsReq.dstAddress().setNwk(dstAddress.nwk());
    } else if (dstAddress.hasGroup()) {
    	apsReq.setDstAddressMode(deCONZ::ApsGroupAddress);
    	apsReq.dstAddress().setGroup(dstAddress.group());
    } else if (dstAddress.hasExt()) {
    	apsReq.setDstAddressMode(deCONZ::ApsExtAddress);
    	apsReq.dstAddress().setExt(dstAddress.ext());
    }
	apsReq.setDstEndpoint(ep);
	apsReq.setSrcEndpoint(0x1 /*ZDO_ENDPOINT*/);
	apsReq.setProfileId(HA_PROFILE_ID);
	apsReq.setClusterId(cluster);

	// prepare payload
	QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
	stream.setByteOrder(QDataStream::LittleEndian);
	zclReq.writeToStream(stream);

	unsigned int length = apsReq.asdu().length();
	int strlen = length * 3;  // print each byte in hex (2 bytes) and white space
	char rawstr[strlen+1];
	memset(rawstr, '\0', strlen + 1);

	QDataStream asdustream(apsReq.asdu());
	asdustream.setByteOrder(QDataStream::LittleEndian);
	uint8_t asdudata;
	while (!asdustream.atEnd()) {
		asdustream >> asdudata;
		char rawtmp[4];
		memset(rawtmp, '\0', 4);
		snprintf(rawtmp, 4, "%02X ", asdudata);
		strcat(rawstr, rawtmp);
	}
	DBG_Printf(DBG_INFO, "APS Req id 0x%02X %d: %s. dstAddr = 0x%04X dstEp = 0x%02X done\n",
			apsReq.id(), length, rawstr, dstAddress.nwk(), ep);

	if (m_apsCtrl && ((rc = m_apsCtrl->apsdeDataRequest(apsReq)) == deCONZ::Success)) {
		// remember request
		m_apsReqQueue.push_back(apsReq);
		return true;
    } else {
    	//    Success = 0,       //!< Success
    	//    ErrorNotConnected, //!< Not connected (device or network)
    	//    ErrorQueueIsFull,  //!< Queue is full
    	//    ErrorNodeIsZombie, //!< Node is not reachable
    	//    ErrorNotFound      //!< Not found
    	switch(rc) {
    		case -1: snprintf(strtmp, TMP_STR, "controller not connected "); break;
    		case deCONZ::ErrorNotConnected: snprintf(strtmp, TMP_STR, " ErrorNotConnected "); break;
    		case deCONZ::ErrorQueueIsFull: snprintf(strtmp, TMP_STR, " ErrorQueueIsFull "); break;
    		case deCONZ::ErrorNodeIsZombie: snprintf(strtmp, TMP_STR, " ErrorNodeIsZombie "); break;
    		case deCONZ::ErrorNotFound: snprintf(strtmp, TMP_STR, " ErrorNotFound "); break;
    		default: snprintf(strtmp, TMP_STR, " Error %d ", rc);

    	}
    	writeToConnectedClients(strtmp);
    }

	return false;
}

/*! Send ZCL Request ClusterCommand (Send Command)
   \return true if request was added to queue
 */
bool PilightApsPlugin::sendZclCmdRequest(deCONZ::Address dstAddress,
		uint8_t ep, uint16_t cluster, QByteArray payload) {
	return sendZclCmdGenericRequest(dstAddress, ep, cluster, payload, 0);
}

/*! Send ZCL Request ClusterCommand (Send Command) Manufacturer Specific
   \return true if request was added to queue
 */
bool PilightApsPlugin::sendZclCmdManuSpecRequest(deCONZ::Address dstAddress,
		uint8_t ep, uint16_t cluster, QByteArray payload, int manu) {
	return sendZclCmdGenericRequest(dstAddress, ep, cluster, payload, manu);
}

/*! Send ZCL Default Response
   \return true if request was added to queue
 */
bool PilightApsPlugin::sendZclDefaultResponse(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame, quint8 status) {
    char strtmp[TMP_STR];
    int rc = -1;
    deCONZ::ApsDataRequest apsReq;

    // ZDP Header
    apsReq.dstAddress() = ind.srcAddress();
    apsReq.setDstAddressMode(ind.srcAddressMode());
    apsReq.setDstEndpoint(ind.srcEndpoint());
    apsReq.setSrcEndpoint(ind.dstEndpoint());
    apsReq.setProfileId(ind.profileId());
    apsReq.setRadius(0);
    apsReq.setClusterId(ind.clusterId());

    deCONZ::ZclFrame outZclFrame;
    outZclFrame.setSequenceNumber(zclFrame.sequenceNumber());
    outZclFrame.setCommandId(deCONZ::ZclDefaultResponseId);
    outZclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // ZCL payload
        QDataStream stream(&outZclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << zclFrame.commandId();
        stream << status;
    }

    { // ZCL frame
        QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        outZclFrame.writeToStream(stream);
    }

    snprintf(strtmp, TMP_STR, "->ZCL default response seq id 0x%02X command id 0x%02X, status 0x%02X\n",
    		zclFrame.sequenceNumber(), zclFrame.commandId(), status);
    DBG_Printf(DBG_INFO, strtmp);

    if (m_apsCtrl && ((rc = m_apsCtrl->apsdeDataRequest(apsReq)) == deCONZ::Success)) {
        // remember request
        m_apsReqQueue.push_back(apsReq);
        writeToConnectedClients(strtmp);
        return true;
    } else {
    	//    Success = 0,       //!< Success
    	//    ErrorNotConnected, //!< Not connected (device or network)
    	//    ErrorQueueIsFull,  //!< Queue is full
    	//    ErrorNodeIsZombie, //!< Node is not reachable
    	//    ErrorNotFound      //!< Not found
    	switch(rc) {
    		case -1: snprintf(strtmp, TMP_STR, "controller not connected "); break;
    		case deCONZ::ErrorNotConnected: snprintf(strtmp, TMP_STR, " ErrorNotConnected "); break;
    		case deCONZ::ErrorQueueIsFull: snprintf(strtmp, TMP_STR, " ErrorQueueIsFull "); break;
    		case deCONZ::ErrorNodeIsZombie: snprintf(strtmp, TMP_STR, " ErrorNodeIsZombie "); break;
    		case deCONZ::ErrorNotFound: snprintf(strtmp, TMP_STR, " ErrorNotFound "); break;
    		default: snprintf(strtmp, TMP_STR, " Error %d ", rc);
    	}
    	writeToConnectedClients(strtmp);
    }
    return false;

}

/*! Send ZDP Match Request
   \return true if request was added to queue
 */
bool PilightApsPlugin::sendZdpMatchCmdRequest(int profile, int cluster) {
	deCONZ::Address dstAddress;
	dstAddress.setNwk(deCONZ::BroadcastRxOnWhenIdle);
	uint16_t zdpcommand = ZDP_MATCH_DESCRIPTOR_CLID;
	QByteArray asdu;
	QDataStream stream(&asdu, QIODevice::WriteOnly);
	stream.setByteOrder(QDataStream::LittleEndian);

	//	write payload according to ZigBee specification (2.4.3.1.7 Match_Descr_req)
	//	NOTE: explicit castings ensure correct size of the fields
	stream << (quint16) deCONZ::BroadcastRxOnWhenIdle; // NWKAddrOfInterest
	stream << (quint16) profile; //ZLL_PROFILE_ID or HA_PROFILE_ID;
	stream << (quint8) 0x01; // NumInClusters
	stream << (quint16) cluster; // OnOff ClusterID
	stream << (quint8) 0x00; // NumOutClusters

	bool result = sendZdpCmdRequest(dstAddress, zdpcommand, asdu);

	if (result) {
		m_profile = profile;
		m_cluster = cluster;
		m_zdpmatchreq = true;
	}
	return result;
}

/*! Send ZDP Permit Join Request
   \return true if request was added to queue
 */
bool PilightApsPlugin::sendZdpPermitJoinCmdRequest(int shortaddr) {
	deCONZ::Address dstAddress;
	dstAddress.setNwk(shortaddr);
	uint16_t zdpcommand = ZDP_MGMT_PERMIT_JOINING_REQ_CLID; //ZDP_MATCH_DESCRIPTOR_CLID;
	QByteArray asdu;
	QDataStream stream(&asdu, QIODevice::WriteOnly);
	stream.setByteOrder(QDataStream::LittleEndian);

	//	write payload according to ZigBee specification (2.4.3.3.7 Mgmt_Permit_Joining_req)
	//	NOTE: explicit castings ensure correct size of the fields
	stream << (quint8) 180;   // Permit joining for 180 seconds
	stream << (quint8) 0x00;  // TC_Significance always 0x00 or 0x01

	DBG_Printf(DBG_INFO, "Permit Join reguest payload = %s\n", asdu.toHex().data());
	m_apsCtrl->setPermitJoin(180); // set permitJoin on coordinator

	return sendZdpCmdRequest(dstAddress, zdpcommand, asdu);
}


/*! Send ZDP Request (for example Zdp_match_req)
   \return true if request was added to queue
 */
bool PilightApsPlugin::sendZdpCmdRequest(deCONZ::Address dstAddress, uint16_t zdpcommand, QByteArray asdu) {
    deCONZ::ApsDataRequest apsReq;
    char strtmp[TMP_STR];
    int rc = -1;

    // set destination addressing
    if (dstAddress.hasNwk()) {
    	apsReq.dstAddress().setNwk(dstAddress.nwk() /*deCONZ::BroadcastRxOnWhenIdle*/);
       	apsReq.setDstAddressMode(deCONZ::ApsNwkAddress);
    } else if (dstAddress.hasGroup()) {
    	apsReq.dstAddress().setGroup(dstAddress.group());
       	apsReq.setDstAddressMode(deCONZ::ApsGroupAddress);
    } else if (dstAddress.hasExt()) {
    	apsReq.dstAddress().setExt(dstAddress.ext());
       	apsReq.setDstAddressMode(deCONZ::ApsExtAddress);
    }
    apsReq.setDstEndpoint(ZDO_ENDPOINT);
    apsReq.setSrcEndpoint(ZDO_ENDPOINT);
    apsReq.setProfileId(ZDP_PROFILE_ID);
    apsReq.setClusterId(zdpcommand);

	char zdpseq = (char) qrand(); // ZDP transaction sequence number
	asdu.prepend(zdpseq);         // insert at beginning
    apsReq.setAsdu(asdu);
    DBG_Printf(DBG_INFO, "APS ZDP Req id 0x%02X %s\n", apsReq.id(), asdu.toHex().toUpper().data());

    if (m_apsCtrl && ((rc = m_apsCtrl->apsdeDataRequest(apsReq)) == deCONZ::Success)) {
        // remember request
        m_apsReqQueue.push_back(apsReq);
        return true;
    } else {
    	//    Success = 0,       //!< Success
    	//    ErrorNotConnected, //!< Not connected (device or network)
    	//    ErrorQueueIsFull,  //!< Queue is full
    	//    ErrorNodeIsZombie, //!< Node is not reachable
    	//    ErrorNotFound      //!< Not found
    	switch(rc) {
    		case -1: snprintf(strtmp, TMP_STR, "controller not connected "); break;
    		case deCONZ::ErrorNotConnected: snprintf(strtmp, TMP_STR, " ErrorNotConnected "); break;
    		case deCONZ::ErrorQueueIsFull: snprintf(strtmp, TMP_STR, " ErrorQueueIsFull "); break;
    		case deCONZ::ErrorNodeIsZombie: snprintf(strtmp, TMP_STR, " ErrorNodeIsZombie "); break;
    		case deCONZ::ErrorNotFound: snprintf(strtmp, TMP_STR, " ErrorNotFound "); break;
    		default: snprintf(strtmp, TMP_STR, " Error %d ", rc);
    	}
    	writeToConnectedClients(strtmp);
    }
    return false;
}

/*! Send ZCL Write Attributes to Time_Cluster 0x000A
 *  Time Cluster Attributes in 3.12.2.2
 *  write Attributes 0x0000 Time, 0x0001 TimeStatus (synchronized)
 *  TimeZone 0x0002 , DstStart 0x0003, DstEnd 0x0004, DstShift 0x0005
 *  \return true if request was added to queue
 */
bool PilightApsPlugin::sendZclTimeAttributes(int shortaddr, int ep) {
	time_t time_dst_start, time_dst_end, time_now, time_utc_2000;
	int time_dst_shift, time_gmt_offset;
	int x;
	char data[MAX_STR];
	memset(data, '\0', MAX_STR);

	time_now = time(NULL);
	time_utc_2000 = 946684800; // on 1. Jan 2000 UTC since epoch
	get_dst(&time_dst_start, &time_dst_end, &time_dst_shift);
	time_gmt_offset = get_gmtdiff();

	DBG_Printf(DBG_INFO, "<- time_now       %ld, %s", (long) time_now, ctime(&time_now));
	DBG_Printf(DBG_INFO, "<- time_dst_start %ld, %s", (long) time_dst_start, ctime(&time_dst_start));
	DBG_Printf(DBG_INFO, "<- time_dst_end   %ld, %s", (long) time_dst_end, ctime(&time_dst_end));
	DBG_Printf(DBG_INFO, "<- time_dst_shift %d\n", time_dst_shift);
	DBG_Printf(DBG_INFO, "<- time_zone      %d\n", time_gmt_offset);
	x = sprintf(data, "<- time_now       %ld, %s", (long) time_now, ctime(&time_now) );
	x += sprintf(&data[x], "<- time_dst_start %ld, %s", (long) time_dst_start, ctime(&time_dst_start));
	x += sprintf(&data[x], "<- time_dst_end   %ld, %s", (long) time_dst_end, ctime(&time_dst_end));
	x += sprintf(&data[x], "<- time_dst_shift %d\n", time_dst_shift);
	x += sprintf(&data[x], "<- time_zone      %d\n", time_gmt_offset);
	writeToConnectedClients(data);

	// convert time to epoch seconds since 1. Jan 2000 UTC
	uint32_t timenow = (uint32_t) difftime(time_now, time_utc_2000);
	uint8_t timestatus = 0x02; // Synchronized bit-1
	int32_t timezone = (int32_t) time_gmt_offset;
	uint32_t dststart = (uint32_t) difftime(time_dst_start, time_utc_2000);
	uint32_t dstend = (uint32_t) difftime(time_dst_end, time_utc_2000);
	int32_t dstshift = (int32_t) time_dst_shift;

	// generate and remember a new ZDP transaction sequence number
	m_zclSeq = (uint8_t) qrand();

	int cluster = 0x000A;  // Time cluster
	uint8_t zclattrcmd = deCONZ::ZclWriteAttributesId;

	QByteArray zclpayload;
	QDataStream zclstream(&zclpayload, QIODevice::WriteOnly);
	zclstream.setByteOrder(QDataStream::LittleEndian);

	zclstream << (quint8) zclattrcmd; // first byte is command id

	zclstream << (quint16) 0x0000;  // Attribute ID
	zclstream << (quint8) 0xE2;     // Attribute Type 0xE2 UTC Time
	zclstream << (quint32) timenow; // Time

	zclstream << (quint16) 0x0001;  // Attribute ID
	zclstream << (quint8) 0x18;     // Attribute Type 0x18 bit8
	zclstream << (quint8) timestatus; // Time	Status

	zclstream << (quint16) 0x0002;  // Attribute ID
	zclstream << (quint8) 0x2B;     // Attribute Type 0x2B
	zclstream << (qint32) timezone; // Timezone

	zclstream << (quint16) 0x0003;  // Attribute ID
	zclstream << (quint8) 0x23;     // Attribute Type 0x23
	zclstream << (quint32) dststart; // Daylight Saving Start

	zclstream << (quint16) 0x0004;  // Attribute ID
	zclstream << (quint8) 0x23;     // Attribute Type 0x23
	zclstream << (quint32) dstend;  // Daylight Saving End

	zclstream << (quint16) 0x0005;  // Attribute ID
	zclstream << (quint8) 0x2B;     // Attribute Type 0x2B
	zclstream << (qint32) dstshift; // Daylight Saving Time Shift

	return sendZclAttributeRequest(shortaddr, ep, cluster, zclpayload);
}

/*! get dst daylight saving start, end, shift
 *
 */
void PilightApsPlugin::get_dst_start_end(int finddst /* 0 or 1 */, time_t *time_dst_start, time_t *time_dst_end, int *time_dst_shift) {
	struct tm tm1, tm2;
	time_t tstart, time_dst_start_end;
	int i_start, i, j, k;
	i_start = 1;
	tstart = time(NULL);
	tm1 = *gmtime(&tstart);
	tm1.tm_mon = tm1.tm_hour = tm1.tm_min = tm1.tm_sec = 0; // reset to Januar midnight
	if (finddst == 0) {
		tstart = *time_dst_start;
		tm1 = *gmtime(&tstart);
		i_start = tm1.tm_mday + 1;
	}
	for (i = i_start; i < 364; i++) {  // step over every day
		tm1.tm_mday = i;
		tm2 = tm1;
		time_dst_start_end = mktime(&tm2);
		if (tm2.tm_isdst == finddst /* 1 or 0*/) {  // found the day after dst start/end
			tm1 = tm2;
			for (j = 0; j < 24; j++) {
				tm1.tm_hour--;
				tm2 = tm1;
				time_dst_start_end = mktime(&tm2);
				if (tm2.tm_isdst == (1 - finddst) /* 0 or 1*/) {  // found the hour before dst start/end
					for (k = 0; k < 60; k++) {
						tm1.tm_min++;
						tm2 = tm1;
						time_dst_start_end = mktime(&tm2);
						if (tm2.tm_isdst == finddst /* 1 or 0*/) {  // found the first minute with dst start/end
							if (finddst == 1) {
								*time_dst_start = time_dst_start_end;
								struct tm time_gmt_tm = *gmtime(&time_dst_start_end);
								struct tm time_local_tm = *localtime(&time_dst_start_end);
								time_t time_gmt_t = mktime(&time_gmt_tm);
								time_t time_local_t = mktime(&time_local_tm);
								*time_dst_shift = (int) difftime(time_local_t, time_gmt_t);
							} else {
								*time_dst_end = time_dst_start_end;
							}
							break;
						}
					}
					break;
				}
			}
			break;
		}
	}
}

/*! get dst daylight saving start, end, shift
 *
 */
void PilightApsPlugin::get_dst(time_t *time_dst_start, time_t *time_dst_end, int *time_dst_shift) {
	get_dst_start_end(1, time_dst_start, time_dst_end, time_dst_shift);
	get_dst_start_end(0, time_dst_start, time_dst_end, time_dst_shift);
}

/*! epoch time since 1. Jan 2000
 *
 */
long PilightApsPlugin::get_epoch_since_2000(time_t tinput) {
	time_t time_utc_on_2000 = 946684800; // 1. Jan 2000 UTC seconds since epoch
	return (long) difftime(tinput, time_utc_on_2000);
}

/*! get gmt offset to local timezone
 *
 */
int PilightApsPlugin::get_gmtdiff() {
	struct tm tm1, tm2;
	time_t t1, t2;
	time_t tnow = time(NULL);
	tm1 = *gmtime(&tnow);
	tm2 = *localtime(&tnow);
	t1 = mktime(&tm1);
	t2 = mktime(&tm2);
	return (int) difftime(t2, t1);
}

/*! print help
 *
 */
void PilightApsPlugin::printhelp() {
	char data[MAX_STR];
	int x = 0;
	x += snprintf(&data[x], MAX_STR - x, "r <shortaddr> <ep> <cluster> <attrid>       \tread attributes\n");
	x += snprintf(&data[x], MAX_STR - x, "b <shortaddr> <ep>                          \tread basic attributes\n");
	x += snprintf(&data[x], MAX_STR - x, "b <shortaddr> <ep> <cluster>                \tread basic attributes on cluster\n");
	x += snprintf(&data[x], MAX_STR - x, "m <profile> <cluster>                       \tsend match descriptor request (discover cluster)\n");
	x += snprintf(&data[x], MAX_STR - x, "p <shortaddr>                               \tpermit Joining on device (coordinator = 0)\n");
	x += snprintf(&data[x], MAX_STR - x, "zclattr <shortaddr> <ep> <cluster> <command>\tsend ZCL attribute request\n");
	x += snprintf(&data[x], MAX_STR - x, "zclcmd <shortaddr> <ep> <cluster> <command>\tsend ZCL command request\n");
	x += snprintf(&data[x], MAX_STR - x, "zclcmdgrp <groupaddr> <ep> <cluster> <command>\tsend ZCL command request to group\n");
	x += snprintf(&data[x], MAX_STR - x, "zdpcmd <shortaddr> <cluster> <command>      \tsend ZDP command request\n");
	x += snprintf(&data[x], MAX_STR - x, "zclattrmanu <shortaddr> <ep> <cluster> <manufacturer id> <command>\tsend ZCL attribute request manufacturer specific\n");
	x += snprintf(&data[x], MAX_STR - x, "zclcmdmanu <shortaddr> <ep> <cluster> <manufacturer id> <command>\tsend ZCL command request manufacturer specific\n");

	writeToConnectedClients(data);
}
#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(pilight_plugin, PilightApsPlugin)
#endif
