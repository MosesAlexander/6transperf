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

using namespace std;

extern unsigned int num_sockets;
extern vector<struct rte_mempool *> mempools_vector;
extern uint16_t ports_ids[RTE_MAX_ETHPORTS];

extern const uint16_t RX_RING_SIZE;
extern const uint16_t TX_RING_SIZE;
extern const uint16_t NUM_BUFFERS;


class Port
{
private:
	bool m_initialized = false;
	int m_port_id;
	unsigned int m_socket_id;
	int rx_queues;
	int tx_queues;
	uint8_t mac_addr[6];
	PortConfig *config;
	
public:
	int init(int num_queues);
	void send(int count);
	Port(int id) : m_port_id(id) { };
};


extern vector<Port*> ports_vector;

class PortConfig
{
public:
        PortConfig(char *config_file, int port);
        uint8_t self_mac[6];
        uint8_t peer_mac[6];
private:
	void SetMac(string &line, bool self);
};


