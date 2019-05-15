/*
 * controller_interface.c
 *
 *  Created on: Nov 1, 2015
 *      Author: arkadeep
 */


#include "controller_interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include "openflow.h"
#include "nicira-ext.h"
#include "nicira-new.h"
#include "ofSwitchAP.h"
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <time.h>
#include <ifaddrs.h>

#include "common.h"
#include "common/ieee802_11_defs.h"
#include "ap/sta_info.h"
#include "ap/hostapd.h"
#include "ap/ieee802_11.h"

#include <sys/inotify.h>
#include <limits.h>
#include <stddef.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/netfilter.h>            /* for NF_ACCEPT */

#include <libnetfilter_queue/libnetfilter_queue.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))
#define MAX_ADDR_LEN 32
#define FOLDER "signal"

#define SOCK_ADDR "127.0.0.1"
//#define SOCK_ADDR "10.6.21.107"
#define SOCK_PORT 6653

#define IFACE "wlan0"
#define NATIFACE "eth0"
#define NATSUBNET "192.168.5.0"
#define PORT_TRANSLATION 1
#define MAX_PACKET_SIZE 0xffff

/**
 * Macros to turn a numeric macro into a string literal.  See
 * https://gcc.gnu.org/onlinedocs/cpp/Stringification.html
 */
#define xstr(s) str(s)
#define str(s) #s

#define ARP_CACHE       "/proc/net/arp"
#define ARP_STRING_LEN  1023
#define ARP_BUFFER_LEN  (ARP_STRING_LEN + 1)

/* Format for fscanf() to read the 1st and 4th space-delimited fields */
#define ARP_LINE_FORMAT "%" xstr(ARP_STRING_LEN) "s %*s %*s " \
                        "%" xstr(ARP_STRING_LEN) "s %*s %*s"


#define MAXENTRY 0xff
#define HASH_FUNC(addr) addr[3]


static struct controller_connection *controller;

static struct STAList *staList;

static void controller_conn_open(void);
static void controller_send(void *msg, size_t n);
static void controller_receive(void);
static struct infoSTA *addSTA(uint8_t *addr);
static void *process_sta_data(void *rssi_msg);
static void changeRSSI(struct infoSTA *sta, int rssi);
static void handle_ap_rssi(struct ofp_wlan_sta *ap_RSSI);
static void *create_ofp_msg(size_t size);

struct ip_header_t {
	uint8_t  ver_ihl;  // 4 bits version and 4 bits internet header length
	uint8_t  tos;
	uint16_t total_length;
	uint16_t id;
	uint16_t flags_fo; // 3 bits flags and 13 bits fragment-offset
	uint8_t  ttl;
	uint8_t  protocol;
	uint16_t checksum;
	unsigned char src_addr[4];
	unsigned char dst_addr[4];
};

struct transport_header {
	uint16_t src_port;
	uint16_t dst_port;
};

struct pseudo_header {
	unsigned char src_addr[4];
	unsigned char dst_addr[4];
	uint8_t  zeroes;
	uint8_t  protocol;
	uint16_t length;
};

struct natMap {
	unsigned char src_addr[4];
	unsigned char dst_addr[4];
	uint16_t src_port;
	uint16_t dst_port;
	uint16_t new_port;

	struct natMap *next;
};

struct natList {
	struct natMap **table;
};

static struct natList *entryList;

struct PortData {
	struct nfq_q_handle *qh;
	u_int32_t packet_id;	/* unique ID of packet in queue */
	int ret;
	unsigned char packet[MAX_PACKET_SIZE];

	struct PortData *next;
};

struct PortQueue {
	struct PortData **table;
};

static struct PortQueue *portQueue;

struct natMap *addPortData(struct nfq_q_handle *qh, u_int32_t packet_id,
		int ret, char *data, unsigned char dst_addr[4])
{
	struct PortData *pData;

	pData = calloc(1, sizeof(struct PortData));
	if (pData == NULL) {
		//printf("Calloc failed\n");
		return NULL;
	}
	pData->qh = qh;
	pData->packet_id = packet_id;
	pData->ret = ret;
	memcpy(pData->packet, data, ret);
	pthread_mutex_lock(&controller->mutex_port);
	pData->next = portQueue->table[HASH_FUNC(dst_addr)];
	portQueue->table[HASH_FUNC(dst_addr)] = pData;
	pthread_mutex_unlock(&controller->mutex_port);
	return pData;
}

void *send_nat_entry(void *msg)
{
	pthread_detach(pthread_self());
	struct ofp_nat_port_entry *nat = (struct ofp_nat_port_entry *)msg;
	size_t size = sizeof(struct ofp_wlan_header) + sizeof(struct ofp_nat_port_entry);
	void *experimenter = create_ofp_msg(size);
	if(experimenter == NULL)
	{
		//printf("Unable to handle OF message\n");
		return NULL;
	}
	struct ofp_wlan_header *wlan_header = (struct ofp_wlan_header *)experimenter;
	wlan_header->vendor = htonl(AP_EXPERIMENTER_ID);
	wlan_header->subtype = htonl(WLAN_NAT_UPDATE);
	char *strPtr = experimenter;
	strPtr += sizeof(struct ofp_wlan_header);
	struct ofp_nat_port_entry *nat_entry = (struct ofp_nat_port_entry *)strPtr;
	memcpy(nat_entry->staMacAddress, nat->staMacAddress, 6);
	nat_entry->nw_src = nat->nw_src;
	nat_entry->nw_dst = nat->nw_dst;
	nat_entry->tp_src = nat->tp_src;
	nat_entry->tp_dst = nat->tp_dst;
	controller_send(experimenter, ntohs(wlan_header->header.length));
	free(nat);
	return NULL;
}

void *send_nat_port_entry(void *msg)
{
	pthread_detach(pthread_self());
	struct ofp_nat_port_entry *nat = (struct ofp_nat_port_entry *)msg;
	size_t size = sizeof(struct ofp_wlan_header) + sizeof(struct ofp_nat_port_entry);
	void *experimenter = create_ofp_msg(size);
	if(experimenter == NULL)
	{
		//printf("Unable to handle OF message\n");
		return NULL;
	}
	struct ofp_wlan_header *wlan_header = (struct ofp_wlan_header *)experimenter;
	wlan_header->vendor = htonl(AP_EXPERIMENTER_ID);
	wlan_header->subtype = htonl(WLAN_PORT_TRANSLATION);
	char *strPtr = experimenter;
	strPtr += sizeof(struct ofp_wlan_header);
	struct ofp_nat_port_entry *nat_entry = (struct ofp_nat_port_entry *)strPtr;
	memcpy(nat_entry->staMacAddress, nat->staMacAddress, 6);
	nat_entry->nw_src = nat->nw_src;
	nat_entry->nw_dst = nat->nw_dst;
	nat_entry->tp_src = nat->tp_src;
	nat_entry->tp_dst = nat->tp_dst;
	controller_send(experimenter, ntohs(wlan_header->header.length));
	free(nat);
	return NULL;
}

struct natMap *lookupNatMap(uint16_t src_port, unsigned char dst_addr[4],
		uint16_t dst_port)
{
	struct natMap *nMap;
	pthread_mutex_lock(&controller->mutex_nat);
	nMap = entryList->table[HASH_FUNC(dst_addr)];
	while (nMap != NULL && (memcmp(nMap->dst_addr, dst_addr, 4) != 0
			|| nMap->src_port != src_port || nMap->dst_port != dst_port))
		nMap = nMap->next;
	pthread_mutex_unlock(&controller->mutex_nat);
	return nMap;
}

struct natMap *lookupPortMapQ1(uint16_t new_port, unsigned char dst_addr[4],
		uint16_t dst_port)
{
	struct natMap *nMap;
	pthread_mutex_lock(&controller->mutex_nat);
	nMap = entryList->table[HASH_FUNC(dst_addr)];
	while (nMap != NULL && (memcmp(nMap->dst_addr, dst_addr, 4) != 0
			|| nMap->new_port != new_port || nMap->dst_port != dst_port))
		nMap = nMap->next;
	pthread_mutex_unlock(&controller->mutex_nat);
	return nMap;
}

struct natMap *lookupPortMapQ0(unsigned char src_addr[4], uint16_t src_port,
		unsigned char dst_addr[4], uint16_t dst_port)
{
	struct natMap *nMap;
	pthread_mutex_lock(&controller->mutex_nat);
	nMap = entryList->table[HASH_FUNC(dst_addr)];
	while (nMap != NULL && (memcmp(nMap->src_addr, src_addr, 4) != 0
			|| memcmp(nMap->dst_addr, dst_addr, 4) != 0
			|| nMap->src_port != src_port
			|| nMap->dst_port != dst_port))
		nMap = nMap->next;
	pthread_mutex_unlock(&controller->mutex_nat);
	return nMap;
}

struct natMap *addEntry(uint16_t src_port, unsigned char src_addr[4],
		uint16_t dst_port, unsigned char dst_addr[4])
{
	struct natMap *nMap;
	if(!PORT_TRANSLATION)
		nMap = lookupNatMap(src_port, dst_addr, dst_port);
	else
		nMap = lookupPortMapQ0(src_addr, src_port, dst_addr, dst_port);
	if (nMap)
		return nMap;

	nMap = calloc(1, sizeof(struct natMap));
	if (nMap == NULL) {
		//printf("Calloc failed\n");
		return NULL;
	}
	nMap->src_port = src_port;
	memcpy(nMap->src_addr, src_addr, 4);
	nMap->dst_port = dst_port;
	memcpy(nMap->dst_addr, dst_addr, 4);
	pthread_mutex_lock(&controller->mutex_nat);
	nMap->next = entryList->table[HASH_FUNC(dst_addr)];
	entryList->table[HASH_FUNC(dst_addr)] = nMap;
	pthread_mutex_unlock(&controller->mutex_nat);
	return nMap;
}

uint16_t calculateCheckSum(unsigned char *data, int last, int headerSum, int skip, int odd)
{
	uint16_t *pseudoH = (uint16_t *)data;
	int num;
	for(num = 0; num < last; num++)
	{
		if(num != skip)
			headerSum += ntohs(pseudoH[num]);
	}
	if(odd)
	{
		headerSum += ntohs(pseudoH[num]) & 0xff00;
	}
	int carry = headerSum >> 16;
	headerSum &= 0xffff;
	return ~(headerSum+carry) & 0xffff;
}

void *reinject_pkt(void *msg)
{
	pthread_detach(pthread_self());
	printf("Enter Reinject\n");
	struct natMap *nMap = (struct natMap *)msg;

	pthread_mutex_lock(&controller->mutex_port);
	struct PortData *temp = portQueue->table[HASH_FUNC(nMap->dst_addr)], *prev;
	int first = 1;

	while(temp != NULL)
	{
		struct ip_header_t *ipH = (struct ip_header_t *)temp->packet;
		struct transport_header *tH = (struct transport_header *)
								(temp->packet + (ipH->ver_ihl & 0x0f)*4);
		if(memcmp(ipH->src_addr, nMap->src_addr, 4) == 0
				&& memcmp(ipH->dst_addr, nMap->dst_addr, 4) == 0
				&& tH->src_port == nMap->src_port
				&& tH->dst_port == nMap->dst_port)
		{
			printf("%hu.%hu.%hu.%hu %hu %hu.%hu.%hu.%hu %hu %hu\n",
					nMap->src_addr[0], nMap->src_addr[1],
					nMap->src_addr[2], nMap->src_addr[3], ntohs(nMap->src_port),
					nMap->dst_addr[0], nMap->dst_addr[1],
					nMap->dst_addr[2], nMap->dst_addr[3], ntohs(nMap->dst_port),
					ntohs(nMap->new_port));
			if(first)
				portQueue->table[HASH_FUNC(nMap->dst_addr)] = temp->next;
			else
				prev->next = temp->next;

			tH->src_port = nMap->new_port;
			memcpy(ipH->src_addr, controller->ifIP, 4);

			uint16_t checksum = calculateCheckSum(temp->packet, (ipH->ver_ihl & 0x0f)*2, 0, 5, 0);
			if(ntohs(ipH->checksum) != checksum)
				ipH->checksum = htons(checksum);

			struct pseudo_header pH;
			int num = 0;
			for(num = 0; num < 4; num++)
			{
				pH.src_addr[num] = ipH->src_addr[num];
				pH.dst_addr[num] = ipH->dst_addr[num];
			}
			pH.zeroes = 0;
			pH.protocol = ipH->protocol;
			pH.length = htons(ntohs(ipH->total_length) - (ipH->ver_ihl & 0x0f)*4);
			uint16_t *pseudoH;
			pseudoH = (uint16_t *)&pH;
			int headerSum = 0;
			for(num = 0; num < 6; num++)
			{
				headerSum += ntohs(pseudoH[num]);
			}
			//~ printf("Header sum:%d ", headerSum);
			int odd = ntohs(ipH->total_length) % 2;
			if(ipH->protocol == 17)
			{
				struct udphdr *udpH = (struct udphdr *)(temp->packet + (ipH->ver_ihl & 0x0f)*4);
				//~ printf("UDP Len: %d ", ntohs(udpH->len));
				checksum = calculateCheckSum((temp->packet + (ipH->ver_ihl & 0x0f)*4),
						(ntohs(ipH->total_length) - (ipH->ver_ihl & 0x0f)*4)/2, headerSum, 3, odd);
				//~ printf("Sum:%u ", checksum);
				//~ printf("UDP Checksum:%d ", ntohs(udpH->uh_sum));
				if(ntohs(udpH->uh_sum) != checksum)
					udpH->uh_sum = htons(checksum);
				//			fputc('\n', stdout);
				//			printf("entering callback=%d\n", queueNum);
				int resp = nfq_set_verdict(temp->qh, temp->packet_id,
						NF_ACCEPT, temp->ret, temp->packet);
//				printf("Reinject %d\n", resp);
			}
			else if(ipH->protocol == 6)
			{
				struct tcphdr *tcpH = (struct tcphdr *)(temp->packet + (ipH->ver_ihl & 0x0f)*4);
				//~ printf("TCP Offset: %d ", 4*tcpH->th_off);
				checksum = calculateCheckSum((temp->packet + (ipH->ver_ihl & 0x0f)*4),
						(ntohs(ipH->total_length) - (ipH->ver_ihl & 0x0f)*4)/2, headerSum, 8, odd);
				//~ printf("Sum:%u ", checksum);
				//~ printf("TCP Checksum:%d ", ntohs(tcpH->th_sum));
				if(ntohs(tcpH->th_sum) != checksum)
					tcpH->th_sum = htons(checksum);
				//			fputc('\n', stdout);
				//			printf("entering callback=%d\n", queueNum);
				int resp = nfq_set_verdict(temp->qh, temp->packet_id,
						NF_ACCEPT, temp->ret, temp->packet);
//				printf("Reinject %d\n", resp);
			}
			else
			{
			//		fputc('\n', stdout);
			//		printf("entering callback=%d\n", queueNum);
				int resp = nfq_set_verdict(temp->qh, temp->packet_id,
						NF_ACCEPT, temp->ret, temp->packet);
				printf("Reinject %d\n", resp);
			}

			free(temp);
			if(first)
				temp = portQueue->table[HASH_FUNC(nMap->dst_addr)];
			else
				temp = prev->next;
		}
		else
		{
			if(first)
				first = 0;
			prev = temp;
			temp = temp->next;
		}
	}

	pthread_mutex_unlock(&controller->mutex_port);

	return NULL;
}

/* returns packet id */
static int handle_pkt(struct nfq_data *tb, int queueNum, struct nfq_q_handle *qh)
{
	int id = 0;
	struct nfqnl_msg_packet_hdr *ph;
	struct nfqnl_msg_packet_hw *hwph;
//	u_int32_t mark,ifi;
	int ret;
	unsigned char *data;

	ph = nfq_get_msg_packet_hdr(tb);
	if (ph) {
		id = ntohl(ph->packet_id);
	}

	ret = nfq_get_payload(tb, &data);
	if (ret >= 0)
	{
		struct ip_header_t *ipH = (struct ip_header_t *)data;
		char srcIP[50], dstIP[50];
		sprintf(srcIP, "%hhu.%hhu.%hhu.%hhu", ipH->src_addr[0], ipH->src_addr[1],
				ipH->src_addr[2], ipH->src_addr[3]);
		sprintf(dstIP, "%hhu.%hhu.%hhu.%hhu", ipH->dst_addr[0], ipH->dst_addr[1],
				ipH->dst_addr[2], ipH->dst_addr[3]);
		if(queueNum == 0)
		{
			if(strncmp(srcIP, controller->subnet, strlen(controller->subnet)) == 0)
			{
				struct transport_header *tH = (struct transport_header *)
						(data + (ipH->ver_ihl & 0x0f)*4);
				struct natMap *nMap;
				if(!PORT_TRANSLATION)
					nMap = lookupNatMap(tH->src_port, ipH->dst_addr, tH->dst_port);
				else
					nMap = lookupPortMapQ0(ipH->src_addr, tH->src_port,
							ipH->dst_addr, tH->dst_port);
				if(!nMap && !PORT_TRANSLATION)
				{
					nMap = addEntry(tH->src_port, ipH->src_addr,
							tH->dst_port, ipH->dst_addr);
					hwph = nfq_get_packet_hw(tb);
					struct ofp_nat_port_entry *nat = calloc(1,sizeof(struct ofp_nat_port_entry));
					memcpy(nat->staMacAddress, hwph->hw_addr, OFP_ETH_ALEN);
					memcpy(&nat->nw_src, ipH->src_addr, 4);
					memcpy(&nat->nw_dst, ipH->dst_addr, 4);
					nat->tp_src = tH->src_port;
					nat->tp_dst = tH->dst_port;
					pthread_t thread;
					int rc = pthread_create(&thread, NULL, send_nat_entry, nat);
					if(rc)
					{
						printf("ERROR; return code from pthread_create() is %d\n", rc);
				//		exit(-1);
					}
				}
				else if(!nMap)
				{
					addPortData(qh, id, ret, data, ipH->dst_addr);
					hwph = nfq_get_packet_hw(tb);
					struct ofp_nat_port_entry *nat = calloc(1,sizeof(struct ofp_nat_port_entry));
					memcpy(nat->staMacAddress, hwph->hw_addr, OFP_ETH_ALEN);
					memcpy(&nat->nw_src, ipH->src_addr, 4);
					memcpy(&nat->nw_dst, ipH->dst_addr, 4);
					nat->tp_src = tH->src_port;
					nat->tp_dst = tH->dst_port;
					pthread_t thread;
					int rc = pthread_create(&thread, NULL, send_nat_port_entry, nat);
					if(rc)
					{
						printf("ERROR; return code from pthread_create() is %d\n", rc);
				//		exit(-1);
					}
					return 1;
				}
				if(PORT_TRANSLATION)
					tH->src_port = nMap->new_port;
				memcpy(ipH->src_addr, controller->ifIP, 4);
//				printf("Source IP=%hhu.%hhu.%hhu.%hhu ", ipH->src_addr[0],
//						ipH->src_addr[1], ipH->src_addr[2], ipH->src_addr[3]);
//				printf("Dest IP=%hhu.%hhu.%hhu.%hhu ", ipH->dst_addr[0],
//						ipH->dst_addr[1], ipH->dst_addr[2], ipH->dst_addr[3]);
//				printf("Transport Protocol=%d ", ipH->protocol);
//				printf("Source port=%d ", ntohs(tH->src_port));
//				printf("Dest port=%d ", ntohs(tH->dst_port));
			}
			else
				return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
		}
		else if(queueNum == 1)
		{
			if(memcmp(ipH->dst_addr, controller->ifIP, 4) == 0)
			{
				struct transport_header *tH = (struct transport_header *)
						(data + (ipH->ver_ihl & 0x0f)*4);
				struct natMap *nMap;
				if(!PORT_TRANSLATION)
					nMap = lookupNatMap(tH->dst_port, ipH->src_addr, tH->src_port);
				else
					nMap = lookupPortMapQ1(tH->dst_port, ipH->src_addr, tH->src_port);
				if(nMap)
				{
					if(PORT_TRANSLATION)
						tH->dst_port = nMap->src_port;
					memcpy(ipH->dst_addr, nMap->src_addr, 4);
//					printf("Source IP=%hhu.%hhu.%hhu.%hhu ", ipH->src_addr[0],
//							ipH->src_addr[1], ipH->src_addr[2], ipH->src_addr[3]);
//					printf("Dest IP=%hhu.%hhu.%hhu.%hhu ", ipH->dst_addr[0],
//							ipH->dst_addr[1], ipH->dst_addr[2], ipH->dst_addr[3]);
//					printf("Transport Protocol=%d ", ipH->protocol);
//					printf("Source port=%d ", ntohs(tH->src_port));
//					printf("Dest port=%d ", ntohs(tH->dst_port));
				}
				else
					return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
			}
			else
				return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
		}
		else
			return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);

		uint16_t checksum = calculateCheckSum(data, (ipH->ver_ihl & 0x0f)*2, 0, 5, 0);
		if(ntohs(ipH->checksum) != checksum)
			ipH->checksum = htons(checksum);

		struct pseudo_header pH;
		int num = 0;
		for(num = 0; num < 4; num++)
		{
			pH.src_addr[num] = ipH->src_addr[num];
			pH.dst_addr[num] = ipH->dst_addr[num];
		}
		pH.zeroes = 0;
		pH.protocol = ipH->protocol;
		pH.length = htons(ntohs(ipH->total_length) - (ipH->ver_ihl & 0x0f)*4);
		uint16_t *pseudoH;
		pseudoH = (uint16_t *)&pH;
		int headerSum = 0;
		for(num = 0; num < 6; num++)
		{
			headerSum += ntohs(pseudoH[num]);
		}
		//~ printf("Header sum:%d ", headerSum);
		int odd = ntohs(ipH->total_length) % 2;
		if(ipH->protocol == 17)
		{
			struct udphdr *udpH = (struct udphdr *)(data + (ipH->ver_ihl & 0x0f)*4);
			//~ printf("UDP Len: %d ", ntohs(udpH->len));
			checksum = calculateCheckSum((data + (ipH->ver_ihl & 0x0f)*4),
					(ntohs(ipH->total_length) - (ipH->ver_ihl & 0x0f)*4)/2, headerSum, 3, odd);
			//~ printf("Sum:%u ", checksum);
			//~ printf("UDP Checksum:%d ", ntohs(udpH->uh_sum));
			if(ntohs(udpH->uh_sum) != checksum)
				udpH->uh_sum = htons(checksum);
//			fputc('\n', stdout);
//			printf("entering callback=%d\n", queueNum);
			return nfq_set_verdict(qh, id, NF_ACCEPT, ret, data);
		}
		else if(ipH->protocol == 6)
		{
			struct tcphdr *tcpH = (struct tcphdr *)(data + (ipH->ver_ihl & 0x0f)*4);
			//~ printf("TCP Offset: %d ", 4*tcpH->th_off);
			checksum = calculateCheckSum((data + (ipH->ver_ihl & 0x0f)*4),
					(ntohs(ipH->total_length) - (ipH->ver_ihl & 0x0f)*4)/2, headerSum, 8, odd);
			//~ printf("Sum:%u ", checksum);
			//~ printf("TCP Checksum:%d ", ntohs(tcpH->th_sum));
			if(ntohs(tcpH->th_sum) != checksum)
				tcpH->th_sum = htons(checksum);
//			fputc('\n', stdout);
//			printf("entering callback=%d\n", queueNum);
			return nfq_set_verdict(qh, id, NF_ACCEPT, ret, data);
		}

//		fputc('\n', stdout);
//		printf("entering callback=%d\n", queueNum);
		return nfq_set_verdict(qh, id, NF_ACCEPT, ret, data);
	}
	return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
}


static int cb(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg,
		   struct nfq_data *nfa, void *data)
{
	int *num = (int *)data;
	return handle_pkt(nfa, *num, qh);
}

void *recvPkt(void *msg)
{
	pthread_detach(pthread_self());
	int *num = (int *)msg;
	int rv = 1;
	char buf[65535] __attribute__ ((aligned));
	struct nfq_handle *h;
	struct nfq_q_handle *qh;
	struct nfnl_handle *nh;
	int fd;

	printf("Queue Num=%d\n", *num);

	printf("opening library handle h\n");
	h = nfq_open();
	if (!h) {
		fprintf(stderr, "error during nfq_open()\n");
		exit(1);
	}

	printf("unbinding existing nf_queue handler for AF_INET (if any) h\n");
	if (nfq_unbind_pf(h, AF_INET) < 0) {
		fprintf(stderr, "error during nfq_unbind_pf()\n");
		exit(1);
	}

	printf("binding nfnetlink_queue as nf_queue handler for AF_INET h\n");
	if (nfq_bind_pf(h, AF_INET) < 0) {
		fprintf(stderr, "error during nfq_bind_pf()\n");
		exit(1);
	}

	printf("binding this socket to queue '%d'\n", *num);
	qh = nfq_create_queue(h, *num, &cb, num);
	if (!qh) {
		fprintf(stderr, "error during nfq_create_queue()\n");
		exit(1);
	}

	printf("setting copy_packet mode h\n");
	if (nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xffff) < 0) {
		fprintf(stderr, "can't set packet_copy mode\n");
		exit(1);
	}

	nh = nfq_nfnlh(h);
	nfnl_rcvbufsiz(nh, 10000*1500);
	fd = nfnl_fd(nh);
	while (controller->loop_terminate && rv >= 0) {
		struct timeval tv;
		fd_set rfds;
		int t;
		tv.tv_sec = 10;
		tv.tv_usec = 0;
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		t = select(fd + 1, &rfds, NULL, NULL, &tv);
		if (t < 0)
			exit(1);
		if (FD_ISSET(fd, &rfds))
		{
			/*---- Read the message from the server into the buffer ----*/
			rv = recv(fd, buf, sizeof(buf), 0);
			if (rv <= 0)
			{
				perror("recv");
				printf("Error no is : %d\n", errno);
				printf("Error description is : %s\n",strerror(errno));
				controller->loop_terminate = 0;
				continue;
			}
//			printf("pkt received\n");
			nfq_handle_packet(h, buf, rv);
		}
	}

	printf("unbinding from queue %d\n", *num);
	nfq_destroy_queue(qh);

#ifdef INSANE
	/* normally, applications SHOULD NOT issue this command, since
	* it detaches other programs/sockets from AF_INET, too ! */
	printf("unbinding from AF_INET\n");
	nfq_unbind_pf(h, AF_INET);
#endif

	printf("closing library handle\n");
	nfq_close(h);
	return NULL;
}

int controller_conn_init(struct hostapd_data *hapd)
{
	controller = (struct controller_connection *)calloc(1,
			sizeof(struct controller_connection));
	if(controller == NULL)
	{
		//printf("Unable to allocate memory for controller struct \n");
		return 1;
	}
	controller->sock = -1;
	controller->error = 0;
	controller->miss_send_len = 0;
	controller->hapd = hapd;
	controller->tunCount = 0;
	staList = (struct STAList *)calloc(1, sizeof(struct STAList));
	if(staList == NULL)
	{
		//printf("Unable to allocate memory staList struct\n");
		return 1;
	}
	staList->num_sta = 0;
	staList->table = (struct infoSTA **)calloc(hapd->conf->max_num_sta,
			sizeof(struct infoSTA *));
	if(staList->table == NULL)
	{
		//printf("Unable to allocate memory staList->table\n");
		return 1;
	}
	pthread_mutex_init(&controller->mutex, NULL);
    struct stat st = {0};

	if (stat(FOLDER, &st) == -1)
	{
		mkdir(FOLDER, 0700);
	}
    controller->inotifyFd = inotify_init();                 /* Create inotify instance */
    if (controller->inotifyFd == -1)
    {
//		printf("inotify_init\n");
		return 1;
	}
    controller->wd = inotify_add_watch(controller->inotifyFd, FOLDER, IN_CLOSE_WRITE);
	if (controller->wd == -1)
	{
//		printf("inotify_add_watch\n");
		return 1;
	}
	int fd;
	struct ifreq ifr;

	fd = socket(AF_INET, SOCK_DGRAM, 0);

	/* I want to get an IPv4 IP address */
	ifr.ifr_addr.sa_family = AF_INET;

	/* I want IP address attached to "eth0" */
	strncpy(ifr.ifr_name, NATIFACE, IFNAMSIZ-1);

	int t = ioctl(fd, SIOCGIFADDR, &ifr);

	close(fd);

	if(t != 0)
	{
		printf("Interface not up\n");
		return 1;
	}
	/* display result */
	controller->ifIP = calloc(4, sizeof(unsigned char));
	memcpy(controller->ifIP,
			(unsigned char *)&((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr, 4);
	struct in_addr addr;
	if (inet_aton(NATSUBNET, &addr) == 0)
	{
		printf("Invalid address\n");
		return 1;
	}
	unsigned char *caddr;
	caddr = (unsigned char *)&addr;
	sprintf(controller->subnet, "%hhu.%hhu.%hhu", caddr[0], caddr[1], caddr[2]);

	entryList = calloc(1, sizeof(struct natList));
	entryList->table = calloc(MAXENTRY, sizeof(struct natMap *));

	portQueue = calloc(1, sizeof(struct PortQueue));
	portQueue->table = calloc(MAXENTRY, sizeof(struct PortData *));

	pthread_mutex_init(&controller->mutex_nat, NULL);
	pthread_mutex_init(&controller->mutex_port, NULL);
	controller->loop_terminate = 1;

    int ret = system("modprobe ipip");
	if(!WIFSIGNALED(ret) ||
    		!(WTERMSIG(ret) == SIGINT || WTERMSIG(ret) == SIGQUIT))
    {
    	return 0;
    }
	return 0;
}

void *controller_run()
{
	pthread_detach(pthread_self());
	while(1)
	{
		controller_conn_open();
		controller_receive();
	}
	return NULL;
}

//static void print_header(struct ofp_header *msg)
//{
//	printf("Version:%d\n",msg->version);
//	printf("Type:%d\n",msg->type);
//	printf("Length:%d\n", ntohs(msg->length));
//	printf("Xid:%u\n",msg->xid);
//}

static void handle_hello(struct ofp_header* hello)
{
	void *msg = calloc(1, sizeof(struct ofp_hello));
	if(msg == NULL)
	{
		//printf("Unable to handle OF message\n");
		return;
	}
	struct ofp_hello *hello_rep = msg;
	hello_rep->header.version = OFP_VERSION;
	hello_rep->header.type = OFPT_HELLO;
	hello_rep->header.length = htons(sizeof msg);
	hello_rep->header.xid = hello->xid;
	controller_send(msg, sizeof(struct ofp_hello));
//	free(msg);
}

static void handle_feature_request(struct ofp_header *features_req)
{
	void *msg = calloc(1, sizeof(struct ofp_switch_features));
	if(msg == NULL)
	{
		//printf("Unable to handle OF message\n");
		return;
	}
	struct ofp_switch_features *features_rep = msg;
	features_rep->header.type = OFPT_FEATURES_REPLY;
	features_rep->header.version = OFP_VERSION;
	features_rep->header.xid = features_req->xid;
	features_rep->header.length = htons(sizeof(struct ofp_switch_features));
	features_rep->datapath_id = htonll(controller->dpid);
	features_rep->n_buffers = htonl(0);
	features_rep->n_tables = 0;
	features_rep->capabilities = htonl(0);
	features_rep->actions = htonl(0);
	controller_send(msg, ntohs(features_rep->header.length));
//	free(msg);
}

static void handle_set_config(struct ofp_header *set_config)
{
	controller->miss_send_len = ntohs(((struct ofp_switch_config *)
			set_config)->miss_send_len);
}

static void handle_barrier_request(struct ofp_header *barrier_req)
{
	void *msg = calloc(1, sizeof(struct ofp_header));
	if(msg == NULL)
	{
		//printf("Unable to handle OF message\n");
		return;
	}
	struct ofp_header *barrier_rep = msg;
	barrier_rep->type = OFPT_BARRIER_REPLY;
	barrier_rep->version = OFP_VERSION;
	barrier_rep->xid = barrier_req->xid;
	barrier_rep->length = htons(sizeof(struct ofp_header));
	controller_send(msg, ntohs(barrier_rep->length));
//	free(msg);
}

static void handle_get_config_request(struct ofp_header *get_config_req)
{
	void *msg = calloc(1, sizeof(struct ofp_switch_config));
	if(msg == NULL)
	{
		//printf("Unable to handle OF message\n");
		return;
	}
	struct ofp_switch_config *get_config_rep = msg;
	get_config_rep->header.type = OFPT_GET_CONFIG_REPLY;
	get_config_rep->header.version = OFP_VERSION;
	get_config_rep->header.xid = get_config_req->xid;
	get_config_rep->header.length = htons(sizeof(struct ofp_switch_config));
	get_config_rep->flags = htons(OFPC_FRAG_NORMAL);
	get_config_rep->miss_send_len = htons(controller->miss_send_len);
	controller_send(msg, ntohs(get_config_rep->header.length));
//	free(msg);
}

static void handle_stats_request(struct ofp_header *oh)
{
	struct ofp_stats_request *stats_req = (struct ofp_stats_request *)oh;
	void *msg;
	size_t size;
	switch (stats_req->type)
	{
	case OFPST_DESC:
//		printf("oh::OFPST_DESC\n");
		size = sizeof(struct ofp_stats_reply) + sizeof(struct ofp_desc_stats);
		msg = calloc(1, size);
		if(msg == NULL)
		{
			//printf("Unable to handle OF message\n");
			return;
		}
		struct ofp_stats_reply *stats_rep = msg;
		stats_rep->header.type = OFPT_STATS_REPLY;
		stats_rep->header.version = OFP_VERSION;
		stats_rep->header.xid = oh->xid;
		stats_rep->header.length = htons(size);
		stats_rep->type = htons(OFPST_DESC);
		stats_rep->flags = 0;
		char *strPtr = msg;
		strPtr += sizeof(struct ofp_header);
		struct ofp_desc_stats *desc = (struct ofp_desc_stats *)strPtr;
		memset(desc->mfr_desc, '\0', DESC_STR_LEN);
		memset(desc->hw_desc, '\0', DESC_STR_LEN);
		memset(desc->sw_desc, '\0', DESC_STR_LEN);
		memset(desc->dp_desc, '\0', DESC_STR_LEN);
		memset(desc->serial_num, '\0', SERIAL_NUM_LEN);
		controller_send(msg, ntohs(stats_rep->header.length));
		break;
	case OFPST_FLOW:
		//printf("oh::OFPST_FLOW\n");
		break;
	case OFPST_PORT:
		//printf("oh::OFPST_PORT\n");
		break;
	case OFPST_TABLE:
		//printf("oh::OFPST_TABLE\n");
		break;
	case OFPST_QUEUE:
		//printf("oh::OFPST_QUEUE\n");
		break;
	case OFPST_AGGREGATE:
		//printf("oh::OFPST_AGGREGATE\n");
		break;
	case OFPST_VENDOR:
		//printf("oh::OFPST_VENDOR\n");
		break;
	//default:
		//printf("Invalid stats req type\n");
	}
//	free(msg);
}

static void handle_vendor(struct ofp_header *oh)
{
	void *msg;
	size_t size;
	struct ofp_vendor_header *vendor_header = (struct ofp_vendor_header *)oh;
	switch (ntohl(vendor_header->vendor))
	{
	case NX_VENDOR_ID:
	{
		//printf("Nicira message\n");
		struct nicira_header *nxt_header = (struct nicira_header *)oh;
		switch (ntohl(nxt_header->subtype))
		{
		case NXT_ROLE_REQUEST:
			size = sizeof(struct nicira_header) + sizeof(struct nx_role_request);
			msg = calloc(1, size);
			if(msg == NULL)
			{
				//printf("Unable to handle OF message\n");
				return;
			}
			struct nicira_header *nxt_rep = (struct nicira_header *)msg;
			nxt_rep->header.version = OFP_VERSION;
			nxt_rep->header.type = OFPT_VENDOR;
			nxt_rep->header.xid = oh->xid;
			nxt_rep->header.length = htons(size);
			nxt_rep->vendor = htonl(NX_VENDOR_ID);
			nxt_rep->subtype = htonl(NXT_ROLE_REPLY);
			char *strPtrRep = msg;
			char *strPtrReq = (char *)(void *)oh;
			strPtrRep += sizeof(struct nicira_header);
			strPtrReq += sizeof(struct nicira_header);
			struct nx_role_request *nx_role_rep = (struct nx_role_request *)strPtrRep;
			struct nx_role_request *nx_role_req = (struct nx_role_request *)strPtrReq;
			nx_role_rep->role = nx_role_req->role;
			controller_send(msg, ntohs(nxt_rep->header.length));
			break;
		//default:
			//printf("Type:%d\n", ntohl(nxt_header->subtype));
		}
		break;
	}
	case AP_EXPERIMENTER_ID:
	{
		//printf("OF SWITCH AP message\n");
		struct ofp_wlan_header *wlan_header = (struct ofp_wlan_header *)oh;
		char *strPtrMsg;
//		printf("subtype:%d\n", ntohl(wlan_header->subtype));
		switch (ntohl(wlan_header->subtype))
		{
		case WLAN_ADD_STA:
			strPtrMsg = (char *)(void *)oh;
			strPtrMsg += sizeof(struct ofp_wlan_header);
			struct ofp_wlan_sta *ofp_add_STA = (struct ofp_wlan_sta *)strPtrMsg;
		    struct infoSTA *staAdd = lookupSenderSTA(ofp_add_STA->staMacAddress);
		    if (!staAdd)
		    {
		    	staAdd =  addSTA(ofp_add_STA->staMacAddress);
		    	if(!staAdd)
		    	{
		    		return;
		    	}
		    }
		    changeStatus(staAdd, SELECTED);
		    //printf("Added STA in SELECTED state\n");
			break;
		case WLAN_LAST_REC_POW:
			strPtrMsg = (char *)(void *)oh;
			strPtrMsg += sizeof(struct ofp_wlan_header);
			struct ofp_wlan_sta *ap_RSSI = (struct ofp_wlan_sta *)strPtrMsg;
			handle_ap_rssi(ap_RSSI);
			break;
		case WLAN_UPDATE_STA_ADD:
			strPtrMsg = (char *)(void *)oh;
			strPtrMsg += sizeof(struct ofp_wlan_header);
			struct ofp_wlan_add_update_msg *add_update_STA =
					(struct ofp_wlan_add_update_msg *)strPtrMsg;
			struct infoSTA *staAddUpdate = lookupSenderSTA(add_update_STA->staMacAddress);
		    if (staAddUpdate)
		    {
		    	changeStatus(staAddUpdate, ASSOCIATED);
		    	pthread_mutex_lock(&controller->mutex);
		    	staAddUpdate->auth_alg = ntohs(add_update_STA->auth_alg);
		    	staAddUpdate->aid = ntohs(add_update_STA->aid);
		    	staAddUpdate->capability = ntohs(add_update_STA->capability);
		    	staAddUpdate->listen_interval = ntohs(add_update_STA->listen_interval);
		    	staAddUpdate->qosinfo = add_update_STA->qosinfo;
		    	staAddUpdate->supported_rates_len = ntohl(add_update_STA->supported_rates_len);
		    	memcpy(staAddUpdate->supported_rates, add_update_STA->supported_rates,
		    			staAddUpdate->supported_rates_len);
		    	pthread_mutex_unlock(&controller->mutex);
		    	add_STA(controller->hapd, staAddUpdate);
		    }
			break;
		case WLAN_REMOVE_STA:
			strPtrMsg = (char *)(void *)oh;
			strPtrMsg += sizeof(struct ofp_wlan_header);
			struct ofp_wlan_sta *ofp_remove_STA = (struct ofp_wlan_sta *)strPtrMsg;
		    printf("WLAN_REMOVE_STA\n");
			struct infoSTA *staRemove = lookupSenderSTA(ofp_remove_STA->staMacAddress);
		    if (staRemove)
		    {
		    	changeStatus(staRemove, NOT_AUTHENTICATED);
			    printf("Changed STA state to NOT AUTHENTICATED\n");
		    	remove_STA(controller->hapd, staRemove);
		    }
		    else
		    {
		    	printf("Delete:STA not found\n");
		    }
			break;
		case WLAN_NAT_ADD:
			strPtrMsg = (char *)(void *)oh;
			strPtrMsg += sizeof(struct ofp_wlan_header);
			struct ofp_nat_port_entry *ofp_nat_add = (struct ofp_nat_port_entry *)strPtrMsg;
		    printf("WLAN_NAT_ADD\n");
		    unsigned char src_addr[4], dst_addr[4];
		    memcpy(src_addr, &ofp_nat_add->nw_src, 4);
		    memcpy(dst_addr, &ofp_nat_add->nw_dst, 4);
//		    printf("%d %hu %d %hu %hu\n", ntohl(ofp_nat_add->nw_src), ntohs(ofp_nat_add->tp_src),
//		    		ntohl(ofp_nat_add->nw_dst), ntohs(ofp_nat_add->tp_dst),
//					ntohs(ofp_nat_add->tp_new));
		    struct natMap *nMap;
		    if(!PORT_TRANSLATION)
		    	nMap = lookupNatMap(ofp_nat_add->tp_src, dst_addr, ofp_nat_add->tp_dst);
		    else
		    	nMap = lookupPortMapQ0(src_addr, ofp_nat_add->tp_src,
		    			dst_addr, ofp_nat_add->tp_dst);
		    if(!nMap)
		    	nMap = addEntry(ofp_nat_add->tp_src, src_addr, ofp_nat_add->tp_dst, dst_addr);
		    if(PORT_TRANSLATION)
		    {
		    	nMap->new_port = ofp_nat_add->tp_new;
				pthread_t thread;
				int rc = pthread_create(&thread, NULL, reinject_pkt, nMap);
				if(rc)
				{
					printf("ERROR; return code from pthread_create() is %d\n", rc);
			//		exit(-1);
				}
		    }
		default:
			printf("Type:%d\n", ntohl(wlan_header->subtype));
		}
		break;
	}
	//default:
		//printf("Vendor:%x\n", ntohl(vendor_header->vendor));
	}
//	free(msg);
}

static void handle_echo_request(struct ofp_header *echo_req)
{
	struct ofp_header *echo_reply;
	echo_req->type = OFPT_ECHO_REPLY;
	void *msg = calloc(1, ntohs(echo_req->length));
	if(msg == NULL)
	{
		//printf("Unable to handle OF message\n");
		return;
	}
	echo_reply = msg;
	memcpy(echo_reply, echo_req, ntohs(echo_req->length));
	//printf("Echo_reply\n");
	//printf("Version:%d\n",echo_reply->version);
	//printf("Type:%d\n",echo_reply->type);
	//printf("Length:%d\n",ntohs(echo_reply->length));
	//printf("Xid:%u\n",echo_reply->xid);
	controller_send(msg, ntohs(echo_reply->length));
//	free(msg);
}

static char *getIPaddress(char *mac)
{
    FILE *arpCache = fopen(ARP_CACHE, "r");
    if (!arpCache)
    {
        perror("Arp Cache: Failed to open file \"" ARP_CACHE "\"");
        return NULL;
    }

    /* Ignore the first line, which contains the header */
    char header[ARP_BUFFER_LEN];
    if (!fgets(header, sizeof(header), arpCache))
    {
        return NULL;
    }

    char ipAddr[ARP_BUFFER_LEN], hwAddr[ARP_BUFFER_LEN];
    int found = 0;
    while (2 == fscanf(arpCache, ARP_LINE_FORMAT, ipAddr, hwAddr))
    {
		if(strncmp(hwAddr, mac, 17) == 0)
        {
			printf("%s\n", ipAddr);
			found = 1;
            break;
		}
    }
    fclose(arpCache);
    if(found)
    {
    	char *ip = calloc(1, ARP_BUFFER_LEN);
    	memcpy(ip, ipAddr, ARP_BUFFER_LEN);
    	return ip;
    }
    else
    	return NULL;
}

static char *getDevice(char *ipaddr)
{
	struct ifaddrs *addrs, *iap;
	struct sockaddr_in *sa;
	char buf[32];
	int found = 0;

	getifaddrs(&addrs);
	for (iap = addrs; iap != NULL; iap = iap->ifa_next)
	{
		if (iap->ifa_addr
				&& (iap->ifa_flags & IFF_UP)
				&& iap->ifa_addr->sa_family == AF_INET)
		{
			sa = (struct sockaddr_in *)(iap->ifa_addr);
			inet_ntop(iap->ifa_addr->sa_family, (void *)&(sa->sin_addr), buf, sizeof(buf));
			if (!strcmp(ipaddr, buf))
			{
				found = 1;
				break;
			}
		}
	}
	if(found)
	{
		char *dev = calloc(1, ARP_BUFFER_LEN);
		memcpy(dev, iap->ifa_name, ARP_BUFFER_LEN);
		freeifaddrs(addrs);
		return dev;
	}
	else
	{
		freeifaddrs(addrs);
		return NULL;
	}
}

static void add_flows(struct ofp_flow_mod *flow_mod)
{
	struct ofp_match match = flow_mod->match;
	struct ofp_action_header *actions = flow_mod->actions;
	uint16_t rem_len = ntohs(flow_mod->header.length) - sizeof(struct ofp_flow_mod);
	char *strPtrMsg;
	while(rem_len)
	{
		switch(ntohs(actions->type))
		{
		case OFPAT_VENDOR:
		{
			struct ofp_action_vendor_header *vendor_action =
					(struct ofp_action_vendor_header *)actions;
			if(ntohl(vendor_action->vendor) == AP_EXPERIMENTER_ID)
			{
				strPtrMsg = (char *) (void *) vendor_action;
				strPtrMsg += sizeof(struct ofp_action_vendor_header);
				struct ofp_action_iph *iph = (struct ofp_action_iph *)strPtrMsg;
				if(ntohl(iph->subtype) == OFPAT_EXPERIMENTER_PUSH_IPH)
				{
					char mac[18];
					struct infoSTA *sta = lookupSenderSTA(match.dl_dst);
					if(!sta)
					{
						printf("STA not found\n");
						return;
					}
					if(!sta->ipaddr)
					{
						sprintf(mac, MACSTR, MAC2STR(match.dl_dst));
						sta->ipaddr = getIPaddress(mac);
					}
					if(!sta->ipaddr)
					{
						printf("IP address not found in ARP cache\n");
						return;
					}
					printf("IP address found in ARP cache\n");
					char tunnelEntry[ARP_BUFFER_LEN];
					char tunnelExit[ARP_BUFFER_LEN];
					uint32_t entry = ntohl(iph->tunnelEntry);
					uint32_t exit = ntohl(iph->tunnelExit);
					sprintf(tunnelEntry, "%d.%d.%d.%d", (uint8_t) (entry >> 24),
							 (uint8_t) (entry >> 16),(uint8_t) (entry >> 8), (uint8_t) entry);
					sprintf(tunnelExit, "%d.%d.%d.%d", (uint8_t) (exit >> 24),
							 (uint8_t) (exit >> 16),(uint8_t) (exit >> 8), (uint8_t) exit);
					char *device = getDevice(tunnelEntry);
					char command[1024];
					sprintf(command, "ip tun add ofaptun%d mode ipip"
							" remote %s dev %s", controller->tunCount, tunnelExit, device);
					printf("%s\n", command);
				    int ret = system(command);
//				    if(!WIFSIGNALED(ret) ||
//				    		!(WTERMSIG(ret) == SIGINT || WTERMSIG(ret) == SIGQUIT))
//				    {
//				    	printf("Tunnel could not be created\n");
//				    	return;
//				    }
			    	printf("Tunnel created\n");
				    sta->tunNum = controller->tunCount++;
				    sprintf(command, "ifconfig ofaptun%d up", sta->tunNum);
					printf("%s\n", command);
				    ret = system(command);
//				    if(!WIFSIGNALED(ret) ||
//				    		!(WTERMSIG(ret) == SIGINT || WTERMSIG(ret) == SIGQUIT))
//				    {
//				    	printf("Flags could not be set on tunnel device\n");
//				    	return;
//				    }
			    	printf("Flags set on tunnel device\n");
				    sprintf(command, "route add -host %s"
				    		" dev ofaptun%d", sta->ipaddr, sta->tunNum);
					printf("%s\n", command);
				    ret = system(command);
//				    if(!WIFSIGNALED(ret) ||
//				    		!(WTERMSIG(ret) == SIGINT || WTERMSIG(ret) == SIGQUIT))
//				    {
//				    	printf("route could not be added for tunnel\n");
//				    	return;
//				    }
			    	printf("route added for tunnel\n");
				}
			}
			break;
		}
		default:
			break;
		}
		rem_len -= ntohs(actions->len);
		strPtrMsg = (char *) (void *) actions;
		strPtrMsg += ntohs(actions->len);
		actions = (struct ofp_action_header *) strPtrMsg;
	}
}

static void delete_flows(struct ofp_flow_mod *flow_mod)
{
	struct ofp_match match = flow_mod->match;
	struct infoSTA *sta = lookupSenderSTA(match.dl_dst);
	if(!sta)
	{
		printf("STA not found\n");
		return;
	}
	char command[1024];
    sprintf(command, "route del -host %s"
    		" dev ofaptun%d", sta->ipaddr, sta->tunNum);
    printf("%s\n", command);
    int ret = system(command);
//    if(!WIFSIGNALED(ret) ||
//    		!(WTERMSIG(ret) == SIGINT || WTERMSIG(ret) == SIGQUIT))
//    {
//    	printf("route could not be added for tunnel\n");
//    	return;
//    }
	sprintf(command, "ip tunnel del ofaptun%d", sta->tunNum);
    printf("%s\n", command);
	ret = system(command);
//    if(!WIFSIGNALED(ret) ||
//    		!(WTERMSIG(ret) == SIGINT || WTERMSIG(ret) == SIGQUIT))
//    {
//    	printf("route could not be added for tunnel\n");
//    	return;
//    }
}

static void handle_flow_mod(struct ofp_header *oh)
{
	struct ofp_flow_mod *flow_mod = (struct ofp_flow_mod *)oh;
//	void *msg;
//	size_t size;
	switch (ntohs(flow_mod->command))
	{
	case OFPFC_ADD:
		add_flows(flow_mod);
		break;
	case OFPFC_DELETE:
		delete_flows(flow_mod);
		break;
	case OFPFC_DELETE_STRICT:
		break;
	case OFPFC_MODIFY:
		break;
	case OFPFC_MODIFY_STRICT:
		break;
	//default:
		//printf("Invalid stats req type\n");
	}
//	free(msg);
}

static void process_controller_msg(struct ofp_header *oh)
{
//	struct ofp_header *oh;
//	oh = (struct ofp_header *)(void *) str;
	uint8_t type = oh->type;
//	print_header(oh);
	switch (type)
	{
	case OFPT_HELLO:
//		printf("oh::Hello\n");
		handle_hello(oh);
		break;
	case OFPT_ECHO_REQUEST:
//		printf("oh::echo_req\n");
		handle_echo_request(oh);
		break;
	case OFPT_FEATURES_REQUEST:
//		printf("oh::features_req\n");
		handle_feature_request(oh);
		break;
	case OFPT_SET_CONFIG:
//		printf("oh::set_config\n");
		handle_set_config(oh);
		break;
	case OFPT_BARRIER_REQUEST:
//		printf("oh::barrier_req\n");
		handle_barrier_request(oh);
		break;
	case OFPT_GET_CONFIG_REQUEST:
//		printf("oh::get_config_req\n");
		handle_get_config_request(oh);
		break;
	case OFPT_STATS_REQUEST:
//		printf("oh::stats_req\n");
		handle_stats_request(oh);
		break;
	case OFPT_FLOW_MOD:
		printf("oh::flow_mod\n");
		handle_flow_mod(oh);
		break;
	case OFPT_VENDOR:
//		printf("oh::vendor\n");
		handle_vendor(oh);
		break;
	default:
//		printf("%u\n",type);
		break;
	}
//	printf("Length:%d\n",oh->length);
}

static int conn_open(int* s)
{
    struct sockaddr_in dest;
	int flags;

	struct ifreq ifr;
    int i, err;
	struct ethtool_perm_addr *epaddr;

	*s = socket(PF_INET, SOCK_STREAM, 0);
	if (*s < 0)
	{
		return 1;
	}

	/*
	 * Make socket non-blocking so that we don't hang forever if
	 * target dies unexpectedly.
	 */
	flags = fcntl(*s, F_GETFL);
	if (flags >= 0)
	{
		flags |= O_NONBLOCK;
		if (fcntl(*s, F_SETFL, flags) < 0)
		{
			perror("fcntl(s, O_NONBLOCK)");
			/* Not fatal, continue on.*/
		}
	}

	dest.sin_family = AF_INET;
	inet_aton(SOCK_ADDR, &dest.sin_addr);
	dest.sin_port = htons(SOCK_PORT);
	int val = 63 << 2;
	int retval = setsockopt(*s, IPPROTO_IP, IP_TOS, &val, sizeof val);
	if(retval)
	{
		//printf("Error on DSCP setsockopt\n");
        perror("DSCP");
	}
	if (connect(*s, (struct sockaddr *) &dest, sizeof(dest)) < 0)
	{
		if(errno == EINPROGRESS)
		{
			struct timeval tv;
			fd_set wfds;
			int t;
			tv.tv_sec = 10;
			tv.tv_usec = 0;
			FD_ZERO(&wfds);
			FD_SET(controller->sock, &wfds);
			t = select(*s + 1, NULL, &wfds, NULL, &tv);
			if (t < 0)
				exit(1);
			if (FD_ISSET(controller->sock, &wfds))
			{
				unsigned int len = sizeof(errno);
				getsockopt(*s, SOL_SOCKET, SO_ERROR, &errno, &len);
				if (errno != 0)
				{
					close(*s);
					return 1;
				}
				else
				{
					ifr.ifr_addr.sa_family = AF_INET;
					strcpy(ifr.ifr_name , IFACE);
					epaddr = calloc(1, sizeof(struct ethtool_perm_addr) + MAX_ADDR_LEN);
					epaddr->cmd = ETHTOOL_GPERMADDR;
					epaddr->size = MAX_ADDR_LEN;
					ifr.ifr_data = (char *)epaddr;
					err = ioctl(*s, SIOCETHTOOL, &ifr);
					if (err < 0)
					{
						return 1;
					}
					controller->mac = calloc(1, ETH_ALEN);
					if(controller->mac == NULL)
					{
						//printf("Unable to store MAC address\n");
						return 1;
					}
					memcpy(controller->mac, (unsigned char *)epaddr->data, ETH_ALEN);
					uint64_t dpid = 0;

					for(i = 0; i < 6; i++)
					{
						dpid = (uint8_t)controller->mac[i] | dpid << 8;
					}
					controller->dpid = dpid;
				    int on = 1;
				    int retval;
				    retval = setsockopt(*s, IPPROTO_TCP, TCP_NODELAY, &on, sizeof on);
				    if (retval)
				    {
						//printf("Error on DSCP setsockopt\n");
				        perror("No Delay");
				    }
				}
			}
		}
		else
		{
			perror("Connect");
			close(*s);
			return 1;
		}
	}

	return 0;
}


static void controller_conn_open(void)
{
	if(controller->sock == -1)
	{
		if (conn_open(&(controller->sock)))
		{
			controller->sock = -1;
			return;
		}
		//printf("Connected\n");
	}
}

static void controller_send(void *msg, size_t n)
{
	if(controller->sock != -1)
	{
		int t;
		t = send(controller->sock, msg, n, 0);
		if (t == -1)
		{
			perror("send");
			controller->error++;
			if(controller->error == 10)
			{
				controller->error = 0;
				controller->sock = -1;
			}
		}
		else if(t == n)
		{
			//printf("Complete message sent\n");
		}
	}
	free(msg);
}

static void controller_receive(void)
{
	if(controller->sock != -1)
	{
		struct timeval tv;
		fd_set rfds;
		char str[100];
		int t;
		tv.tv_sec = 10;
		tv.tv_usec = 0;
		FD_ZERO(&rfds);
		FD_SET(controller->sock, &rfds);
		t = select(controller->sock + 1, &rfds, NULL, NULL, &tv);
		if (t < 0)
			exit(1);
		if (FD_ISSET(controller->sock, &rfds))
		{
			t = recv(controller->sock, str, sizeof(struct ofp_header), 0);
			if (t <= 0)
			{
				perror("recv");
				controller->sock = -1;
				return;
			}
			struct ofp_header *oh = (struct ofp_header *)(void *) str;
			char *strPtr = str;
			if(ntohs(oh->length) > sizeof(struct ofp_header))
			{
				//printf("More message:%d\n", ntohs(oh->length));
				strPtr += sizeof(struct ofp_header);
				t = recv(controller->sock, strPtr,
						ntohs(oh->length) -sizeof(struct ofp_header), 0);
				if (t < 0)
				{
					perror("recv");
					exit(1);
				}
			}
			process_controller_msg(oh);
		}
	}
}

static struct infoSTA *addSTA(uint8_t *addr)
{
	struct infoSTA *sta;

	sta = lookupSenderSTA(addr);
	if (sta)
		return sta;

	if (staList->num_sta >= controller->hapd->conf->max_num_sta) {
		//printf("No more room for new STAs\n");
		return NULL;
	}

	sta = calloc(1, sizeof(struct infoSTA));
	if (sta == NULL) {
		//printf("Calloc failed\n");
		return NULL;
	}
	memcpy(sta->address, addr, ETH_ALEN);
	pthread_mutex_lock(&controller->mutex);
	staList->num_sta++;
	sta->next = staList->table[HASH_FUNC(sta->address)];
	staList->table[HASH_FUNC(sta->address)] = sta;
	pthread_mutex_unlock(&controller->mutex);
	return sta;
}

struct infoSTA *lookupSenderSTA(const uint8_t *address)
{
	struct infoSTA *sta;
	pthread_mutex_lock(&controller->mutex);
	sta = staList->table[HASH_FUNC(address)];
	while (sta != NULL && memcmp(sta->address, address, ETH_ALEN) != 0)
		sta = sta->next;
	pthread_mutex_unlock(&controller->mutex);
	return sta;
}

void changeStatus(struct infoSTA *sta, enum STAStatus status)
{
	pthread_mutex_lock(&controller->mutex);
	sta->status = status;
	if(status == NOT_AUTHENTICATED)
	{
		sta->lastReceivedSignalStrength = 0;
	}
	pthread_mutex_unlock(&controller->mutex);
}

static uint32_t generate_xid()
{
	time_t t;
	srand((unsigned) time(&t));
	return rand();
}

static void *create_ofp_msg(size_t size)
{
	void *msg = calloc(1, size);
	if(msg == NULL)
	{
		//printf("Unable to handle OF message\n");
		return NULL;
	}
	struct ofp_header *oh = (struct ofp_header *)msg;
	oh->version = OFP_VERSION;
	oh->type = OFPT_VENDOR;
	oh->length = htons(size);
	oh->xid = htons(generate_xid());
	return msg;
}

static void handle_auth(struct ieee80211_mgmt *mgmt)
{
	size_t size = sizeof(struct ofp_wlan_header) + sizeof(struct ofp_wlan_auth_msg);
	void *experimenter = create_ofp_msg(size);
	if(experimenter == NULL)
	{
		//printf("Unable to handle OF message\n");
		return;
	}
	struct ofp_wlan_header *wlan_header = (struct ofp_wlan_header *)experimenter;
	wlan_header->vendor = htonl(AP_EXPERIMENTER_ID);
	wlan_header->subtype = htonl(WLAN_AUTH_REQ);
	char *strPtr = experimenter;
	strPtr += sizeof(struct ofp_wlan_header);
	struct ofp_wlan_auth_msg *wlan_auth_msg = (struct ofp_wlan_auth_msg *)strPtr;
	memcpy(wlan_auth_msg->staMacAddress, mgmt->sa, 6);
	memcpy(wlan_auth_msg->apMacAddress, mgmt->da, 6);
	wlan_auth_msg->auth_alg = htons(le_to_host16(mgmt->u.auth.auth_alg));

	struct infoSTA *infoSta = lookupSenderSTA(mgmt->sa);
	pthread_mutex_lock(&controller->mutex);
	infoSta->auth_alg = htons(le_to_host16(mgmt->u.auth.auth_alg));
	pthread_mutex_unlock(&controller->mutex);

	controller_send(experimenter, ntohs(wlan_header->header.length));
}

static void handle_assoc_req(struct mgmt_handle *handle, int reassoc)
{
	struct ieee80211_mgmt *mgmt = handle->mgmt;
	size_t size = sizeof(struct ofp_wlan_header) + sizeof(struct ofp_wlan_assoc_msg);
	void *experimenter = create_ofp_msg(size);
	if(experimenter == NULL)
	{
		//printf("Unable to handle OF message\n");
		return;
	}
	struct ofp_wlan_header *wlan_header = (struct ofp_wlan_header *)experimenter;
	wlan_header->vendor = htonl(AP_EXPERIMENTER_ID);
	if(reassoc)
		wlan_header->subtype = htonl(WLAN_REASSOC_REQ);
	else
		wlan_header->subtype = htonl(WLAN_ASSOC_REQ);
	char *strPtr = experimenter;
	strPtr += sizeof(struct ofp_wlan_header);
	struct ofp_wlan_assoc_msg *wlan_assoc_msg = (struct ofp_wlan_assoc_msg *)strPtr;
	memcpy(wlan_assoc_msg->staMacAddress, mgmt->sa, 6);
	memcpy(wlan_assoc_msg->apMacAddress, mgmt->da, 6);
	wlan_assoc_msg->aid = htons(handle->sta->aid);
	wlan_assoc_msg->capability = htons(handle->sta->capability);
	wlan_assoc_msg->listen_interval = htons(handle->sta->listen_interval);
	wlan_assoc_msg->qosinfo =  handle->sta->qosinfo;
	memcpy(wlan_assoc_msg->supported_rates, handle->sta->supported_rates,
			handle->sta->supported_rates_len);
	wlan_assoc_msg->supported_rates_len = htonl(handle->sta->supported_rates_len);

	struct infoSTA *infoSta = lookupSenderSTA(mgmt->sa);
	pthread_mutex_lock(&controller->mutex);
	infoSta->aid = handle->sta->aid;
	infoSta->capability = handle->sta->capability;
	infoSta->listen_interval = handle->sta->listen_interval;
	infoSta->qosinfo =  handle->sta->qosinfo;
	memcpy(infoSta->supported_rates, handle->sta->supported_rates,
			handle->sta->supported_rates_len);
	infoSta->supported_rates_len = handle->sta->supported_rates_len;
	pthread_mutex_unlock(&controller->mutex);

	controller_send(experimenter, ntohs(wlan_header->header.length));
}

static void handle_disassoc_req(struct ieee80211_mgmt *mgmt)
{
	size_t size = sizeof(struct ofp_wlan_header) + sizeof(struct ofp_wlan_msg);
	void *experimenter = create_ofp_msg(size);
	if(experimenter == NULL)
	{
		//printf("Unable to handle OF message\n");
		return;
	}
	struct ofp_wlan_header *wlan_header = (struct ofp_wlan_header *)experimenter;
	wlan_header->vendor = htonl(AP_EXPERIMENTER_ID);
	wlan_header->subtype = htonl(WLAN_DISASSOC_REQ);
	char *strPtr = experimenter;
	strPtr += sizeof(struct ofp_wlan_header);
	struct ofp_wlan_msg *wlan_msg = (struct ofp_wlan_msg *)strPtr;
	memcpy(wlan_msg->staMacAddress, mgmt->sa, 6);
	memcpy(wlan_msg->apMacAddress, mgmt->da, 6);
	controller_send(experimenter, ntohs(wlan_header->header.length));
}

void *handle_deauth_conn(void *msg)
{
	uint8_t *addr = (uint8_t *)msg;
	size_t size = sizeof(struct ofp_wlan_header) + sizeof(struct ofp_wlan_msg);
	void *experimenter = create_ofp_msg(size);
	if(experimenter == NULL)
	{
		//printf("Unable to handle OF message\n");
		return NULL;
	}
	struct ofp_wlan_header *wlan_header = (struct ofp_wlan_header *)experimenter;
	wlan_header->vendor = htonl(AP_EXPERIMENTER_ID);
	wlan_header->subtype = htonl(WLAN_DEAUTH_REQ);
	char *strPtr = experimenter;
	strPtr += sizeof(struct ofp_wlan_header);
	struct ofp_wlan_msg *wlan_msg = (struct ofp_wlan_msg *)strPtr;
	memcpy(wlan_msg->staMacAddress, addr, 6);
	memcpy(wlan_msg->apMacAddress, controller->mac, 6);
	controller_send(experimenter, ntohs(wlan_header->header.length));
	free(addr);
	return NULL;
}

static void handle_deauth(struct ieee80211_mgmt *mgmt)
{
	size_t size = sizeof(struct ofp_wlan_header) + sizeof(struct ofp_wlan_msg);
	void *experimenter = create_ofp_msg(size);
	if(experimenter == NULL)
	{
		//printf("Unable to handle OF message\n");
		return;
	}
	struct ofp_wlan_header *wlan_header = (struct ofp_wlan_header *)experimenter;
	wlan_header->vendor = htonl(AP_EXPERIMENTER_ID);
	wlan_header->subtype = htonl(WLAN_DEAUTH_REQ);
	char *strPtr = experimenter;
	strPtr += sizeof(struct ofp_wlan_header);
	struct ofp_wlan_msg *wlan_msg = (struct ofp_wlan_msg *)strPtr;
	memcpy(wlan_msg->staMacAddress, mgmt->sa, 6);
	memcpy(wlan_msg->apMacAddress, mgmt->da, 6);
	controller_send(experimenter, ntohs(wlan_header->header.length));
}

static void handle_probe_request(struct ieee80211_mgmt *mgmt)
{
	size_t size = sizeof(struct ofp_wlan_header) + sizeof(struct ofp_wlan_msg);
	void *experimenter = create_ofp_msg(size);
	if(experimenter == NULL)
	{
		//printf("Unable to handle OF message\n");
		return;
	}
	struct ofp_wlan_header *wlan_header = (struct ofp_wlan_header *)experimenter;
	wlan_header->vendor = htonl(AP_EXPERIMENTER_ID);
	wlan_header->subtype = htonl(WLAN_PROBE);
	char *strPtr = experimenter;
	strPtr += sizeof(struct ofp_wlan_header);
	struct ofp_wlan_msg *wlan_msg = (struct ofp_wlan_msg *)strPtr;
	memcpy(wlan_msg->staMacAddress, mgmt->sa, 6);
	memcpy(wlan_msg->apMacAddress, mgmt->da, 6);
	controller_send(experimenter, ntohs(wlan_header->header.length));
}

void *process_sta_msg(void *msg)
{
//	printf("Mac : " MACSTR "\n",MAC2STR(controller->mac));
	u16 fc;
	char stype;
	struct mgmt_handle *handle = (struct mgmt_handle *)msg;
	struct ieee80211_mgmt *mgmt = handle->mgmt;
	pthread_detach(pthread_self());
	memcpy(mgmt->da, controller->mac, ETH_ALEN);
	fc = le_to_host16(mgmt->frame_control);
	stype = WLAN_FC_GET_STYPE(fc);
	switch (stype)
	{
	case WLAN_FC_STYPE_AUTH:
		//printf("mgmt::auth\n");
		handle_auth(mgmt);
		break;
	case WLAN_FC_STYPE_ASSOC_REQ:
		//printf("mgmt::assoc_req\n");
		handle_assoc_req(handle, 0);
		break;
	case WLAN_FC_STYPE_REASSOC_REQ:
		//printf("mgmt::reassoc_req\n");
		handle_assoc_req(handle, 1);
		break;
	case WLAN_FC_STYPE_DISASSOC:
		//printf("mgmt::disassoc\n");
		handle_disassoc_req(mgmt);
		break;
	case WLAN_FC_STYPE_DEAUTH:
		//printf("mgmt::deauth\n");
		handle_deauth(mgmt);
		break;
	case WLAN_FC_STYPE_PROBE_REQ:
		//printf("mgmt::probe req\n");
		handle_probe_request(mgmt);
		break;
	default:
		//printf("Message type not handled: %u\n",stype);
		break;
	}
	free(handle->mgmt);
	free(handle);
	return NULL;
}

void controller_conn_close(void)
{
    close(controller->sock);
    pthread_mutex_destroy(&controller->mutex);
    if(controller != NULL)
    	free(controller);
    if(staList != NULL)
    	free(staList);
    int ret = system("rm -r " FOLDER);
//    if(!WIFSIGNALED(ret) ||
//    		!(WTERMSIG(ret) == SIGINT || WTERMSIG(ret) == SIGQUIT))
//    {
//    	return;
//    }
    ret = system("modprobe -r ipip");
//	if(!WIFSIGNALED(ret) ||
//    		!(WTERMSIG(ret) == SIGINT || WTERMSIG(ret) == SIGQUIT))
//    {
//    	return;
//    }
//    int rem = staList->num_sta;
//    int i = 0;
//    for(i = 0; i <= 255 || rem != 0; i++)
//    {
//    	struct infoSTA *sta = staList->table[i];
//    	if(!sta)
//    		continue;
//    	else
//    	{
//    		if(sta->status == ASSOCIATED)
//    		{
//    			char command[1024];
//			    sprintf(command, "route del -host %s"
//			    		" dev ofaptun%d", sta->ipaddr, sta->tunNum);
//			    system(command);
//    			sprintf(command, "ip tunnel del ofaptun%d", sta->tunNum);
//    			system(command);
//    		}
//    		rem--;
//    		while(sta->next)
//    		{
//    			sta = sta->next;
//    			if(sta->status == ASSOCIATED)
//    			{
//    				char command[1024];
//    			    sprintf(command, "route del -host %s"
//    			    		" dev ofaptun%d", sta->ipaddr, sta->tunNum);
//    			    system(command);
//    				sprintf(command, "ip tunnel del ofaptun%d", sta->tunNum);
//    				system(command);
//    			}
//    			rem--;
//    		}
//    	}
//    }
	printf("Program Terminate\n");
	controller->loop_terminate = 0;
}

void *data_frames_rssi()
{
	char buf[BUF_LEN];
	ssize_t numRead;
	char *p;
	struct inotify_event *event;
	pthread_detach(pthread_self());
	while(1)
	{
		struct timeval tv;
		fd_set rfds;
		int t;
		tv.tv_sec = 10;
		tv.tv_usec = 0;
		FD_ZERO(&rfds);
		FD_SET(controller->inotifyFd, &rfds);
		t = select(controller->inotifyFd + 1, &rfds, NULL, NULL, &tv);
		if (t < 0)
			exit(1);
		if (FD_ISSET(controller->inotifyFd, &rfds))
		{                                  /* Read events forever */
			numRead = read(controller->inotifyFd, buf, BUF_LEN);
			if (numRead == 0)
			{
				printf("read() from inotify fd returned 0!\n");
				exit(0);
			}

			if (numRead == -1)
			{
				printf("read\n");
				exit(0);
			}

			/* Process all of the events in buffer returned by read() */

			for (p = buf; p < buf + numRead; )
			{
				event = (struct inotify_event *) p;
				if (event->mask & IN_CLOSE_WRITE)
				{
					FILE *file;
					char fileName[100];
					strcpy(fileName, FOLDER);
					strcat(fileName, "/");
					strcat(fileName, event->name);
					file = fopen(fileName, "r");
					if (file!=NULL)
					{
						uint8_t signalStrength;
						fscanf(file, "%hhu", &signalStrength);
						fclose (file);
						uint8_t mac[6];
						sscanf(event->name, "%hhx-%hhx-%hhx-%hhx-%hhx-%hhx",
								&mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
//						printf("%hhu SRC:%hhx-%hhx-%hhx-%hhx-%hhx-%hhx\n",
//								signalStrength,
//								mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
						pthread_t thread;
						int rc;
						struct rssi_handle *handle = calloc(1, sizeof(struct rssi_handle));
						handle->mac = calloc(1, ETH_ALEN);
						memcpy(handle->mac, mac, ETH_ALEN);
						handle->rssi = signalStrength;
						rc = pthread_create(&thread, NULL, process_sta_data, (void *)handle);
						if (rc)
						{
							printf("ERROR; return code from pthread_create() is %d\n", rc);
//							exit(-1);
						}
					}
				}
				p += sizeof(struct inotify_event) + event->len;
			}
		}
	}
	return NULL;
}

static void *process_sta_data(void *rssi_msg)
{
	struct rssi_handle *handle = (struct rssi_handle *)rssi_msg;
	struct infoSTA *info = lookupSenderSTA(handle->mac);
	pthread_detach(pthread_self());
	if(info && info->status == ASSOCIATED)
	{
		changeRSSI(info, handle->rssi);
	}
	else
	{
		if(!info)
		{
			info = addSTA(handle->mac);
			if(!info)
	    	{
	    		return NULL;
	    	}
		    changeStatus(info, NOT_AUTHENTICATED);
	    }
		if(info->lastReceivedSignalStrength < handle->rssi)
		{
			changeRSSI(info, handle->rssi);
			size_t size = sizeof(struct ofp_wlan_header) + sizeof(struct ofp_wlan_rssi);
			void *experimenter = create_ofp_msg(size);
			if(experimenter == NULL)
			{
				//printf("Unable to handle OF message\n");
				return NULL;
			}
			struct ofp_wlan_header *wlan_header = (struct ofp_wlan_header *)experimenter;
			wlan_header->vendor = htonl(AP_EXPERIMENTER_ID);
			wlan_header->subtype = htonl(WLAN_REC_POW);
			char *strPtr = experimenter;
			strPtr += sizeof(struct ofp_wlan_header);
			struct ofp_wlan_rssi *wlan_data_rssi = (struct ofp_wlan_rssi *)strPtr;
			memcpy(wlan_data_rssi->staMacAddress, handle->mac, 6);
			memcpy(wlan_data_rssi->apMacAddress, controller->mac, 6);
			wlan_data_rssi->rssi = handle->rssi;
			controller_send(experimenter, ntohs(wlan_header->header.length));
		}
	}
	return NULL;
}

static void changeRSSI(struct infoSTA *sta, int rssi)
{
	pthread_mutex_lock(&controller->mutex);
	sta->lastReceivedSignalStrength = rssi;
	pthread_mutex_unlock(&controller->mutex);
}

static void handle_ap_rssi(struct ofp_wlan_sta *ap_RSSI)
{
	size_t size = sizeof(struct ofp_wlan_header) + sizeof(struct ofp_wlan_rssi);
	void *experimenter = create_ofp_msg(size);
	if(experimenter == NULL)
	{
		//printf("Unable to handle OF message\n");
		return;
	}
	struct ofp_wlan_header *wlan_header = (struct ofp_wlan_header *)experimenter;
	wlan_header->vendor = htonl(AP_EXPERIMENTER_ID);
	wlan_header->subtype = htonl(WLAN_AP_REC_POW);
	char *strPtr = experimenter;
	strPtr += sizeof(struct ofp_wlan_header);
	struct ofp_wlan_rssi *wlan_data_rssi = (struct ofp_wlan_rssi *)strPtr;
	struct infoSTA *staAP = lookupSenderSTA(ap_RSSI->staMacAddress);
	memcpy(wlan_data_rssi->staMacAddress, staAP->address, 6);
	memcpy(wlan_data_rssi->apMacAddress, controller->mac, 6);
	wlan_data_rssi->rssi = staAP->lastReceivedSignalStrength;
	controller_send(experimenter, ntohs(wlan_header->header.length));
}
