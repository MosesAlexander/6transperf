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

void Port::construct_ip_packet(char *buf, int buf_len)
{
	struct ether_hdr *hdr;
	uint16_t *ptr16;
	uint32_t ip_cksum;
	uint16_t data_len = buf_len - sizeof(struct ether_hdr) - sizeof(struct ipv4_hdr);
	uint16_t pkt_len;
	struct ipv4_hdr *ip_hdr;
	struct udp_hdr *udp_hdr;

	hdr = (struct ether_hdr *)buf;
	ip_hdr = (struct ipv4_hdr *)(buf+(sizeof(struct ether_hdr)));
	udp_hdr = (struct udp_hdr *)(buf+(sizeof(struct ether_hdr))+(sizeof(struct ipv4_hdr)));

	memcpy(hdr->d_addr.addr_bytes, m_config->peer_mac,6);
	memcpy(hdr->s_addr.addr_bytes, mac_addr, 6);

	/* IPv4 */
	hdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);

	/* IPv4 header */
	pkt_len = (uint16_t) (data_len + sizeof(struct ipv4_hdr)); // data + ip header
	ip_hdr->version_ihl   = IP_VHL_DEF;
	ip_hdr->type_of_service   = 0;
	ip_hdr->fragment_offset = 0;
	ip_hdr->time_to_live   = IP_DEFTTL;
	ip_hdr->next_proto_id = IPPROTO_UDP;
	ip_hdr->packet_id = 0;
	ip_hdr->total_length   = rte_cpu_to_be_16(pkt_len);
	ip_hdr->src_addr = rte_cpu_to_be_32(m_config->self_ip4);
	ip_hdr->dst_addr = rte_cpu_to_be_32(m_config->dest_ip4);

	/*
	 * Compute IP header checksum.
	 */
	ptr16 = (unaligned_uint16_t*) ip_hdr;
	ip_cksum = 0;
	ip_cksum += ptr16[0]; ip_cksum += ptr16[1];
	ip_cksum += ptr16[2]; ip_cksum += ptr16[3];
	ip_cksum += ptr16[4];
	ip_cksum += ptr16[6]; ip_cksum += ptr16[7];
	ip_cksum += ptr16[8]; ip_cksum += ptr16[9];

	/*
	 * Reduce 32 bit checksum to 16 bits and complement it.
	 */
	ip_cksum = ((ip_cksum & 0xFFFF0000) >> 16) +
		(ip_cksum & 0x0000FFFF);
	if (ip_cksum > 65535)
		ip_cksum -= 65535;
	ip_cksum = (~ip_cksum) & 0x0000FFFF;
	if (ip_cksum == 0)
		ip_cksum = 0xFFFF;
	ip_hdr->hdr_checksum = (uint16_t) ip_cksum;

	// Add some udp payload, we won't really care about this, unless
	// we want to add some random data here and checksum it to test
	// for data integrity between the NICs
	udp_hdr->src_port = rte_cpu_to_be_16(1024);
	udp_hdr->dst_port = rte_cpu_to_be_16(1024);
	udp_hdr->dgram_len      = rte_cpu_to_be_16(data_len);
	udp_hdr->dgram_cksum    = 0; /* No UDP checksum. */
}

void Port::send(int count)
{
	for (;;) {
		struct rte_mbuf *pktmbuf = rte_pktmbuf_alloc(mempools_vector[m_port_id]);
		struct rte_mbuf *pktsbuf[1];
		char *buf = rte_pktmbuf_append(pktmbuf, ETH_SIZE_1024);

		construct_ip_packet(buf, ETH_SIZE_1024);

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
int Port::init(int num_queues, PortConfig *config) {
	int ret;
	struct rte_mempool *pool;

	rx_queues = tx_queues = num_queues;

	ret = rte_eth_dev_configure(m_port_id, rx_queues, tx_queues, &port_conf_default);
	if (ret != 0)
		return ret;

	m_socket_id = rte_eth_dev_socket_id(m_port_id);

	memcpy(mac_addr, rte_eth_devices[m_port_id].data->mac_addrs->addr_bytes, 6);

	m_config = config;

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

void PortConfig::SetIp(string &line, bool self)
{
	vector<string> tokens; 
	stringstream check1(line); 
	string intermediate; 
	int ip_byte = 3;

	if (self)
		self_ip4 = 0;
	else
		dest_ip4 = 0;

	while(getline(check1, intermediate, '=')) 
	{ 
		tokens.push_back(intermediate); 
	} 

	for(int i = 0; i < tokens.size(); i++) 
		cout << tokens[i] << '\n'; 

	// token[1] should contain the mac adress
	stringstream check2(tokens[1]);
	while(getline(check2, intermediate, '.'))
	{
		int shift = 8 * ip_byte;
		if(self) {
			self_ip4 |= (static_cast<uint8_t>(
					std::stoul(intermediate, nullptr, 10)) << shift);
		} else {
			dest_ip4 |= (static_cast<uint8_t>(
					std::stoul(intermediate, nullptr, 10)) << shift);
		}

		ip_byte--;
	}

}

void PortConfig::SetIp6(string &line, bool self)
{ }

PortConfig::PortConfig(char *config_file, int port, config_type_t config_type)
{
	ifstream infile(config_file);
	string line;

	while (getline(infile, line))
	{
		switch(config_type)
		{
		case TESTER_CONFIG:
			if(!(line.find("tester_")!=string::npos))
				continue;
			break;
		case B4_CONFIG:
			if(!(line.find("b4_")!=string::npos))
				continue;
			break;
		case AFTR_CONFIG:
			if(!(line.find("aftr_")!=string::npos))
				continue;
			break;
		}

		if ((line.find("port0")!=string::npos) && port == 0)
		{
			if (line.find("self")!=string::npos) {
				if (line.find("_mac_")!=string::npos)
					SetMac(line, true);
				else if (line.find("_ip_")!=string::npos)
					SetIp(line, true);
				else if (line.find("_ip6_")!=string::npos)
					SetIp6(line, true);
			} else if (line.find("peer")!=string::npos ||
					line.find("dest")!=string::npos) {
				if (line.find("_mac_")!=string::npos)
					SetMac(line, false);
				else if (line.find("_ip_")!=string::npos)
					SetIp(line, false);
				else if (line.find("_ip6_")!=string::npos)
					SetIp6(line, false);
			}

			cout << line << endl;
		}
		else if ((line.find("port1")!=string::npos) && port == 1)
		{
			if (line.find("self")!=string::npos) {
				if (line.find("_mac_")!=string::npos)
					SetMac(line, true);
				else if (line.find("_ip_")!=string::npos)
					SetIp(line, true);
				else if (line.find("_ip6_")!=string::npos)
					SetIp6(line, true);
			} else if (line.find("peer")!=string::npos ||
					line.find("dest")!=string::npos) {
				if (line.find("_mac_")!=string::npos)
					SetMac(line, false);
				else if (line.find("_ip_")!=string::npos)
					SetIp(line, false);
				else if (line.find("_ip6_")!=string::npos)
					SetIp6(line, false);
			}

			cout << line << endl;
		}
	}
}


