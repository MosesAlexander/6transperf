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

#define TX_BURST 1 /* Set to 1 to avoid back-to-back frames, can be modified freely, just make sure the RX side can handle the load */
#define RX_BURST 16

inline bool verify_ip_packet(char *buf)
{
	uint64_t *data = (uint64_t *)(buf+(sizeof(struct ether_hdr))+(sizeof(struct ipv4_hdr))+(sizeof(struct udp_hdr)));

	if (*data == 0x811136ee17e)
		return true;
	else
		return false;
}

inline uint64_t extract_ip_pkt_id(char *buf)
{
	uint64_t *data = (uint64_t *)(buf+(sizeof(struct ether_hdr))+(sizeof(struct ipv4_hdr))+(sizeof(struct udp_hdr)) + 8);

	return *data;
}

inline uint64_t extract_ip_queue_id(char *buf)
{
	uint64_t *data = (uint64_t *)(buf+(sizeof(struct ether_hdr))+(sizeof(struct ipv4_hdr))+(sizeof(struct udp_hdr)) + 16);

	return *data;
}

inline uint64_t extract_ipip6_pkt_id(char *buf)
{
	uint64_t *data = (uint64_t *) (buf+
			(sizeof(struct ether_hdr))+
			(sizeof(struct ipv6_hdr))+
			(sizeof(struct ip6_opt))+
			(sizeof(struct ip6_opt_tunnel))+
			(sizeof(struct ip6_opt_padn))+
			(sizeof(struct ipv4_hdr))+
			//(sizeof(struct udp_hdr)) + 8); //TODO: find out why the received packets are 8 bytes minus from expected
			(sizeof(struct udp_hdr)));

	return *data;
}

inline uint64_t extract_ipip6_queue_id(char *buf)
{
	uint64_t *data = (uint64_t *) (buf+
			(sizeof(struct ether_hdr))+
			(sizeof(struct ipv6_hdr))+
			(sizeof(struct ip6_opt))+
			(sizeof(struct ip6_opt_tunnel))+
			(sizeof(struct ip6_opt_padn))+
			(sizeof(struct ipv4_hdr))+
			// (sizeof(struct udp_hdr)) + 16); //TODO: find out why the received packets are 8 bytes minus from expected
			(sizeof(struct udp_hdr)) + 8);

	return *data;
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
			// (sizeof(struct udp_hdr))); //TODO: find out why the received packets are 8 bytes minus from expected
			(sizeof(struct udp_hdr)) - 8);

	if (*data == 0x811136ee17e || *(data+1) == 0x811136ee17e)
		return true;
	else
		return false;
}

/*
 * This function implements a tester in accordance with the requirements of RFC8219.
 *
 * Test Mode: AFTR
 *                   +----------------------------------+
 *                   |port1                        port0|
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
 *                   |port0                        port1|
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
 *                   |port0                        port1|
 *      +----------->|IPv4          Tester       IPv4   |<-------------+
 *      |            |                                  |              |
 *      |            +----------------------------------+              |
 *      |                                                              |
 *      +--------------------------------------------------------------+
 */
void DSLiteTester::runtest(uint64_t target_rate_fps, uint64_t buf_len, dslite_test_mode_t test_mode)
{
	int lcore = (int)rte_lcore_id();
	uint64_t tsc_hz;
	uint64_t wait_ticks;
	uint64_t frames_per_burst;
	int total_rx_queues;
	uint64_t frames_to_send = (tester_duration * target_rate_fps) / m_num_queues;
	uint64_t *port0_sent_frames = (uint64_t*) malloc(sizeof(uint64_t) * m_num_queues);
	uint64_t *port1_sent_frames = (uint64_t*) malloc(sizeof(uint64_t) * m_num_queues);
	uint64_t *port0_start_tsc = (uint64_t*)malloc(sizeof(uint64_t) * m_num_queues); // one per queue pair
	uint64_t *port1_start_tsc = (uint64_t*)malloc(sizeof(uint64_t) * m_num_queues);

	total_rx_queues = port0->rx_queues + port1->rx_queues;

	/*
	 * The rate calculation:
	 * number of TSC ticks in 1 sec ...... target rate in frames
	 * number of ticks to wait ........... amount of frames we send in one burst
	 * number of ticks to wait = (TSC ticks in 1 sec * amount of bits per burst) / target rate in bits
	 */

	tsc_hz = rte_get_tsc_hz();

	frames_per_burst = TX_BURST;

	wait_ticks = frames_per_burst * tsc_hz / target_rate_fps;

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
	if (port0_lcore_rx_mask & (0x1ULL << lcore))
	{
		struct rte_mbuf *rx_buffers[RX_BURST];
		int received_pkts = 0;
		int transmit_pkts = 0;
		int tunnelled_pkts = 0;
		int rx_idx = 0;
		int nb_rx = 0;
		char *buf;
		int queue_num;
		uint64_t received_tsc;

		
		port0->rx_queue_mutex.lock();
		if (!port0->rx_queue_index.empty())
		{
			queue_num = port0->rx_queue_index.front();
			//std::cout<< "Into the port0 RX thread, got queue "<<queue_num<<std::endl;
			port0->rx_queue_index.pop();
		}
		port0->rx_queue_mutex.unlock();

		rx_ready_counter++;
		
		while (rx_running) {
			received_pkts = rte_eth_rx_burst(port0->m_port_id, queue_num, rx_buffers, RX_BURST);

			received_tsc = rte_rdtsc();

			nb_rx = received_pkts; 
			rx_idx = received_pkts - 1;

			while (received_pkts > 0)
			{
				buf = rte_pktmbuf_mtod(rx_buffers[rx_idx], char*);
				if (test_mode == AFTR || test_mode == B4 || test_mode == SELFTEST)
				{
					// if the OTHER port is tunneling, check if we are receiving encapsulated packets
					if (port0->m_config->is_ipip6_tun_intf? verify_ipip6_packet(buf) : verify_ip_packet(buf))
					{
						port0_stats[queue_num].rx_frames++;
						if (timestamp_packets) {
							uint64_t qnum, pktid;
							if (port0->m_config->is_ipip6_tun_intf) {
								qnum = extract_ipip6_queue_id(buf);
								pktid = extract_ipip6_pkt_id(buf);
							} else {
								qnum  = extract_ip_queue_id(buf);
								pktid = extract_ip_pkt_id(buf);
							}
							port1_tsc_pairs_array[qnum][pktid].rx_tsc_value = received_tsc;
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

		port0->rx_queue_mutex.lock();
		port0->rx_queue_index.push(queue_num);
		port0->rx_queue_mutex.unlock();
	}
	if (port0_lcore_tx_mask & (0x1ULL << lcore))
	{
		char *buf;
		int queue_num;

		port0->tx_queue_mutex.lock();
		if (!port0->tx_queue_index.empty())
		{
			queue_num = port0->tx_queue_index.front();
			port0->tx_queue_index.pop();
		}
		port0->tx_queue_mutex.unlock();

		while(rx_ready_counter.load() < total_rx_queues);
		while(tx_delay_flag);

		port0_start_tsc[queue_num] = rte_rdtsc();

		for(port0_sent_frames[queue_num] = 0; port0_sent_frames[queue_num] < frames_to_send; port0_sent_frames[queue_num]++) {
			struct rte_mbuf *pktsbuf[TX_BURST];
			int buffer_idx = 0;
			int allocated_packets, remaining_packets;


			for (allocated_packets = 0; allocated_packets < TX_BURST; allocated_packets++)
			{
				struct rte_mbuf *pktmbuf = rte_pktmbuf_alloc(port0->m_pool);
				if (!pktmbuf)
				{
					cout<<"Failed to allocate pktmbuf!"<<endl;
					exit(EXIT_FAILURE);
				}

				buf = rte_pktmbuf_append(pktmbuf, buf_len);

				if (test_mode == AFTR || test_mode == B4 || test_mode == SELFTEST)
				{
					if (port0->m_config->is_ipip6_tun_intf)
					{
						construct_ipip6_packet(port0->m_config, buf, buf_len, timestamp_packets, port0->port_pkt_identifier[queue_num], queue_num);
					}
					else 
					{
						construct_ip_packet(port0->m_config, buf, buf_len, timestamp_packets, port0->port_pkt_identifier[queue_num], queue_num);
					}

				}

				pktsbuf[allocated_packets] = pktmbuf;
			}

			/* Send burst of TX packets, to second port of pair. */
			uint16_t nb_tx = rte_eth_tx_burst(port0->m_port_id, queue_num, pktsbuf, TX_BURST);

			if (timestamp_packets) {
				port0_tsc_pairs_array[queue_num][port0->port_pkt_identifier[queue_num]++].tx_tsc_value = rte_rdtsc();
			}

			port0_stats[queue_num].tx_frames += nb_tx;

			/* Free any unsent packets. */
			if (unlikely(nb_tx < TX_BURST)) {
				for (remaining_packets = nb_tx; remaining_packets < TX_BURST ; remaining_packets++)
				{
					rte_pktmbuf_free(pktsbuf[remaining_packets]);
				}
			}

			/* Busy loop to keep rate constant */
			while(rte_rdtsc() < port0_start_tsc[queue_num] + port0_sent_frames[queue_num] * tsc_hz / target_rate_fps );
		}

		port0->tx_queue_mutex.lock();
		port0->tx_queue_index.push(queue_num);
		port0->tx_queue_mutex.unlock();

		port0_elapsed_seconds = (double)(rte_rdtsc() - port0_start_tsc[queue_num])/tsc_hz;
	}
	if (port1_lcore_rx_mask & (0x1ULL << lcore))
	{
		struct rte_mbuf *rx_buffers[RX_BURST];
		int received_pkts = 0;
		int rx_idx = 0;
		int nb_rx = 0;
		char *buf;
		int queue_num;
		uint64_t received_tsc;

		port1->rx_queue_mutex.lock();
		if (!port1->rx_queue_index.empty())
		{
			queue_num = port1->rx_queue_index.front();
			//std::cout<< "Into the port1 RX thread, got queue "<<queue_num<<std::endl;
			port1->rx_queue_index.pop();
		}
		port1->rx_queue_mutex.unlock();

		rx_ready_counter++;
		
		while (rx_running) {
			received_pkts = rte_eth_rx_burst(port1->m_port_id, queue_num, rx_buffers, RX_BURST);

			received_tsc = rte_rdtsc();

			nb_rx = received_pkts;
			rx_idx = received_pkts - 1;

			while (received_pkts > 0)
			{
				buf = rte_pktmbuf_mtod(rx_buffers[rx_idx], char*);
				if (test_mode == AFTR || test_mode == B4 || test_mode == SELFTEST)
				{
					// if the OTHER port is tunneling, check if you're receiving encapsulated packets
					if (port1->m_config->is_ipip6_tun_intf? verify_ipip6_packet(buf) : verify_ip_packet(buf))
					{
						port1_stats[queue_num].rx_frames++;
						if (timestamp_packets) {
							uint64_t qnum, pktid;
							if (port1->m_config->is_ipip6_tun_intf) {
								qnum = extract_ipip6_queue_id(buf);
								pktid = extract_ipip6_pkt_id(buf);
							} else {
								qnum  = extract_ip_queue_id(buf);
								pktid = extract_ip_pkt_id(buf);
							}
							port0_tsc_pairs_array[qnum][pktid].rx_tsc_value = received_tsc;
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

		port1->rx_queue_mutex.lock();
		port1->rx_queue_index.push(queue_num);
		port1->rx_queue_mutex.unlock();

	}
	if (port1_lcore_tx_mask & (0x1ULL << lcore))
	{
		char *buf;
		int queue_num;

		port1->tx_queue_mutex.lock();
		if (!port1->tx_queue_index.empty())
		{
			queue_num = port1->tx_queue_index.front();
			port1->tx_queue_index.pop();
		}
		port1->tx_queue_mutex.unlock();

		while(rx_ready_counter.load() < total_rx_queues);
		while(tx_delay_flag);

		port1_start_tsc[queue_num] = rte_rdtsc();

		for(port1_sent_frames[queue_num] = 0; port1_sent_frames[queue_num] < frames_to_send; port1_sent_frames[queue_num]++) {
			struct rte_mbuf *pktsbuf[TX_BURST];
			int buffer_idx = 0;
			int allocated_packets, remaining_packets;

			for (allocated_packets = 0; allocated_packets < TX_BURST; allocated_packets++)
			{
				struct rte_mbuf *pktmbuf = rte_pktmbuf_alloc(port1->m_pool);
				if (!pktmbuf)
				{
					cout<<"Failed to allocate pktmbuf!"<<endl;
					exit(EXIT_FAILURE);
				}

				buf = rte_pktmbuf_append(pktmbuf, buf_len);

				if (test_mode == AFTR || test_mode == B4 || test_mode == SELFTEST)
				{
					if (port1->m_config->is_ipip6_tun_intf)
					{
						construct_ipip6_packet(port1->m_config, buf, buf_len, timestamp_packets, port1->port_pkt_identifier[queue_num], queue_num);
					}
					else
					{
						construct_ip_packet(port1->m_config, buf, buf_len, timestamp_packets, port1->port_pkt_identifier[queue_num], queue_num);
					}

				}

				pktsbuf[allocated_packets] = pktmbuf;
			}

			/* Send burst of TX packets, to second port of pair. */
			uint16_t nb_tx = rte_eth_tx_burst(port1->m_port_id, queue_num, pktsbuf, TX_BURST);

			if (timestamp_packets) {
				port1_tsc_pairs_array[queue_num][port1->port_pkt_identifier[queue_num]++].tx_tsc_value = rte_rdtsc();
			}

			port1_stats[queue_num].tx_frames += nb_tx;

			/* Free any unsent packets. */
			if (unlikely(nb_tx < TX_BURST)) {
				for (remaining_packets = nb_tx; remaining_packets < TX_BURST ; remaining_packets++)
				{
					rte_pktmbuf_free(pktsbuf[remaining_packets]);
				}
			}

			/* Busy loop to keep rate constant */
			while(rte_rdtsc() < port1_start_tsc[queue_num] + port1_sent_frames[queue_num] * tsc_hz / target_rate_fps );
		}

		port1->tx_queue_mutex.lock();
		port1->tx_queue_index.push(queue_num);
		port1->tx_queue_mutex.unlock();

		port1_elapsed_seconds = (double)(rte_rdtsc() - port1_start_tsc[queue_num])/tsc_hz;

	}
}

void DSLiteTester::set_ports_from_config(void)
{
	port0 = ports_vector[0];
	port1 = ports_vector[1];
	port0_mask = port0_lcore_mask;
	port1_mask = port1_lcore_mask;


	set_lcore_allocation(port0_mask, port1_mask);

	port0_stats  = new queue_stats[port0->rx_queues > port0->tx_queues ?
					    port0->rx_queues : port0->tx_queues]();
	port1_stats = new queue_stats[port1->rx_queues > port1->tx_queues ?
					    port1->rx_queues : port1->tx_queues]();
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

		port0_lcore_rx_mask = (local_mask &  (split_mask0 << cut1));
		port0_lcore_tx_mask = (local_mask & ~(split_mask0 << cut1));

		port1_lcore_rx_mask = (tunnel_mask &  (split_mask1 << cut2));
		port1_lcore_tx_mask = (tunnel_mask & ~(split_mask1 << cut2));
}

