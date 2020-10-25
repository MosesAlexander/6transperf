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

#include "port.h"
#include "router.h"

enum dslite_test_mode_t {
	AFTR,
	B4,
	BOTH,
	SELFTEST,
};

class DSLiteTester : public Tester
{
public:
	uint64_t port0_lcore_rx_mask;
	uint64_t port0_lcore_tx_mask;
	uint64_t port1_lcore_rx_mask;
	uint64_t port1_lcore_tx_mask;
	Port *port0, *port1;

	void runtest(uint64_t target_rate, uint64_t buf_len, dslite_test_mode_t test_mode);
	void set_ports_from_config(void);
	void set_lcore_allocation(uint64_t port0_mask, uint64_t port1_mask);
};
