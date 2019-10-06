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
		struct rte_mbuf *tx_buffers[TX_BURST];
		struct rte_mbuf *rx_buffers[RX_BURST];
		int received_pkts = 0;
		int transmit_pkts = 0;
		int tunnelled_pkts = 0;
		int tx_idx = 0;
		char *buf;
		int queue_num;

		local_port->rx_queue_mutex.lock();
		if (!local_port->rx_queue_index.empty())
		{
			queue_num = local_port->rx_queue_index.front();
			local_port->rx_queue_index.pop();
		}
		local_port->rx_queue_mutex.unlock();

		for (;;)
		{
			received_pkts = rte_eth_rx_burst(local_port->m_port_id, queue_num, rx_buffers, RX_BURST);

			while (received_pkts > 0 && tx_idx < received_pkts)
			{
				tx_buffers[tunnelled_pkts] = rx_buffers[tx_idx];
				tunnelled_pkts++;

				buf = rte_pktmbuf_prepend(rx_buffers[tx_idx], IPIP6_OVERHEAD_BYTES);

				encapsulate_ipip6_packet(tunnel_port->m_config, buf, ETH_SIZE_1024+IPIP6_OVERHEAD_BYTES);

				if (tunnelled_pkts == TX_BURST)
				{
					transmit_pkts = rte_eth_tx_burst(tunnel_port->m_port_id, queue_num, tx_buffers, TX_BURST);

					if (transmit_pkts < TX_BURST)
					{
						for(int i = transmit_pkts; i < TX_BURST; i++)
						{
							rte_pktmbuf_free(tx_buffers[i]);
						}
					}
				}
				tx_idx++;
			}

		}

		local_port->rx_queue_mutex.lock();
		local_port->rx_queue_index.push(queue_num);
		local_port->rx_queue_mutex.unlock();
	}

	if (port1_lcore_mask & (1 << lcore))
	{
		struct rte_mbuf *tx_buffers[TX_BURST];
		struct rte_mbuf *rx_buffers[RX_BURST];
		int received_pkts = 0;
		int transmit_pkts = 0;
		int tunnelled_pkts = 0;
		int tx_idx = 0;
		char *buf;
		int queue_num;

		tunnel_port->rx_queue_mutex.lock();
		if (!tunnel_port->rx_queue_index.empty())
		{
			queue_num = tunnel_port->rx_queue_index.front();
			tunnel_port->rx_queue_index.pop();
		}
		tunnel_port->rx_queue_mutex.unlock();

		for (;;)
		{
			received_pkts = rte_eth_rx_burst(tunnel_port->m_port_id, queue_num, rx_buffers, RX_BURST);

			while (received_pkts > 0 && tx_idx < received_pkts)
			{
				tx_buffers[tunnelled_pkts] = rx_buffers[tx_idx];
				tunnelled_pkts++;

				buf = rte_pktmbuf_adj(rx_buffers[tx_idx], IPIP6_OVERHEAD_BYTES);

				decapsulate_ipip6_packet(local_port->m_config, buf);

				if (tunnelled_pkts == TX_BURST)
				{
					transmit_pkts = rte_eth_tx_burst(local_port->m_port_id, queue_num, tx_buffers, TX_BURST);

					if (transmit_pkts < TX_BURST)
					{
						for(int i = transmit_pkts; i < TX_BURST; i++)
						{
							rte_pktmbuf_free(tx_buffers[i]);
						}
					}
				}
				tx_idx++;
			}
		}

		tunnel_port->rx_queue_mutex.lock();
		if (tunnel_port->rx_queue_index.empty())
		{
			queue_num = tunnel_port->rx_queue_index.front();
			tunnel_port->rx_queue_index.pop();
		}
		tunnel_port->rx_queue_mutex.unlock();
	}
}

void DSLiteB4Router::set_lcore_allocation(uint64_t port0_mask, uint64_t port1_mask)
{

}
