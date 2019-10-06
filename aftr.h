#include "port.h"
#include "router.h"

class DSLiteAFTRRouter : public Router
{
	void set_lcore_allocation(uint64_t port0_mask, uint64_t port1_mask);
	void set_ports_from_config(void);

	Port *local_port, *tunnel_port;
};
