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

#ifndef ROUTER_H
#define ROUTER_H

#include "port.h"

#define IPIP6_OVERHEAD_BYTES ((sizeof(struct ipv6_hdr) +sizeof(struct ip6_opt) +sizeof(struct ip6_opt_tunnel) +sizeof(struct ip6_opt_padn)))

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
}__attribute__((packed));

struct queue_stats
{
	uint64_t rx_frames;
	uint64_t tx_frames;
};

struct timestamp_pair
{
	uint64_t rx_tsc_value;
	uint64_t tx_tsc_value;
};

class Tester
{
public:
	vector<Port *> ports_vector;
	//TODO: Must support more lcores than 64
	uint64_t port0_lcore_mask = 0;
	uint64_t port1_lcore_mask = 0;
	uint64_t port0_mask = 0;
	uint64_t port1_mask = 0;
	struct queue_stats *port0_stats;
	struct queue_stats *port1_stats;
	uint64_t num_tsc_pairs_per_qp;
	// one index per RX queue
	uint64_t *port0_tsc_pairs_index;
	uint64_t *port1_tsc_pairs_index;
	struct timestamp_pair **port0_tsc_pairs_array;
	struct timestamp_pair **port1_tsc_pairs_array;
	bool timestamp_packets = false;
	bool timestamp_500_packets = false;
	uint64_t tester_duration;
	uint64_t m_num_queues;

	// When this counter is equal to the number of total rx queues
	// tx threads will start
	std::atomic<int> rx_ready_counter {0};
	bool tx_delay_flag = true;

	void encapsulate_ipip6_packet(PortConfig *config, char *buf, int buf_len);
	void decapsulate_ipip6_packet(PortConfig *config, char *buf);
	void construct_ip6_packet(PortConfig *config, char *buf, int buf_len, bool timestamp_packets, uint64_t pkt_id, uint64_t queue_id);
	void construct_ip_packet(PortConfig *config, char *buf, int buf_len, bool timestamp_packets, uint64_t pkt_id, uint64_t queue_id);
	void construct_ipip6_packet(PortConfig *config, char *buf, int buf_len, bool timestamp_packets, uint64_t pkt_id, uint64_t queue_id);
	virtual void set_lcore_allocation(uint64_t port0_mask, uint64_t port1_mask) { }
	virtual void set_ports_from_config(void) { }

	void add_port(Port * port);

	virtual ~Tester() = default;
};

#endif /* ROUTER_H */
