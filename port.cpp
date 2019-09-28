#include "port.h"

using namespace std;

const uint16_t RX_RING_SIZE = 512;
const uint16_t TX_RING_SIZE = 512;
const uint16_t NUM_BUFFERS = 1024;


unsigned int num_sockets;
vector<struct rte_mempool *> mempools_vector;
vector<Port*> ports_vector;

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


void PortConfig::SetMac(string &line, bool self)
{
	vector<string> tokens; 
	stringstream check1(line); 
	string intermediate; 
	int mac_byte = 0;

	while(getline(check1, intermediate, '=')) 
	{ 
		tokens.push_back(intermediate); 
	} 

	for(int i = 0; i < tokens.size(); i++) 
		cout << tokens[i] << '\n'; 

	// token[1] should contain the mac adress
	stringstream check2(tokens[1]);
	while(getline(check2, intermediate, ':'))
	{
		if(self) {
			self_mac[mac_byte] = static_cast<uint8_t>(
					std::stoul(intermediate, nullptr, 16));
		} else {
			peer_mac[mac_byte] = static_cast<uint8_t>(
					std::stoul(intermediate, nullptr, 16));
		}

		mac_byte++;
	}
}

PortConfig::PortConfig(char *config_file, int port)
{
	ifstream infile(config_file);
	string line;

	while (getline(infile, line))
	{
		if ((line.find("port0")!=string::npos) && port == 0)
		{
			if (line.find("self")!=string::npos)
				SetMac(line, true);
			else if (line.find("peer")!=string::npos)
				SetMac(line, false);

			cout << line << endl;
		}
		else if ((line.find("port1")!=string::npos) && port == 1)
		{
			if (line.find("self")!=string::npos)
				SetMac(line, true);
			else if (line.find("peer")!=string::npos)
				SetMac(line, false);

			cout << line << endl;
		}
	}
}


