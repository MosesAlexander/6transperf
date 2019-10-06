#include "b4.h"

void DSLiteB4Router::set_ports_from_config(void)
{
	if (ports_vector[0]->m_config->is_ipip6_tun_intf) {
		tunnel_port = ports_vector[0];
		local_port = ports_vector[1];
	} else if (ports_vector[1]->m_config->is_ipip6_tun_intf) {
		tunnel_port = ports_vector[1];
		local_port = ports_vector[0];
	}
}

// First get it to forward from port 0 to port 1, then set up the tunnel
void DSLiteB4Router::forward(void)
{
	unsigned lcore = rte_lcore_id();
	/*
	 * If  each port has 2 RX queues and 2 TX queues
	 * the traffic should be handled like this:
	 * lcore0: PORT0_RX0 -> PORT1_TX0
	 * lcore1: PORT0_RX1 -> PORT1_TX1
	 * lcore2: PORT1_RX0 -> PORT0_TX0
	 * lcore3: PORT1_RX1 -> PORT0_TX1
	 *
	 * In the B4 case we optimize for the RX side, so the socket0 threads will
	 * do RX on the socket 0 NIC and the socket1 threads will do RX on the
	 * socket1 NIC.
	 */
	if (port0_lcore_mask & (1 << lcore))
	{
		for (;;) {
			struct rte_mbuf *pktmbuf = rte_pktmbuf_alloc(mempools_vector[local_port->m_port_id]);
			struct rte_mbuf *pktsbuf[1];
			char *buf = rte_pktmbuf_append(pktmbuf, ETH_SIZE_1024);

			construct_ip6_packet(local_port->m_config, buf, ETH_SIZE_1024);

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

	if (port1_lcore_mask & (1 << lcore))
	{
		for (;;) {
			struct rte_mbuf *pktmbuf = rte_pktmbuf_alloc(mempools_vector[local_port->m_port_id]);
			struct rte_mbuf *pktsbuf[1];
			char *buf = rte_pktmbuf_append(pktmbuf, ETH_SIZE_1024);

			construct_ip6_packet(local_port->m_config, buf, ETH_SIZE_1024);

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
}

void DSLiteB4Router::set_lcore_allocation(uint64_t port0_mask, uint64_t port1_mask)
{

}
