/*
 * Copyright (C) 2019 Alexandru Moise <00moses.alexander00@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public Licens
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-
 *
 */

#include "tester.h"

#define TX_BURST 4
#define RX_BURST 32

inline bool verify_ip_packet(char *buf)
{
	uint64_t *data = (uint64_t *)(buf+(sizeof(struct ether_hdr))+(sizeof(struct ipv4_hdr))+(sizeof(struct udp_hdr)));

	if (*data == 0x811136ee17e)
		return true;
	else
		return false;
}

inline bool verify_ipip6_packet(char *buf)
{

	uint64_t *data = (uint64_t *) (buf+
			(sizeof(struct ether_hdr))+
			(sizeof(struct ipv6_hdr))+
			(sizeof(struct ip6_opt))+
			(sizeof(struct ip6_opt_tunnel))+
			(sizeof(struct ip6_opt_padn))+
			(sizeof(struct ipv4_hdr))+
			(sizeof(struct udp_hdr)));

	if (*data == 0x811136ee17e)
		return true;
	else
		return false;
}

/*
 * This function implements a tester in accordance with the requirements of RFC8219.
 *
 * Test Mode: AFTR
 *                   +----------------------------------+
 *                   |tunnel_port             local_port|
 *      +----------->|IPv4inIPv6    Tester       IPv4   |<-------------+
 *      |            |                                  |              |
 *      |            +----------------------------------+              |
 *      |                                                              |
 *      |            +----------------------------------+              |
 *      |            |                                  |              |
 *      +----------->|IPv4inIPv6      DUT        IPv4   |<-------------+
 *                   |decapsulate <===NAT===>           |
 *                   +----------------------------------+
 *
 * Test Mode: B4
 *                   +----------------------------------+
 *                   |local_port             tunnel_port|
 *      +----------->|   IPv4       Tester   IPv4inIPv6 |<-------------+
 *      |            |                                  |              |
 *      |            +----------------------------------+              |
 *      |                                                              |
 *      |            +----------------------------------+              |
 *      |            |        <<<===decapsulate         |              |
 *      +----------->|IPv4            DUT     IPv4inIPv6|<-------------+
 *                   |        encapsulate===>>>         |
 *                   +----------------------------------+
 *
 * Test Mode: Selftest
 *
 *                   +----------------------------------+
 *                   |local_port             tunnel_port|
 *      +----------->|IPv4          Tester       IPv4   |<-------------+
 *      |            |                                  |              |
 *      |            +----------------------------------+              |
 *      |                                                              |
 *      +--------------------------------------------------------------+
 */
void DSLiteTester::runtest(uint64_t target_rate, uint64_t buf_len, dslite_test_mode_t test_mode)
{
	int lcore = (int)rte_lcore_id();
	int queue_num;
	uint64_t tsc_hz;
	uint64_t wait_ticks;
	uint64_t ticks_upper_bound;
	uint64_t amt_bits;


	/*
	 * The rate calculation:
	 * number of TSC ticks in 1 sec ...... target rate in bits
	 * number of ticks to wait ...... amount of bits we send in one burst
	 * number of ticks to wait = (TSC ticks in 1 sec * amount of bits per burst) / target rate in bits
	 */

	tsc_hz = rte_get_tsc_hz();

	amt_bits = TX_BURST * buf_len * 8;

	wait_ticks = tsc_hz * amt_bits / target_rate;

	/*
	 * We do a 1:1 mapping between queue and lcore, so in the tester case
	 * you should lcore_num = 2 * queue_num * ports_num
	 * There are 4 bitmasks that we'll use as sieves for the lcores
	 *  port0_lcore_rx_mask;
	 *  port0_lcore_tx_mask;
	 *  port1_lcore_rx_mask;
	 *  port1_lcore_tx_mask;
	 *
	 *  Therefore when operating in tester mode the lcores number should be
	 *  a multiple of 4, and split evenly between the CPU sockets.
	 */
	if (local_lcore_rx_mask & (0x1ULL << lcore))
	{
		struct rte_mbuf *rx_buffers[RX_BURST];
		int received_pkts = 0;
		int transmit_pkts = 0;
		int tunnelled_pkts = 0;
		int rx_idx = 0;
		int nb_rx = 0;
		char *buf;
		int queue_num;

		
		local_port->rx_queue_mutex.lock();
		if (!local_port->rx_queue_index.empty())
		{
			queue_num = local_port->rx_queue_index.front();
			local_port->rx_queue_index.pop();
		}
		local_port->rx_queue_mutex.unlock();
		
		while (traffic_running) {
			received_pkts = rte_eth_rx_burst(local_port->m_port_id, queue_num, rx_buffers, RX_BURST);

			nb_rx = received_pkts; 
			rx_idx = received_pkts - 1;

			while (received_pkts > 0)
			{
				buf = rte_pktmbuf_mtod(rx_buffers[rx_idx], char*);
				if (test_mode == AFTR || test_mode == B4 || test_mode == SELFTEST)
				{
					if (verify_ip_packet(buf))
					{
						local_port_stats[queue_num].rx_frames++;
					}
				}

				rx_idx--;
				received_pkts--;
			}

			for (int i = 0; i < nb_rx; i++)
			{
				rte_pktmbuf_free(rx_buffers[i]);
			}
		}

		// wait for all packets to be received
		usleep(2*1000*1000);

		local_port->rx_queue_mutex.lock();
		local_port->rx_queue_index.push(queue_num);
		local_port->rx_queue_mutex.unlock();
	}
	if (local_lcore_tx_mask & (0x1ULL << lcore))
	{
		char *buf;

		local_port->tx_queue_mutex.lock();
		if (!local_port->tx_queue_index.empty())
		{
			queue_num = local_port->tx_queue_index.front();
			local_port->tx_queue_index.pop();
		}
		local_port->tx_queue_mutex.unlock();

		while (traffic_running) {
			struct rte_mbuf *pktsbuf[TX_BURST];
			int buffer_idx = 0;
			int allocated_packets, remaining_packets;

			ticks_upper_bound = rte_get_tsc_cycles() + wait_ticks;

			for (allocated_packets = 0; allocated_packets < TX_BURST; allocated_packets++)
			{
				struct rte_mbuf *pktmbuf = rte_pktmbuf_alloc(mempools_vector[local_port->m_port_id]);
				if (!pktmbuf)
				{
					cout<<"Failed to allocate pktmbuf!"<<endl;
					exit(EXIT_FAILURE);
				}

				buf = rte_pktmbuf_append(pktmbuf, buf_len);

				if (test_mode == AFTR || test_mode == B4 || test_mode == SELFTEST)
				{
					construct_ip_packet(local_port->m_config, buf, buf_len);
				}

				pktsbuf[allocated_packets] = pktmbuf;
			}

			/* Send burst of TX packets, to second port of pair. */
			uint16_t nb_tx = rte_eth_tx_burst(local_port->m_port_id, queue_num, pktsbuf, TX_BURST);

			local_port_stats[queue_num].tx_frames += nb_tx;

			/* Free any unsent packets. */
			if (unlikely(nb_tx < TX_BURST)) {
				for (remaining_packets = nb_tx; remaining_packets < TX_BURST ; remaining_packets++)
				{
					rte_pktmbuf_free(pktsbuf[remaining_packets]);
				}
			}

			/* Busy loop to keep rate constant */
			while(rte_rdtsc() < ticks_upper_bound);
		}

		local_port->tx_queue_mutex.lock();
		local_port->tx_queue_index.push(queue_num);
		local_port->tx_queue_mutex.unlock();
	}
	if (tunnel_lcore_rx_mask & (0x1ULL << lcore))
	{
		struct rte_mbuf *rx_buffers[RX_BURST];
		int received_pkts = 0;
		int rx_idx = 0;
		int nb_rx = 0;
		char *buf;
		int queue_num;

		tunnel_port->rx_queue_mutex.lock();
		if (!tunnel_port->rx_queue_index.empty())
		{
			queue_num = tunnel_port->rx_queue_index.front();
			tunnel_port->rx_queue_index.pop();
		}
		tunnel_port->rx_queue_mutex.unlock();
		
		while (traffic_running) {
			received_pkts = rte_eth_rx_burst(tunnel_port->m_port_id, queue_num, rx_buffers, RX_BURST);

			nb_rx = received_pkts;

			rx_idx = received_pkts - 1;

			while (received_pkts > 0)
			{
				buf = rte_pktmbuf_mtod(rx_buffers[rx_idx], char*);
				if (test_mode == AFTR || test_mode == B4 || test_mode == SELFTEST)
				{
					if (test_mode == AFTR || test_mode == B4)
					{
						if (verify_ipip6_packet(buf))
						{
							tunnel_port_stats[queue_num].rx_frames++;
						}
					}
					else if (test_mode == SELFTEST)
					{
						if (verify_ip_packet(buf))
						{
							tunnel_port_stats[queue_num].rx_frames++;
						}
					}
				}

				rx_idx--;
				received_pkts--;
			}

			for (int i = 0; i < nb_rx; i++)
			{
				rte_pktmbuf_free(rx_buffers[i]);
			}
		}

		// wait for all packets to be received
		usleep(2*1000*1000);

		tunnel_port->rx_queue_mutex.lock();
		tunnel_port->rx_queue_index.push(queue_num);
		tunnel_port->rx_queue_mutex.unlock();

	}
	if (tunnel_lcore_tx_mask & (0x1ULL << lcore))
	{
		char *buf;

		tunnel_port->tx_queue_mutex.lock();
		if (!tunnel_port->tx_queue_index.empty())
		{
			queue_num = tunnel_port->tx_queue_index.front();
			tunnel_port->tx_queue_index.pop();
		}
		tunnel_port->tx_queue_mutex.unlock();

		while (traffic_running) {
			struct rte_mbuf *pktsbuf[TX_BURST];
			int buffer_idx = 0;
			int allocated_packets, remaining_packets;

			ticks_upper_bound = rte_get_tsc_cycles() + wait_ticks;

			for (allocated_packets = 0; allocated_packets < TX_BURST; allocated_packets++)
			{
				struct rte_mbuf *pktmbuf = rte_pktmbuf_alloc(mempools_vector[tunnel_port->m_port_id]);
				if (!pktmbuf)
				{
					cout<<"Failed to allocate pktmbuf!"<<endl;
					exit(EXIT_FAILURE);
				}

				buf = rte_pktmbuf_append(pktmbuf, buf_len);

				if (test_mode == AFTR || test_mode == B4)
				{
					construct_ipip6_packet(tunnel_port->m_config, buf, buf_len);
				}
				else if (test_mode == SELFTEST)
				{
					construct_ip_packet(tunnel_port->m_config, buf, buf_len);
				}

				pktsbuf[allocated_packets] = pktmbuf;
			}

			/* Send burst of TX packets, to second port of pair. */
			uint16_t nb_tx = rte_eth_tx_burst(tunnel_port->m_port_id, queue_num, pktsbuf, TX_BURST);

			tunnel_port_stats[queue_num].tx_frames += nb_tx;

			/* Free any unsent packets. */
			if (unlikely(nb_tx < TX_BURST)) {
				for (remaining_packets = nb_tx; remaining_packets < TX_BURST ; remaining_packets++)
				{
					rte_pktmbuf_free(pktsbuf[remaining_packets]);
				}
			}

			/* Busy loop to keep rate constant */
			while(rte_rdtsc() < ticks_upper_bound);
		}

		tunnel_port->tx_queue_mutex.lock();
		tunnel_port->tx_queue_index.push(queue_num);
		tunnel_port->tx_queue_mutex.unlock();

	}

}

void DSLiteTester::set_ports_from_config(void)
{
	if (ports_vector[0]->m_config->is_ipip6_tun_intf) {
		tunnel_port = ports_vector[0];
		local_port = ports_vector[1];
		tunnel_port_mask = port0_lcore_mask;
		local_port_mask = port1_lcore_mask;
	} else if (ports_vector[1]->m_config->is_ipip6_tun_intf) {
		tunnel_port = ports_vector[1];
		local_port = ports_vector[0];
		tunnel_port_mask = port1_lcore_mask;
		local_port_mask = port0_lcore_mask;
	}

	set_lcore_allocation(local_port_mask, tunnel_port_mask);

	local_port_stats  = new queue_stats[local_port->rx_queues > local_port->tx_queues ?
					    local_port->rx_queues : local_port->tx_queues]();
	tunnel_port_stats = new queue_stats[tunnel_port->rx_queues > tunnel_port->tx_queues ?
					    tunnel_port->rx_queues : tunnel_port->tx_queues]();
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

void DSLiteTester::set_lcore_allocation(uint64_t local_mask, uint64_t tunnel_mask)
{
		uint64_t split_mask0 = 0xffffffffffffffffULL;
		uint64_t split_mask1 = 0xffffffffffffffffULL;
		int cut1, cut2;
		uint64_t split_lcore_port0[2];
		uint64_t split_lcore_port1[2];

		cut1 = split_mask_in_two(local_mask);
		cut2 = split_mask_in_two(tunnel_mask);

		local_lcore_rx_mask = (local_mask &  (split_mask0 << cut1));
		local_lcore_tx_mask = (local_mask & ~(split_mask0 << cut1));

		tunnel_lcore_rx_mask = (tunnel_mask &  (split_mask1 << cut2));
		tunnel_lcore_tx_mask = (tunnel_mask & ~(split_mask1 << cut2));
}

