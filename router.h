#ifndef ROUTER_H
#define ROUTER_H

#include "port.h"


// Taken from FreeBSD
struct ip6_opt
{
	__u8 ip6o_type;
	__u8 ip6o_len;
} __attribute__((packed));

/* Tunnel Limit Option */
struct ip6_opt_tunnel
{
	__u8 ip6ot_type;
	__u8 ip6ot_len;
    __u8 ip6ot_encap_limit;
} __attribute__((packed));

struct ip6_opt_padn
{
	__u8 pad_type;
	__u8 pad_len;
	__u8 padn[1];
};

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
	void construct_ipip6_packet(PortConfig *config, char *buf, int buf_len);
	virtual void set_lcore_allocation(uint64_t port0_mask, uint64_t port1_mask) { }
	virtual void set_ports_from_config(void) { }

	void add_port(Port * port);

	virtual ~Router() = default;
};

#endif /* ROUTER_H */