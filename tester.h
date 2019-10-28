#include "port.h"
#include "router.h"

enum dslite_test_mode_t {
	AFTR,
	B4,
	BOTH,
};

class DSLiteTester : public Router
{
public:
	uint64_t local_lcore_rx_mask;
	uint64_t local_lcore_tx_mask;
	uint64_t tunnel_lcore_rx_mask;
	uint64_t tunnel_lcore_tx_mask;
	Port *local_port, *tunnel_port;

	void testaftr(void);
	void testb4(void);
	void set_ports_from_config(void);
	void set_lcore_allocation(uint64_t port0_mask, uint64_t port1_mask);
};
