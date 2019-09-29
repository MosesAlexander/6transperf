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
	uint32_t self_ip4;
	uint32_t dest_ip4;
	uint64_t self_ip6;
	uint64_t dest_ip6;
	bool is_ipip6_tun_intf;
private:
	void SetMac(string &line, bool self);
	void SetIp(string &line, bool self);
	void SetIp6(string &line, bool self);
};

class Port
{
private:
	bool m_initialized = false;
	int m_port_id;
	unsigned int m_socket_id;
	int rx_queues;
	int tx_queues;
	PortConfig *m_config;
	
public:
	uint8_t mac_addr[6];
	int init(int num_queues, PortConfig *config);
	void send(int count);
	void construct_ipip6_packet(char *buf, int buf_len);
	void construct_ip_packet(char *buf, int buf_len);
	Port(int id) : m_port_id(id) { };
};


extern vector<Port*> ports_vector;

