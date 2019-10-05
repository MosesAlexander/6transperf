#ifndef ROUTER_H
#define ROUTER_H

#include "port.h"

class Router
{
public:
	vector<Port *> ports_vector;
	//TODO: Must support more lcores than 64
	uint64_t port0_lcore_mask = 0;
	uint64_t port1_lcore_mask = 0;

	void encapsulate_ipip6_packet(PortConfig *config, char *buf, int buf_len);
	void decapsulate_ipip6_packet(PortConfig *config, char *buf, int buf_len);
	void construct_ip6_packet(PortConfig *config, char *buf, int buf_len);
	void construct_ip_packet(PortConfig *config, char *buf, int buf_len);
	virtual void set_lcore_allocation(uint64_t port0_mask, uint64_t port1_mask) { }

	void add_port(Port * port);

	virtual ~Router() = default;
};

#endif /* ROUTER_H */
