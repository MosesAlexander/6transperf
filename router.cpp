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

#include "router.h"

static uint16_t id_gen = 0;

void Router::encapsulate_ipip6_packet(PortConfig *config, char *buf, int buf_len)
{
	struct ether_hdr *ethhdr;
	struct ipv6_hdr *ip6_hdr;
	struct ip6_opt *ip6_opt;
	struct ip6_opt_tunnel *ip6_opt_tun;
	struct ip6_opt_padn *padn;
	uint16_t outer_payload_len = buf_len
			-sizeof(struct ether_hdr)
			-sizeof(struct ipv6_hdr);

	ethhdr = (struct ether_hdr *)buf;
	ip6_hdr = (struct ipv6_hdr *)(buf+(sizeof(struct ether_hdr)));
	ip6_opt = (struct ip6_opt *)(buf+(sizeof(struct ether_hdr))+(sizeof(struct ipv6_hdr)));
	ip6_opt_tun = (struct ip6_opt_tunnel *)(buf+
			(sizeof(struct ether_hdr))+
			(sizeof(struct ipv6_hdr))+
			(sizeof(struct ip6_opt)));
	padn = (struct ip6_opt_padn *)(buf+
			(sizeof(struct ether_hdr))+
			(sizeof(struct ipv6_hdr))+
			(sizeof(struct ip6_opt))+
			(sizeof(struct ip6_opt_tunnel)));

	memcpy(ethhdr->d_addr.addr_bytes, config->peer_mac, 6);
	memcpy(ethhdr->s_addr.addr_bytes, config->self_mac, 6);

	/* IPv6 */
	ethhdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv6);

	/* IPv6 header */
	ip6_hdr->vtc_flow = rte_cpu_to_be_32(0x600e4a1e); // Set IPv6 version number
	ip6_hdr->payload_len = rte_cpu_to_be_16(outer_payload_len);
	ip6_hdr->proto = IPPROTO_DSTOPTS;
	ip6_hdr->hop_limits = IP_DEFTTL;
	rte_memcpy(ip6_hdr->src_addr, config->self_ip6, sizeof(ip6_hdr->src_addr));
	rte_memcpy(ip6_hdr->dst_addr, config->dest_ip6, sizeof(ip6_hdr->dst_addr));

	ip6_opt->ip6o_type = 0x4;
	ip6_opt->ip6o_len = 0;

	ip6_opt_tun->ip6ot_type = 0x4;
	ip6_opt_tun->ip6ot_len = 0x1;
	ip6_opt_tun->ip6ot_encap_limit = 0x4;

	padn->pad_type = 0x1;
	padn->pad_len = 0x1;
	padn->padn[0] = 0x0;
}

void Router::decapsulate_ipip6_packet(PortConfig *config, char *buf)
{
	struct ether_hdr *ethhdr;

	ethhdr = (struct ether_hdr *)buf;

	memcpy(ethhdr->d_addr.addr_bytes, config->peer_mac, 6);
	memcpy(ethhdr->s_addr.addr_bytes, config->self_mac, 6);

	/* IPv4 */
	ethhdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
}

void Router::construct_ipip6_packet(PortConfig *config, char *buf, int buf_len, bool timestamp_all_packets)
{
	uint16_t *ptr16;
	uint32_t ip_cksum;
	struct ether_hdr *ethhdr;
	struct ipv6_hdr *ip6_hdr;
	struct ipv4_hdr *ip4_hdr;
	struct ip6_opt *ip6_opt;
	struct ip6_opt_tunnel *ip6_opt_tun;
	struct ip6_opt_padn *padn;
	struct udp_hdr *udp_hdr;
	uint64_t *data;
	uint64_t start_tsc;
	uint16_t outer_payload_len = buf_len
			-sizeof(struct ether_hdr)
			-sizeof(struct ipv6_hdr);
	uint16_t inner_payload_len = buf_len
			-sizeof(struct ether_hdr)
			-sizeof(struct ipv6_hdr)
			-sizeof(struct ip6_opt)
			-sizeof(struct ip6_opt_tunnel)
			-sizeof(struct ip6_opt_padn);
	uint16_t data_len = buf_len
			-sizeof(struct ether_hdr)
			-sizeof(struct ipv6_hdr)
			-sizeof(struct ip6_opt)
			-sizeof(struct ip6_opt_tunnel)
			-sizeof(struct ip6_opt_padn)
			-sizeof(struct ipv4_hdr);

	ethhdr = (struct ether_hdr *)buf;
	ip6_hdr = (struct ipv6_hdr *)(buf+(sizeof(struct ether_hdr)));
	ip6_opt = (struct ip6_opt *)(buf+(sizeof(struct ether_hdr))+(sizeof(struct ipv6_hdr)));
	ip6_opt_tun = (struct ip6_opt_tunnel *)(buf+
			(sizeof(struct ether_hdr))+
			(sizeof(struct ipv6_hdr))+
			(sizeof(struct ip6_opt)));
	padn = (struct ip6_opt_padn *)(buf+
			(sizeof(struct ether_hdr))+
			(sizeof(struct ipv6_hdr))+
			(sizeof(struct ip6_opt))+
			(sizeof(struct ip6_opt_tunnel)));
	ip4_hdr = (struct ipv4_hdr *)(buf+
			(sizeof(struct ether_hdr))+
			(sizeof(struct ipv6_hdr))+
			(sizeof(struct ip6_opt))+
			(sizeof(struct ip6_opt_tunnel))+
			(sizeof(struct ip6_opt_padn)));
	udp_hdr = (struct udp_hdr *)(buf+
			(sizeof(struct ether_hdr))+
			(sizeof(struct ipv6_hdr))+
			(sizeof(struct ip6_opt))+
			(sizeof(struct ip6_opt_tunnel))+
			(sizeof(struct ip6_opt_padn))+
			(sizeof(struct ipv4_hdr)));
	data = (uint64_t *) (buf+
			(sizeof(struct ether_hdr))+
			(sizeof(struct ipv6_hdr))+
			(sizeof(struct ip6_opt))+
			(sizeof(struct ip6_opt_tunnel))+
			(sizeof(struct ip6_opt_padn))+
			(sizeof(struct ipv4_hdr))+
			(sizeof(struct udp_hdr)));


	memcpy(ethhdr->d_addr.addr_bytes, config->peer_mac, 6);
	memcpy(ethhdr->s_addr.addr_bytes, config->self_mac, 6);

	/* IPv6 */
	ethhdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv6);

	/* IPv6 header */
	ip6_hdr->vtc_flow = rte_cpu_to_be_32(0x600e4a1e); // Set IPv6 version number
	ip6_hdr->payload_len = rte_cpu_to_be_16(outer_payload_len);
	ip6_hdr->proto = IPPROTO_DSTOPTS;
	ip6_hdr->hop_limits = IP_DEFTTL;
	rte_memcpy(ip6_hdr->src_addr, config->self_ip6, sizeof(ip6_hdr->src_addr));
	rte_memcpy(ip6_hdr->dst_addr, config->dest_ip6, sizeof(ip6_hdr->dst_addr));

	ip6_opt->ip6o_type = 0x4;
	ip6_opt->ip6o_len = 0;

	ip6_opt_tun->ip6ot_type = 0x4;
	ip6_opt_tun->ip6ot_len = 0x1;
	ip6_opt_tun->ip6ot_encap_limit = 0x4;

	padn->pad_type = 0x1;
	padn->pad_len = 0x1;
	padn->padn[0] = 0x0;

	/* IPv4 header */
	ip4_hdr->version_ihl   = IP_VHL_DEF;
	ip4_hdr->type_of_service   = 0;
	ip4_hdr->fragment_offset = rte_cpu_to_be_16(0x4000); // don't fragment
	ip4_hdr->time_to_live   = IP_DEFTTL;
	ip4_hdr->next_proto_id = IPPROTO_UDP;
	ip4_hdr->packet_id = 0; //rte_cpu_to_be_16(0x0da8); // random string
	ip4_hdr->total_length   = rte_cpu_to_be_16(inner_payload_len);
	ip4_hdr->src_addr = rte_cpu_to_be_32(config->self_ip4);
	ip4_hdr->dst_addr = rte_cpu_to_be_32(config->dest_ip4);

	/*
	 * Compute IP header checksum.
	 */
	ptr16 = (unaligned_uint16_t*) ip4_hdr;
	ip_cksum = 0;
	ip_cksum += ptr16[0]; ip_cksum += ptr16[1];
	ip_cksum += ptr16[2]; ip_cksum += ptr16[3];
	ip_cksum += ptr16[4];
	ip_cksum += ptr16[6]; ip_cksum += ptr16[7];
	ip_cksum += ptr16[8]; ip_cksum += ptr16[9];

	/*
	 * Reduce 32 bit checksum to 16 bits and complement it.
	 */
	ip_cksum = ((ip_cksum & 0xFFFF0000) >> 16) +
		(ip_cksum & 0x0000FFFF);
	if (ip_cksum > 65535)
		ip_cksum -= 65535;
	ip_cksum = (~ip_cksum) & 0x0000FFFF;
	if (ip_cksum == 0)
		ip_cksum = 0xFFFF;
	ip4_hdr->hdr_checksum = (uint16_t) ip_cksum;

	// Add some udp payload, we won't really care about this, unless
	// we want to add some random data here and checksum it to test
	// for data integrity between the NICs
	udp_hdr->src_port = rte_cpu_to_be_16(1024);
	udp_hdr->dst_port = rte_cpu_to_be_16(id_gen++);
	udp_hdr->dgram_len      = rte_cpu_to_be_16(data_len);
	udp_hdr->dgram_cksum    = 0x0; /* No UDP checksum. */

	data[0] = 0x811136ee17e;
	if (timestamp_all_packets) {
		start_tsc = rte_rdtsc();
		data[1] = start_tsc;
	}
	//udp_hdr->dgram_cksum    = rte_ipv6_udptcp_cksum(ip_hdr, (void*)udp_hdr); /* No UDP checksum. */
}

void Router::construct_ip6_packet(PortConfig *config, char *buf, int buf_len, bool timestamp_all_packets)
{
	struct ether_hdr *hdr;
	uint16_t *ptr16;
	uint32_t ip_cksum;
	uint16_t data_len = buf_len - sizeof(struct ether_hdr) - sizeof(struct ipv6_hdr);
	uint16_t pkt_len;
	struct ipv6_hdr *ip_hdr;
	struct udp_hdr *udp_hdr;
	uint64_t *data;
	uint64_t start_tsc;

	hdr = (struct ether_hdr *)buf;
	ip_hdr = (struct ipv6_hdr *)(buf+(sizeof(struct ether_hdr)));
	udp_hdr = (struct udp_hdr *)(buf+(sizeof(struct ether_hdr))+(sizeof(struct ipv6_hdr)));
	data = (uint64_t *) (buf+(sizeof(struct ether_hdr))+(sizeof(struct ipv6_hdr)));

	memcpy(hdr->d_addr.addr_bytes, config->peer_mac, 6);
	memcpy(hdr->s_addr.addr_bytes, config->self_mac, 6);

	/* IPv6 */
	hdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv6);

	/* IPv6 header */
	pkt_len = (uint64_t) (data_len +sizeof(struct ipv6_hdr)); // data + ip header
	ip_hdr->vtc_flow = rte_cpu_to_be_32(0x60000000); // Set IPv6 version number
	ip_hdr->payload_len = rte_cpu_to_be_16(data_len);
	ip_hdr->proto = IPPROTO_UDP;
	ip_hdr->hop_limits = IP_DEFTTL;
	rte_memcpy(ip_hdr->src_addr, config->self_ip6, sizeof(ip_hdr->src_addr));
	rte_memcpy(ip_hdr->dst_addr, config->dest_ip6, sizeof(ip_hdr->dst_addr));

	// Add some udp payload, we won't really care about this, unless
	// we want to add some random data here and checksum it to test
	// for data integrity between the NICs
	udp_hdr->src_port = rte_cpu_to_be_16(1024);
	udp_hdr->dst_port = rte_cpu_to_be_16(id_gen++);
	udp_hdr->dgram_len      = rte_cpu_to_be_16(data_len);
	udp_hdr->dgram_cksum    = 0; /* No UDP checksum. */

	data[0] = 0x811136ee17e;
	if (timestamp_all_packets) {
		start_tsc = rte_rdtsc();
		data[1] = start_tsc;
	}
	//udp_hdr->dgram_cksum    = rte_ipv6_udptcp_cksum(ip_hdr, (void*)udp_hdr); /* No UDP checksum. */

}

void Router::construct_ip_packet(PortConfig *config, char *buf, int buf_len, bool timestamp_all_packets)
{
	struct ether_hdr *hdr;
	uint16_t *ptr16;
	uint32_t ip_cksum;
	uint16_t data_len = buf_len - sizeof(struct ether_hdr) - sizeof(struct ipv4_hdr);
	uint16_t pkt_len;
	struct ipv4_hdr *ip_hdr;
	struct udp_hdr *udp_hdr;
	uint64_t *data;
	uint64_t start_tsc;

	hdr = (struct ether_hdr *)buf;
	ip_hdr = (struct ipv4_hdr *)(buf+(sizeof(struct ether_hdr)));
	udp_hdr = (struct udp_hdr *)(buf+(sizeof(struct ether_hdr))+(sizeof(struct ipv4_hdr)));
	data = (uint64_t *)(buf+(sizeof(struct ether_hdr))+(sizeof(struct ipv4_hdr))+(sizeof(struct udp_hdr)));

	memcpy(hdr->d_addr.addr_bytes, config->peer_mac, 6);
	memcpy(hdr->s_addr.addr_bytes, config->self_mac, 6);

	/* IPv4 */
	hdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);

	/* IPv4 header */
	pkt_len = (uint16_t) (data_len + sizeof(struct ipv4_hdr)); // data + ip header
	ip_hdr->version_ihl   = IP_VHL_DEF;
	ip_hdr->type_of_service   = 0;
	ip_hdr->fragment_offset = 0;
	ip_hdr->time_to_live   = IP_DEFTTL;
	ip_hdr->next_proto_id = IPPROTO_UDP;
	ip_hdr->packet_id = 0;
	ip_hdr->total_length   = rte_cpu_to_be_16(pkt_len);
	ip_hdr->src_addr = rte_cpu_to_be_32(config->self_ip4);
	ip_hdr->dst_addr = rte_cpu_to_be_32(config->dest_ip4);

	/*
	 * Compute IP header checksum.
	 */
	ptr16 = (unaligned_uint16_t*) ip_hdr;
	ip_cksum = 0;
	ip_cksum += ptr16[0]; ip_cksum += ptr16[1];
	ip_cksum += ptr16[2]; ip_cksum += ptr16[3];
	ip_cksum += ptr16[4];
	ip_cksum += ptr16[6]; ip_cksum += ptr16[7];
	ip_cksum += ptr16[8]; ip_cksum += ptr16[9];

	/*
	 * Reduce 32 bit checksum to 16 bits and complement it.
	 */
	ip_cksum = ((ip_cksum & 0xFFFF0000) >> 16) +
		(ip_cksum & 0x0000FFFF);
	if (ip_cksum > 65535)
		ip_cksum -= 65535;
	ip_cksum = (~ip_cksum) & 0x0000FFFF;
	if (ip_cksum == 0)
		ip_cksum = 0xFFFF;
	ip_hdr->hdr_checksum = (uint16_t) ip_cksum;

	// Add some udp payload, we won't really care about this, unless
	// we want to add some random data here and checksum it to test
	// for data integrity between the NICs
	udp_hdr->src_port = rte_cpu_to_be_16(1024);
	udp_hdr->dst_port = rte_cpu_to_be_16(id_gen++);
	udp_hdr->dgram_len      = rte_cpu_to_be_16(data_len);
	udp_hdr->dgram_cksum    = 0; /* No UDP checksum. */
	
	data[0] = 0x811136ee17e;
	if (timestamp_all_packets) {
		start_tsc = rte_rdtsc();
		data[1] = start_tsc;
	}
}

void Router::add_port(Port *port)
{
	ports_vector.push_back(port);
}






