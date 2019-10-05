#include "router.h"

void Router::encapsulate_ipip6_packet(PortConfig *config, char *buf, int buf_len)
{
}

void Router::decapsulate_ipip6_packet(PortConfig *config, char *buf, int buf_len)
{
}

void Router::construct_ip6_packet(PortConfig *config, char *buf, int buf_len)
{
	struct ether_hdr *hdr;
	uint16_t *ptr16;
	uint32_t ip_cksum;
	uint16_t data_len = buf_len - sizeof(struct ether_hdr) - sizeof(struct ipv6_hdr);
	uint16_t pkt_len;
	struct ipv6_hdr *ip_hdr;
	struct udp_hdr *udp_hdr;

	hdr = (struct ether_hdr *)buf;
	ip_hdr = (struct ipv6_hdr *)(buf+(sizeof(struct ether_hdr)));
	udp_hdr = (struct udp_hdr *)(buf+(sizeof(struct ether_hdr))+(sizeof(struct ipv6_hdr)));

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
	udp_hdr->dst_port = rte_cpu_to_be_16(1024);
	udp_hdr->dgram_len      = rte_cpu_to_be_16(data_len);
	udp_hdr->dgram_cksum    = 0xffff; /* No UDP checksum. */
	//udp_hdr->dgram_cksum    = rte_ipv6_udptcp_cksum(ip_hdr, (void*)udp_hdr); /* No UDP checksum. */

}

void Router::construct_ip_packet(PortConfig *config, char *buf, int buf_len)
{
	struct ether_hdr *hdr;
	uint16_t *ptr16;
	uint32_t ip_cksum;
	uint16_t data_len = buf_len - sizeof(struct ether_hdr) - sizeof(struct ipv4_hdr);
	uint16_t pkt_len;
	struct ipv4_hdr *ip_hdr;
	struct udp_hdr *udp_hdr;

	hdr = (struct ether_hdr *)buf;
	ip_hdr = (struct ipv4_hdr *)(buf+(sizeof(struct ether_hdr)));
	udp_hdr = (struct udp_hdr *)(buf+(sizeof(struct ether_hdr))+(sizeof(struct ipv4_hdr)));

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
	udp_hdr->dst_port = rte_cpu_to_be_16(1024);
	udp_hdr->dgram_len      = rte_cpu_to_be_16(data_len);
	udp_hdr->dgram_cksum    = 0; /* No UDP checksum. */
}

void Router::add_port(Port *port)
{
	ports_vector.push_back(port);
}





























