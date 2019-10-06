#ifndef B4_H
#define B4_H

#include "port.h"
#include "router.h"

class DSLiteB4Router : public Router
{
public:
	void forward(void);
	void set_lcore_allocation(uint64_t port0_mask, uint64_t port1_mask);
	void set_ports_from_config(void);

	Port *local_port, *tunnel_port;
};

#endif /* B4_H */
