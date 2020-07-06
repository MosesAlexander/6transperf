#include "port.h"

using namespace std;

const uint16_t RX_RING_SIZE = 512;
const uint16_t TX_RING_SIZE = 512;
const uint16_t NUM_BUFFERS = 1024*50;


unsigned int num_sockets;
bool tx_running;
bool rx_running;

uint16_t ports_ids[RTE_MAX_ETHPORTS]; 

static struct rte_eth_conf port_conf_default = {
	.rxmode = {
		.mq_mode = ETH_MQ_RX_RSS,
        	.max_rx_pkt_len = ETHER_MAX_LEN,
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
			.rss_hf = ETH_RSS_UDP | ETH_RSS_TCP | ETH_RSS_IPV6 | ETH_RSS_IPV4,
		},
	},
};
// For reference the RSS mask for Ethernet 10G 2P X520 Adapter is 0x38D34
// And in my experience the hash function cannot see the UDP packet within a tunneled
// packet..


// TODO: Calculate number of queues according to number of lcores
int Port::init(int num_queues, PortConfig *config) {
	int ret;
	struct rte_eth_dev_info query_info;

	rx_queues = tx_queues = num_queues;

	for (int i = 0; i < num_queues; i++)
	{
		rx_queue_index.push(i);
		tx_queue_index.push(i);
	}

	rte_eth_dev_info_get(m_port_id, &query_info);

	ret = rte_eth_dev_configure(m_port_id, rx_queues, tx_queues, &port_conf_default);
	if (ret != 0)
		return ret;

	m_socket_id = rte_eth_dev_socket_id(m_port_id);

	memcpy(mac_addr, rte_eth_devices[m_port_id].data->mac_addrs->addr_bytes, 6);

	m_config = config;

	string pool_name = string("pool_port");
	pool_name = pool_name + to_string(m_port_id);
	m_pool = rte_pktmbuf_pool_create(pool_name.c_str(),
							NUM_BUFFERS, MEMPOOL_CACHE_SIZE,
					0, RTE_MBUF_DEFAULT_BUF_SIZE, m_socket_id);
	if (m_pool == nullptr)
		rte_exit(EXIT_FAILURE, "Failed to init memory pool\n");

	for (int queue = 0; queue < rx_queues; queue++) {
		ret = rte_eth_rx_queue_setup(
			m_port_id,
			queue,
			RX_RING_SIZE,
			m_socket_id,
			NULL,
			m_pool);

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

	// token[1] should contain the ip adress
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
{
	vector<string> tokens; 
	stringstream check1(line); 
	string intermediate; 
	int ip_block = 0;
	uint16_t *ip6_addr;

	if (self) {
		ip6_addr = (uint16_t *) self_ip6;
	} else {
		ip6_addr = (uint16_t *) dest_ip6;
	}

	memset(ip6_addr, 0, 8 * sizeof(uint16_t));

	while(getline(check1, intermediate, '=')) 
	{ 
		tokens.push_back(intermediate); 
	} 

	for(int i = 0; i < tokens.size(); i++) 
		cout << tokens[i] << '\n'; 

	// token[1] should contain the ip6 address
	stringstream check2(tokens[1]);
	while(getline(check2, intermediate, ':') && ip_block < 8)
	{
		ip6_addr[ip_block] = rte_cpu_to_be_16(static_cast<uint16_t>(std::stoul(intermediate, nullptr, 16)));

		ip_block++;
	}
}

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

			if (line.find("tunnel")!=string::npos)
				if(line.find("=yes")!=string::npos)
					is_ipip6_tun_intf = true;

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

			if (line.find("tunnel")!=string::npos)
				if(line.find("=yes")!=string::npos)
					is_ipip6_tun_intf = true;

			cout << line << endl;
		}
	}
}


