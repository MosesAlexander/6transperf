#include "tester.h"

void DSLiteTester::testb4(void)
{
	Port *local_port, *tunnel_port;

	if (ports_vector[0]->m_config->is_ipip6_tun_intf) {
		tunnel_port = ports_vector[0];
		local_port = ports_vector[1];
	} else if (ports_vector[1]->m_config->is_ipip6_tun_intf) {
		tunnel_port = ports_vector[1];
		local_port = ports_vector[0];
	}

	/*
	 * We do a 1:1 mapping between queue and lcore, so in the tester case
	 * you should lcore_num = 2 * queue_num * ports_num
	 */
	for (;;) {
		struct rte_mbuf *pktmbuf = rte_pktmbuf_alloc(mempools_vector[local_port->m_port_id]);
		struct rte_mbuf *pktsbuf[1];
		char *buf = rte_pktmbuf_append(pktmbuf, ETH_SIZE_1024);

		construct_ip_packet(local_port->m_config, buf, ETH_SIZE_1024);

		pktsbuf[0] = pktmbuf;

		/* Send burst of TX packets, to second port of pair. */
		uint16_t nb_tx = rte_eth_tx_burst(local_port->m_port_id, 0, pktsbuf, 1);
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

int split_mask_in_two(uint64_t mask)
{
        // Count the number of bits set
        int count = 0, split = 0;

        for (int i = 0; i < 64; i++)
        {
                if (mask & (0x1ULL<<i))
                        count++;
        }

        split = count / 2;
        count = 0;

        for (int i = 0; i < 64; i++)
        {
                if (mask & (0x1ULL<<i))
                {
                        count++;
                        if (count == split)
                                return (i+1);
                }
        }

        return 0;
}

void DSLiteTester::set_lcore_allocation(uint64_t port0_mask, uint64_t port1_mask)
{
		uint64_t split_mask0 = 0xffffffffffffffffULL;
		uint64_t split_mask1 = 0xffffffffffffffffULL;
		int cut1, cut2;
		uint64_t split_lcore_port0[2];
		uint64_t split_lcore_port1[2];

		cut1 = split_mask_in_two(port0_mask);
		cut2 = split_mask_in_two(port1_mask);

		port0_lcore_rx_mask = (port0_mask &  (split_mask0 << cut1));
		port0_lcore_tx_mask = (port0_mask & ~(split_mask0 << cut1));

		port1_lcore_rx_mask = (port1_mask &  (split_mask0 << cut2));
		port1_lcore_tx_mask = (port1_mask & ~(split_mask0 << cut2));
}

