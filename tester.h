#include "port.h"
#include "router.h"

class DSLiteTester : public Router
{
public:
	uint64_t port0_lcore_rx_mask;
	uint64_t port0_lcore_tx_mask;
	uint64_t port1_lcore_rx_mask;
	uint64_t port1_lcore_tx_mask;
	void testb4(void);
	void set_lcore_allocation(uint64_t port0_mask, uint64_t port1_mask);
};
