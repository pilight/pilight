/*
 * pilight_plugin.h
 *
 *  Created on: Dec 18, 2016
 *      Author: ma-ca
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef PILIGHT_PLUGIN_H_
#define PILIGHT_PLUGIN_H_

#include <QObject>
#include <list>
#include <deconz.h>

#if DECONZ_LIB_VERSION < 0x010100
  #error "The basic aps plugin requires at least deCONZ library version 1.1.0."
#endif

class QTcpServer;

/*! \class PilightApsPlugin
    Plugin to pilight support for sending and receiving APS frames via
    APSDE-DATA.request, APSDE-DATA.confirm and APSDE-DATA.indication primitives.
    The plugin opens a TCP socket on port 5008

 */
class PilightApsPlugin : public QObject, public deCONZ::NodeInterface {
    Q_OBJECT
    Q_INTERFACES(deCONZ::NodeInterface)
#if QT_VERSION >= 0x050000
    Q_PLUGIN_METADATA(IID "pilightPlugin")
#endif

public:
    explicit PilightApsPlugin(QObject *parent = 0);
    ~PilightApsPlugin();
    // node interface
    const char *name();
    bool hasFeature(Features feature);

public Q_SLOTS:
    void apsdeDataIndication(const deCONZ::ApsDataIndication &ind);
    void apsdeDataConfirm(const deCONZ::ApsDataConfirm &conf);
    std::string getApsIndSrcAddr(const deCONZ::ApsDataIndication &ind);
    const char *getClusterName(uint16_t clusterid);
    const char *getAttributeTypeIdName(uint8_t attrtypeid);
    void handleZdpSimpleResponse(const deCONZ::ApsDataIndication &ind);
    void handleZdpActiveEpResponse(const deCONZ::ApsDataIndication &ind);
    void handleZdpMatchResponse(const deCONZ::ApsDataIndication &ind);
    void handleZdpLqiResponse(const deCONZ::ApsDataIndication &ind);
    void handleZdpBindResponse(const deCONZ::ApsDataIndication &ind);
    void handleZdpPowerResponse(const deCONZ::ApsDataIndication &ind);
    void handleZdpNwkAddrResponse(const deCONZ::ApsDataIndication &ind);
    void handleZclReadAttrbuteResponse(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclframe);
    void handleZclReportConfigResponse(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclframe);
    void handleZclReportAttributeResponse(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclframe);
    void handleZclDiscoverAttributesResponse(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclframe);
    void handleZclServerToClientResponse(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclframe);
    void clientDisconnected();
    void receiveCommand();
    void readReceivedBytes();
    void writeToConnectedClients(char *data);
    bool sendZclReadAttributeRequest(int shortaddr, int ep, int cluster, int attrid);
    bool sendZclAttributeGenericRequest(int shortaddr, int ep, int cluster, QByteArray payload, int manu);
    bool sendZclAttributeRequest(int shortaddr,	int ep, int cluster, QByteArray payload);
    bool sendZclAttributeManuSpecRequest(int shortaddr, int ep, int cluster, QByteArray payload, int manu);
    bool sendZclCmdGenericRequest(deCONZ::Address dstAddress, uint8_t ep, uint16_t cluster, QByteArray payload, int manu);
    bool sendZclCmdRequest(deCONZ::Address dstAddress, uint8_t ep, uint16_t cluster, QByteArray payload);
    bool sendZclCmdManuSpecRequest(deCONZ::Address dstAddress, uint8_t ep, uint16_t cluster, QByteArray payload, int manu);
    bool sendZclDefaultResponse(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame, quint8 status);
    bool sendZdpMatchCmdRequest(int profile, int cluster);
    bool sendZdpPermitJoinCmdRequest(int shortaddr);
    bool sendZdpCmdRequest(deCONZ::Address dstAddress, uint16_t cluster, QByteArray asdu);
    bool sendZclTimeAttributes(int shortaddr, int ep);
    void get_dst_start_end(int finddst /* 0 or 1 */, time_t *time_dst_start, time_t *time_dst_end, int *time_dst_shift);
    void get_dst(time_t *time_dst_start, time_t *time_dst_end, int *time_dst_shift);
    long get_epoch_since_2000(time_t tinput);
    int get_gmtdiff();
    void printhelp();

private:
    std::list<deCONZ::ApsDataRequest> m_apsReqQueue; //!< queue of active APS requests
    deCONZ::ApsController *m_apsCtrl; //!< pointer to ApsController instance
    QTcpServer *tcpServer;
    QList<QTcpSocket *> clientConnection;
    quint8 m_zclSeq; //!< ZCL sequence number of Zcl_req
    int m_shortaddr; //!< short address
    int m_ep; //!< endpoint id
    int m_attrid; //!< attribute id
    int m_profile;
    int m_cluster;
    bool m_zdpmatchreq;
    bool m_readattr; //!< read basic cluster attributes
    int m_readclust; //!< read basic attributes on cluster
    bool m_isconnected;
};

#endif /* PILIGHT_PLUGIN_H_ */
