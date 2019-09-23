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

#include <rte_eal.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_ether.h>

using namespace std;

const uint16_t RX_RING_SIZE = 512;
const uint16_t TX_RING_SIZE = 512;
const uint16_t NUM_BUFFERS = 1024;


class Port
{
private:
	bool m_initialized = false;
	int m_port_id;
	unsigned int m_socket_id;
	int rx_queues;
	int tx_queues;
	uint8_t mac_addr[6];
	
public:
	int init(int num_queues);
	void send(int count);
	Port(int id) : m_port_id(id) { };
};

unsigned int num_sockets;
vector<struct rte_mempool *> mempools_vector;
vector<Port*> ports_vector;

int transperf_logtype;
uint16_t ports_ids[RTE_MAX_ETHPORTS]; 

static struct rte_eth_conf port_conf_default = {
	.rxmode = {
        	.max_rx_pkt_len = ETHER_MAX_LEN
	},
};


void Port::send(int count)
{
	for (;;) {
		struct rte_mbuf *pktmbuf = rte_pktmbuf_alloc(mempools_vector[m_port_id]);
		char *buf = rte_pktmbuf_append(pktmbuf, 128);
		struct ether_hdr *hdr;

		// TODO: configure dest mac addr
		// TODO: packets should be IPv4 in IPv6 to test B4 element out port
		//	 IPv4 only for in port
		if (m_port_id == 0)
		{
			// Port 0 peer MAC is 52:54:09:11:24:39
			hdr = (struct ether_hdr *)buf;
			hdr->d_addr.addr_bytes[0] = 0x52;
			hdr->d_addr.addr_bytes[1] = 0x54;
			hdr->d_addr.addr_bytes[2] = 0x09;
			hdr->d_addr.addr_bytes[3] = 0x11;
			hdr->d_addr.addr_bytes[4] = 0x24;
			hdr->d_addr.addr_bytes[5] = 0x39;
		}
		else if (m_port_id == 1)
		{
			// Port 1 peer MAC is 52:54:02:12:31:09
			hdr = (struct ether_hdr *)buf;
			hdr->d_addr.addr_bytes[0] = 0x52;
			hdr->d_addr.addr_bytes[1] = 0x54;
			hdr->d_addr.addr_bytes[2] = 0x02;
			hdr->d_addr.addr_bytes[3] = 0x12;
			hdr->d_addr.addr_bytes[4] = 0x31;
			hdr->d_addr.addr_bytes[5] = 0x09;
		}

		memcpy(hdr->s_addr.addr_bytes, mac_addr, 6);

		/* IPv4 */
		hdr->ether_type = 0x0800;

		struct rte_mbuf *pktsbuf[1];

		pktsbuf[0] = pktmbuf;

		/* Send burst of TX packets, to second port of pair. */
		uint16_t nb_tx = rte_eth_tx_burst(m_port_id, 0, pktsbuf, 1);

		rte_pktmbuf_free(pktsbuf[0]);
		/* Free any unsent packets. */
		/*
		if (unlikely(nb_tx < nb_rx)) {
		    uint16_t bufno;
		    for (bufno = nb_tx; bufno < nb_rx; bufno++)
		}
		*/

		rte_delay_ms(2000);
	}

}


// TODO: Calculate number of queues according to number of lcores
int Port::init(int num_queues) {
	int ret;
	struct rte_mempool *pool;

	rx_queues = tx_queues = num_queues;


	ret = rte_eth_dev_configure(m_port_id, rx_queues, tx_queues, &port_conf_default);
	if (ret != 0)
		return ret;

	m_socket_id = rte_eth_dev_socket_id(m_port_id);

	memcpy(mac_addr, rte_eth_devices[m_port_id].data->mac_addrs->addr_bytes, 6);

	pool = mempools_vector[m_socket_id];
	
	for (int queue = 0; queue < rx_queues; queue++) {
		ret = rte_eth_rx_queue_setup(
			m_port_id,
			queue,
			RX_RING_SIZE,
			m_socket_id,
			NULL,
			pool);

		if (ret < 0)
			return ret;
	}

	/* Port TX */
	for (int queue = 0; queue < tx_queues; queue++) {
		ret = rte_eth_tx_queue_setup(
			m_port_id,
			queue,
			TX_RING_SIZE,
			m_socket_id,
			NULL);

		if (ret < 0)
			return ret;
	}

	ret = rte_eth_dev_start(m_port_id);
	if (ret != 0)
		return ret;

	rte_eth_promiscuous_enable(m_port_id);

	m_initialized = true;
	return 0;
}


static void signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		cout<<"\nSignal "<< signum <<" received, preparing to exit..."<<endl;
		//force_quit();
		/* Set flag to indicate the force termination. */
		//f_quit = 1;
		/* exit with the expected status */
		signal(signum, SIG_DFL);
		kill(getpid(), signum);
	}

}

int
traffic_lcore_thread(void *arg __rte_unused)
{
	int socket_id = rte_socket_id();
	Port *port = ports_vector[socket_id];

	port->send(10);
}

int main(int argc, char **argv)
{
	int diag, ret, num_ports;
	uint16_t port_id;
	int lcore_id;

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);


	diag = rte_eal_init(argc, argv);	
	if (diag < 0)
		rte_panic("Cannot init EAL\n");

	transperf_logtype = rte_log_register("transperf");
	if (transperf_logtype < 0)
		rte_panic("Cannot register log type");
	rte_log_set_level(transperf_logtype, RTE_LOG_DEBUG);

	for (int i = 1 ; i < argc; i++)
	{
		int arg;

		if (argv[i] == string("--num-ports"))
		{
			arg = atoi(argv[i+1]);
			num_ports = arg;
		}
	}

	num_sockets = rte_socket_count();

	for (int i = 0; i < num_sockets; i++) {
		string pool_name = string("pool_");
		pool_name = pool_name + to_string(i);
		struct rte_mempool *pool = rte_pktmbuf_pool_create(pool_name.c_str(),
								NUM_BUFFERS, RTE_CACHE_LINE_SIZE,
						0, RTE_MBUF_DEFAULT_BUF_SIZE, i);
		if (pool == nullptr)
			rte_exit(EXIT_FAILURE, "Failed to init memory pool\n");

		mempools_vector.push_back(pool);
	}


	for (int i = 0; i < num_ports; i++)
	{
		Port *port = new Port(i);
		port->init(1);
		ports_vector.push_back(port);
	}

	rte_eal_mp_remote_launch(traffic_lcore_thread, NULL, SKIP_MASTER);

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		ret |= rte_eal_wait_lcore(lcore_id);
	}

out:
	for (vector<Port*>::iterator it = ports_vector.begin() ; it != ports_vector.end(); ++it)
		delete *it;

	return 0;
}
