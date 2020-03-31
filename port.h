/*
 * Copyright (C) 2019 Alexandru Moise <00moses.alexander00@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public Licens
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-
 *
 */

#ifndef PORT_H
#define PORT_H

#include <iostream>
#include <csignal>
#include <unistd.h>
#include <numa.h>
#include <pthread.h>
#include <string>
#include <vector>
#include <sys/sysinfo.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <bits/stdc++.h>
#include <queue>
#include <mutex>
#include <atomic>
#include <sys/mman.h>
#include <fstream>

#include <rte_eal.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

using namespace std;

extern unsigned int num_sockets;
extern vector<struct rte_mempool *> mempools_vector;
extern uint16_t ports_ids[RTE_MAX_ETHPORTS];
extern bool tx_running;
extern bool rx_running;

extern const uint16_t RX_RING_SIZE;
extern const uint16_t TX_RING_SIZE;
extern const uint16_t NUM_BUFFERS;


#define ETH_SIZE_64 64
#define ETH_SIZE_128 128
#define ETH_SIZE_256 256
#define ETH_SIZE_512 512
#define ETH_SIZE_1024 1024
#define ETH_SIZE_1280 1280
#define ETH_SIZE_1518 1518
#define ETH_SIZE_1522 1522

#define IP_DEFTTL  64   /* from RFC 1340. */
#define IP_VERSION 0x40
#define IP_HDRLEN  0x05 /* default IP header length == five 32-bits words. */
#define IP_VHL_DEF (IP_VERSION | IP_HDRLEN)
#define MEMPOOL_CACHE_SIZE 32

#define MAX_PKT_BURST 512

/*
 * Default byte size for the IPv6 Maximum Transfer Unit (MTU).
 * This value includes the size of IPv6 header.
 */
#define IPV4_MTU_DEFAULT        ETHER_MTU
#define IPV6_MTU_DEFAULT        ETHER_MTU
/*
 * Default payload in bytes for the IPv6 packet.
 */
#define IPV4_DEFAULT_PAYLOAD    (IPV4_MTU_DEFAULT - sizeof(struct ipv4_hdr))
#define IPV6_DEFAULT_PAYLOAD    (IPV6_MTU_DEFAULT - sizeof(struct ipv6_hdr))


enum config_type_t {
	TESTER_CONFIG,
	B4_CONFIG,
	AFTR_CONFIG,
};

class PortConfig
{
public:
	PortConfig(char *config_file, int port, config_type_t config_type);
	uint8_t self_mac[6];
	uint8_t peer_mac[6];
	// IPv4 is stored in host byte order
	uint32_t self_ip4;
	uint32_t dest_ip4;
	// IPv6 is stored in network byte order
	uint8_t self_ip6[16];
	uint8_t dest_ip6[16];
	uint64_t lcore_mask = 0;
	bool is_ipip6_tun_intf = false;
private:
	void SetMac(string &line, bool self);
	void SetIp(string &line, bool self);
	void SetIp6(string &line, bool self);
};

class Port
{
public:
	bool m_initialized = false;
	int m_port_id;
	unsigned int m_socket_id;
	int rx_queues;
	int tx_queues;
	PortConfig *m_config;
	queue<int> rx_queue_index;
	queue<int> tx_queue_index;
	mutex rx_queue_mutex;
	mutex tx_queue_mutex;

	uint8_t mac_addr[6];
	int init(int num_queues, PortConfig *config);
	Port(int id) : m_port_id(id) { };
};

#endif /* PORT_H */

