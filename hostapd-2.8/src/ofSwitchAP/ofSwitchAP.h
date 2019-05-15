/*
 * ofSwitchAP.h
 *
 *  Created on: Nov 8, 2015
 *      Author: arkadeep
 */

#ifndef OFSWITCHAP_H_
#define OFSWITCHAP_H_

#include "openflow.h"

#define AP_EXPERIMENTER_ID 0x00000001

#define WLAN_SUPP_RATES_MAX 32

enum exp_type
{
    WLAN_AUTH_REQ,
    WLAN_AUTH_RESP,
    WLAN_AUTH_ERROR,

    WLAN_DEAUTH_REQ,

    WLAN_ASSOC_REQ,
    WLAN_ASSOC_RESP,
    WLAN_ASSOC_ERROR,

    WLAN_REASSOC_REQ,
    WLAN_REASSOC_RESP,
    WLAN_REASSOC_ERROR,

    WLAN_DISASSOC_REQ,

    WLAN_BSSID,

    WLAN_PROBE,

    WLAN_ADD_STA,

    WLAN_REC_POW,

    WLAN_LAST_REC_POW,

    WLAN_AP_REC_POW,

    WLAN_UPDATE_STA_ADD,

    WLAN_REMOVE_STA,

	WLAN_NAT_UPDATE,
	WLAN_NAT_ADD,
	WLAN_PORT_TRANSLATION
};


struct ofp_wlan_header {
    struct ofp_header header;
    uint32_t vendor;            /* AP_EXPERIMENTER_ID. */
    uint32_t subtype;           /* One of WLAN_* above. */
};
OFP_ASSERT(sizeof(struct ofp_wlan_header) == sizeof(struct ofp_vendor_header) + 4);

struct ofp_wlan_msg {
    uint8_t staMacAddress[OFP_ETH_ALEN]; /* STA MAC address. */
    uint8_t apMacAddress[OFP_ETH_ALEN]; /* AP MAC address. */
};
OFP_ASSERT(sizeof(struct ofp_wlan_msg) == 12);

struct ofp_wlan_auth_msg {
    uint8_t staMacAddress[OFP_ETH_ALEN]; /* STA MAC address. */
    uint8_t apMacAddress[OFP_ETH_ALEN]; /* AP MAC address. */
    uint16_t auth_alg;
};
OFP_ASSERT(sizeof(struct ofp_wlan_auth_msg) == 14);

struct ofp_wlan_assoc_msg {
    uint8_t staMacAddress[6]; /* STA MAC address. */
    uint8_t apMacAddress[6]; /* AP MAC address. */
    uint16_t aid;
    uint16_t capability;
    uint16_t listen_interval;
    uint8_t pad1[2];
    uint8_t supported_rates[32];
    uint32_t supported_rates_len;
    uint8_t qosinfo;
    uint8_t pad2[3];
};
OFP_ASSERT(sizeof(struct ofp_wlan_assoc_msg) == 60);

struct ofp_wlan_sta {
    uint8_t staMacAddress[OFP_ETH_ALEN]; /* STA MAC address. */
};
OFP_ASSERT(sizeof(struct ofp_wlan_sta) == 6);

struct ofp_wlan_add_update_msg {
    uint8_t staMacAddress[6]; /* STA MAC address. */
    uint16_t auth_alg;
    uint16_t aid;
    uint16_t capability;
    uint16_t listen_interval;
    uint8_t pad1[2];
    uint8_t supported_rates[32];
    uint32_t supported_rates_len;
    uint8_t qosinfo;
    uint8_t pad2[3];
};
OFP_ASSERT(sizeof(struct ofp_wlan_add_update_msg) == 56);

struct ofp_wlan_rssi {
    uint8_t staMacAddress[OFP_ETH_ALEN]; /* STA MAC address. */
    uint8_t apMacAddress[OFP_ETH_ALEN]; /* AP MAC address. */
    uint8_t rssi; /* Received signal strength of data packet. */
};
OFP_ASSERT(sizeof(struct ofp_wlan_rssi) == 13);

struct ofp_action_iph {
	uint32_t subtype;
	uint32_t tunnelEntry;
	uint32_t tunnelExit;
};
OFP_ASSERT(sizeof(struct ofp_action_iph) == 12);

enum ofp_vendor_subtype {
    OFPAT_EXPERIMENTER_PUSH_IPH,			/* Push the outer IP tag. */
    OFPAT_EXPERIMENTER_POP_IPH				/* Pop the outer IP tag. */
};

struct ofp_nat_entry {
    uint8_t staMacAddress[6]; /* STA MAC address. */
    uint8_t pad[2];
	uint32_t nw_src;           /* IP source address. */
	uint32_t nw_dst;           /* IP destination address. */
	uint16_t tp_src;           /* TCP/UDP source port. */
	uint16_t tp_dst;           /* TCP/UDP destination port. */
};
OFP_ASSERT(sizeof(struct ofp_nat_entry) == 20);

struct ofp_nat_port_entry {
    uint8_t staMacAddress[6]; /* STA MAC address. */
    uint8_t pad[2];
	uint32_t nw_src;           /* IP source address. */
	uint32_t nw_dst;           /* IP destination address. */
	uint16_t tp_src;           /* TCP/UDP source port. */
	uint16_t tp_dst;           /* TCP/UDP destination port. */
	uint16_t tp_new;
    uint8_t pad2[2];
};
OFP_ASSERT(sizeof(struct ofp_nat_port_entry) == 24);

#endif /* OFSWITCHAP_H_ */
