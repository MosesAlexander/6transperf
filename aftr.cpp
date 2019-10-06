#include "aftr.h"

void DSLiteAFTRRouter::set_lcore_allocation(uint64_t port0_mask, uint64_t port1_mask)
{

}

void DSLiteAFTRRouter::set_ports_from_config(void)
{
	if (ports_vector[0]->m_config->is_ipip6_tun_intf) {
		tunnel_port = ports_vector[0];
		local_port = ports_vector[1];
	} else if (ports_vector[1]->m_config->is_ipip6_tun_intf) {
		tunnel_port = ports_vector[1];
		local_port = ports_vector[0];
	}
}

