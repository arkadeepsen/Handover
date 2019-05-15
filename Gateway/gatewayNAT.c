#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/netfilter.h>            /* for NF_ACCEPT */

#include <libnetfilter_queue/libnetfilter_queue.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>

#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <fcntl.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <limits.h>
#include "openflow.h"
#include "nicira-ext.h"
#include "nicira-new.h"
#include <time.h>

#define MAXENTRY 0xff
#define HASH_FUNC(addr) addr[3]

#define SOCK_ADDR "127.0.0.1"
//#define SOCK_ADDR "10.6.21.107"
#define SOCK_PORT 6653

#define IFACE "eth0"
#define MAX_ADDR_LEN 32

#define AP_EXPERIMENTER_ID 0x00000001

int loop_terminate;
unsigned char *ifIP;
char subnet[50];
static struct controller_connection *controller;

enum gw_subtype {
	WLAN_NAT_ADD = 20,
	GW_CONNECT = 100
};

struct ofp_wlan_header {
    struct ofp_header header;
    uint32_t vendor;            /* AP_EXPERIMENTER_ID. */
    uint32_t subtype;           /* One of WLAN_* above. */
};
OFP_ASSERT(sizeof(struct ofp_wlan_header) == sizeof(struct ofp_vendor_header) + 4);

struct ofp_nat_entry {
    uint8_t staMacAddress[6]; /* STA MAC address. */
    uint8_t pad[2];
	uint32_t nw_src;           /* IP source address. */
	uint32_t nw_dst;           /* IP destination address. */
	uint16_t tp_src;           /* TCP/UDP source port. */
	uint16_t tp_dst;           /* TCP/UDP destination port. */
};
OFP_ASSERT(sizeof(struct ofp_nat_entry) == 20);

struct controller_connection
{
	int sock;
	int error;
	uint64_t dpid;
	uint16_t miss_send_len;
	uint8_t *mac;

	pthread_mutex_t mutex;
};

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
	uint16_t port;
	unsigned char src_addr[4];
	unsigned char dst_addr[4];
	uint16_t src_port;
	uint16_t dst_port;
	
	struct natMap *next;
};

struct natList {
	struct natMap **table;
};

static struct natList *entryList;
pthread_mutex_t mutex;

static inline uint64_t htonll(uint64_t n)
{
    return htonl(1) == 1 ? n : ((uint64_t) htonl(n) << 32) | htonl(n >> 32);
}


struct natMap *lookupNatMap(uint16_t src_port, unsigned char dst_addr[4],
		uint16_t dst_port)
{
	struct natMap *nMap;
	pthread_mutex_lock(&mutex);
	nMap = entryList->table[HASH_FUNC(dst_addr)];
	while (nMap != NULL && (memcmp(nMap->dst_addr, dst_addr, 4) != 0
			|| nMap->src_port != src_port || nMap->dst_port != dst_port))
		nMap = nMap->next;
	pthread_mutex_unlock(&mutex);
	return nMap;
}

struct natMap *addEntry(uint16_t src_port, unsigned char dst_addr[4], uint16_t dst_port)
{
	struct natMap *nMap;
	
	nMap = lookupNatMap(src_port, dst_addr, dst_port);
	if (nMap)
		return nMap;

	nMap = calloc(1, sizeof(struct natMap));
	if (nMap == NULL) {
		//printf("Calloc failed\n");
		return NULL;
	}
	nMap->src_port = src_port;
	nMap->dst_port = dst_port;
	memcpy(nMap->dst_addr, dst_addr, 4);
	pthread_mutex_lock(&mutex);
	nMap->next = entryList->table[HASH_FUNC(dst_addr)];
	entryList->table[HASH_FUNC(dst_addr)] = nMap;
	pthread_mutex_unlock(&mutex);
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

/* returns packet id */
static int handle_pkt(struct nfq_data *tb, int queueNum, struct nfq_q_handle *qh)
{
	int id = 0;
	struct nfqnl_msg_packet_hdr *ph;
	//~ struct nfqnl_msg_packet_hw *hwph;
	//~ u_int32_t mark,ifi; 
	int ret;
	unsigned char *data;

	ph = nfq_get_msg_packet_hdr(tb);
	if (ph) {
		id = ntohl(ph->packet_id);
	}

	ret = nfq_get_payload(tb, &data);
	if (ret >= 0)
	{
//		 printf("payload_len=%d ", ret);
		struct ip_header_t *ipH = (struct ip_header_t *)data;
		char srcIP[50], dstIP[50];
		//~ printf("Header Length=%d ", (ipH->ver_ihl & 0x0f)*4);
		sprintf(srcIP, "%hhu.%hhu.%hhu.%hhu", ipH->src_addr[0], ipH->src_addr[1], ipH->src_addr[2], ipH->src_addr[3]);
		sprintf(dstIP, "%hhu.%hhu.%hhu.%hhu", ipH->dst_addr[0], ipH->dst_addr[1], ipH->dst_addr[2], ipH->dst_addr[3]);
		if(queueNum == 0)
		{
			if(strncmp(srcIP, subnet, strlen(subnet)) == 0)
			{
				struct transport_header *tH = (struct transport_header *)(data + (ipH->ver_ihl & 0x0f)*4);
				struct natMap *nMap = lookupNatMap(tH->src_port, ipH->dst_addr, tH->dst_port);
				if(!nMap)
				{
					nMap = addEntry(tH->src_port, ipH->dst_addr, tH->dst_port);
				}
				memcpy(nMap->src_addr, ipH->src_addr, 4);
				memcpy(ipH->src_addr, ifIP, 4);
				printf("Source IP=%hhu.%hhu.%hhu.%hhu ", ipH->src_addr[0], ipH->src_addr[1], ipH->src_addr[2], ipH->src_addr[3]);
				printf("Dest IP=%hhu.%hhu.%hhu.%hhu ", ipH->dst_addr[0], ipH->dst_addr[1], ipH->dst_addr[2], ipH->dst_addr[3]);
				printf("Transport Protocol=%d ", ipH->protocol);
				printf("Source port=%d ", ntohs(tH->src_port));
				printf("Dest port=%d ", ntohs(tH->dst_port));
			}
			else
				return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
		}
		else if(queueNum == 1)
		{
			if(memcmp(ipH->dst_addr, ifIP, 4) == 0)
			{
				struct transport_header *tH = (struct transport_header *)(data + (ipH->ver_ihl & 0x0f)*4);
				struct natMap *nMap = lookupNatMap(tH->dst_port, ipH->src_addr, tH->src_port);
				if(nMap)
				{
					memcpy(ipH->dst_addr, nMap->src_addr, 4);
					//~ tH->dst_port = nMap->src_port;
					printf("Source IP=%hhu.%hhu.%hhu.%hhu ", ipH->src_addr[0], ipH->src_addr[1], ipH->src_addr[2], ipH->src_addr[3]);
					printf("Dest IP=%hhu.%hhu.%hhu.%hhu ", ipH->dst_addr[0], ipH->dst_addr[1], ipH->dst_addr[2], ipH->dst_addr[3]);
					printf("Transport Protocol=%d ", ipH->protocol);
					printf("Source port=%d ", ntohs(tH->src_port));
					printf("Dest port=%d ", ntohs(tH->dst_port));
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
		int odd = ntohs(ipH->total_length) % 2;
		if(ipH->protocol == 17)
		{
			struct udphdr *udpH = (struct udphdr *)(data + (ipH->ver_ihl & 0x0f)*4);
			checksum = calculateCheckSum((data + (ipH->ver_ihl & 0x0f)*4),
					(ntohs(ipH->total_length) - (ipH->ver_ihl & 0x0f)*4)/2, headerSum, 3, odd);
			if(ntohs(udpH->uh_sum) != checksum)
				udpH->uh_sum = htons(checksum);
			fputc('\n', stdout);
			printf("entering callback=%d\n", queueNum);
			return nfq_set_verdict(qh, id, NF_ACCEPT, ret, data);
		}
		else if(ipH->protocol == 6)
		{
			struct tcphdr *tcpH = (struct tcphdr *)(data + (ipH->ver_ihl & 0x0f)*4);
			checksum = calculateCheckSum((data + (ipH->ver_ihl & 0x0f)*4),
					(ntohs(ipH->total_length) - (ipH->ver_ihl & 0x0f)*4)/2, headerSum, 8, odd);
			if(ntohs(tcpH->th_sum) != checksum)
				tcpH->th_sum = htons(checksum);
			fputc('\n', stdout);
			printf("entering callback=%d\n", queueNum);
			return nfq_set_verdict(qh, id, NF_ACCEPT, ret, data);
		}
		
		fputc('\n', stdout);
		printf("entering callback=%d\n", queueNum);
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
	//~ printf("UINT_MAX:%u\n", INT_MAX);
	nfnl_rcvbufsiz(nh, 10000*1500);
	fd = nfnl_fd(nh);
	while (loop_terminate && rv >= 0) {
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
				loop_terminate = 0;
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

int controller_conn_init()
{
	controller = (struct controller_connection *)calloc(1,
			sizeof(struct controller_connection));
	if(controller == NULL)
	{
		//printf("Unable to allocate memory for controller struct \n");
		return 0;
	}
	controller->sock = -1;
	
	pthread_mutex_init(&controller->mutex, NULL);
	return 1;
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
		case WLAN_NAT_ADD:
			strPtrMsg = (char *)(void *)oh;
			strPtrMsg += sizeof(struct ofp_wlan_header);
			struct ofp_nat_entry *ofp_nat_add = (struct ofp_nat_entry *)strPtrMsg;
		    printf("WLAN_NAT_ADD\n");
		    unsigned char src_addr[4], dst_addr[4];
		    memcpy(src_addr, &ofp_nat_add->nw_src, 4);
		    memcpy(dst_addr, &ofp_nat_add->nw_dst, 4);
			struct natMap *nat =lookupNatMap(ofp_nat_add->tp_src, dst_addr, ofp_nat_add->tp_src);
			if(!nat)
			{
				nat = addEntry(ofp_nat_add->tp_src, dst_addr, ofp_nat_add->tp_src);
			}
			memcpy(nat->src_addr, src_addr, 4);
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



static void handle_flow_mod(struct ofp_header *oh)
{
	struct ofp_flow_mod *flow_mod = (struct ofp_flow_mod *)oh;
//	void *msg;
//	size_t size;
	switch (ntohs(flow_mod->command))
	{
	case OFPFC_ADD:
		//~ add_flows(flow_mod);
		break;
	case OFPFC_DELETE:
	{	//~ delete_flows(flow_mod);
		char *msg = calloc(1, sizeof(struct ofp_wlan_header));
		if(msg == NULL)
		{
			//printf("Unable to handle OF message\n");
			return;
		}
		struct ofp_wlan_header *gw_msg = (struct ofp_wlan_header *)msg;
		gw_msg->header.version = OFP_VERSION;
		gw_msg->header.type = OFPT_VENDOR;
		time_t t;
		srand((unsigned) time(&t));
		gw_msg->header.xid = htons(rand());
		gw_msg->header.length = htons(sizeof(struct ofp_wlan_header));
		gw_msg->vendor = ntohl(AP_EXPERIMENTER_ID);
		gw_msg->subtype = ntohl(GW_CONNECT);
		controller_send(msg, ntohs(gw_msg->header.length));
		break;
	}
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

void *controller_run()
{
	pthread_detach(pthread_self());
	while(loop_terminate)
	{
		controller_conn_open();
		controller_receive();
	}
	return NULL;
}

void controller_conn_close(void)
{
    close(controller->sock);
    pthread_mutex_destroy(&controller->mutex);
    if(controller != NULL)
    	free(controller);
}

/* Signal Handler for SIGINT */
void sigintHandler(int sig_num)
{
	printf("Program Terminate\n");
	loop_terminate = 0;
}

int main(int argc, char **argv)
{
	int fd;
	struct ifreq ifr;

	fd = socket(AF_INET, SOCK_DGRAM, 0);

	/* I want to get an IPv4 IP address */
	ifr.ifr_addr.sa_family = AF_INET;

	/* I want IP address attached to "eth0" */
	strncpy(ifr.ifr_name, argv[1], IFNAMSIZ-1);

	int t = ioctl(fd, SIOCGIFADDR, &ifr);

	close(fd);

	if(t != 0)
	{
		printf("Interface not up\n");
		return 1;
	}
	/* display result */
	ifIP = (unsigned char *)&((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
	struct in_addr addr;
	if (inet_aton(argv[2], &addr) == 0)
	{
        printf("Invalid address\n");
		return 1;
    }
    unsigned char *caddr;
    caddr = (unsigned char *)&addr;
    sprintf(subnet, "%hhu.%hhu.%hhu", caddr[0], caddr[1], caddr[2]);
    
    entryList = calloc(1, sizeof(struct natList));
    entryList->table = calloc(MAXENTRY, sizeof(struct natMap *));
    
    pthread_mutex_init(&mutex, NULL);
	loop_terminate = 1;
	signal(SIGINT, sigintHandler);
	int isReady = controller_conn_init();
	pthread_t controllerThread;
	if(isReady)
	{
		int rVal;
		rVal = pthread_create(&controllerThread, NULL, controller_run, NULL);
		if (rVal)
		{
			printf("ERROR; return code from pthread_create() is %d\n", rVal);
		}
	}
	pthread_t thread1;
	int rc;
	int queueNum1 = 0;
	rc = pthread_create(&thread1, NULL, recvPkt, &queueNum1);
	if (rc)
	{
		printf("ERROR; return code from pthread_create() is %d\n", rc);
//		exit(1);
	}
	pthread_t thread2;
	int queueNum2 = 1;
	rc = pthread_create(&thread2, NULL, recvPkt, &queueNum2);
	if (rc)
	{
		printf("ERROR; return code from pthread_create() is %d\n", rc);
//		exit(1);
	}

	pthread_join(controllerThread, NULL);
	controller_conn_close();
	pthread_join(thread1, NULL);
	pthread_join(thread2, NULL);

	return 0;
}
