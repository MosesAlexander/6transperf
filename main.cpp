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
#include "tester.h"
#include "b4.h"
#include "router.h"
#include "aftr.h"

using namespace std;

config_type_t op_mode = TESTER_CONFIG; // tester config is default

Router *router;

static void signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		cout<<"\nSignal "<< signum <<" received, preparing to exit..."<<endl;
		//force_quit();
		/* Set flag to indicate the force termination. */
		//f_quit = 1;
		/* exit with the expected status */
		signal(signum, SIG_DFL);
		kill(getpid(), signum);
	}

}

int
traffic_lcore_thread(void *arg __rte_unused)
{
	switch (op_mode)
	{
	case TESTER_CONFIG:
		dynamic_cast<DSLiteTester*>(router)->testb4();
		break;
	case B4_CONFIG:
		dynamic_cast<DSLiteB4Router*>(router)->forward();
		break;
	case AFTR_CONFIG:
		//TODO
		break;
	}
}

int main(int argc, char **argv)
{
	int diag, ret, num_ports;
	uint16_t port_id;
	int lcore_id;
	int transperf_logtype;
	vector<Port*> ports_vector;
	uint64_t ports_lcore_mask[2];
	bool mode_selected = false;

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	diag = rte_eal_init(argc, argv);	
	if (diag < 0)
		rte_panic("Cannot init EAL\n");

	transperf_logtype = rte_log_register("transperf");
	if (transperf_logtype < 0)
		rte_panic("Cannot register log type");
	rte_log_set_level(transperf_logtype, RTE_LOG_DEBUG);

	for (int i = 1 ; i < argc; i++)
	{
		if (argv[i] == string("--num-ports"))
		{
			int arg;
			arg = atoi(argv[i+1]);
			num_ports = arg;
		}

		if (argv[i] == string("--mode"))
		{
			mode_selected = true;
			if (string(argv[i+1]) == string("b4"))
			{
				op_mode = B4_CONFIG;
				router = new DSLiteB4Router();

			}
			else if (string(argv[i+1]) == string("aftr"))
			{
				op_mode = AFTR_CONFIG;
				router = new DSLiteAFTRRouter();
			}
			else if (string(argv[i+1]) == string("tester"))
			{
				op_mode = TESTER_CONFIG;
				router = new DSLiteTester();
			}
			else
			{
				cout<<"No mode selected, choosing Tester as default"<<endl;
				op_mode = TESTER_CONFIG;
				router = new DSLiteTester();
			}
		}

		if (argv[i] == string("--port0-lcoremask"))
		{
			ports_lcore_mask[0] = stoull(string(argv[i+1]), nullptr, 16);
		}

		if (argv[i] == string("--port1-lcoremask"))
		{
			ports_lcore_mask[1] = stoull(string(argv[i+1]), nullptr, 16);
		}
	}

	if (!mode_selected)
	{
		op_mode = TESTER_CONFIG;
		router = new DSLiteTester();

	}

	num_sockets = rte_socket_count();
	//TODO: Must support more lcores than 64
	router->port0_lcore_mask = ports_lcore_mask[0];
	router->port1_lcore_mask = ports_lcore_mask[1];
	router->set_lcore_allocation(ports_lcore_mask[0], ports_lcore_mask[1]);

	for (int i = 0; i < num_sockets; i++) {
		string pool_name = string("pool_");
		pool_name = pool_name + to_string(i);
		struct rte_mempool *pool = rte_pktmbuf_pool_create(pool_name.c_str(),
								NUM_BUFFERS, MEMPOOL_CACHE_SIZE,
						0, RTE_MBUF_DEFAULT_BUF_SIZE, i);
		if (pool == nullptr)
			rte_exit(EXIT_FAILURE, "Failed to init memory pool\n");

		mempools_vector.push_back(pool);
	}


	for (int i = 0; i < num_ports; i++)
	{
		Port *port = new Port(i);
		PortConfig *port_config = new PortConfig("./config_file", i, op_mode);
		port->init(1, port_config);
		cout << "Port " << i << " mac address: "<< std::hex << std::setfill('0') << std::setw(2)
							<< (unsigned int)port->mac_addr[0] << ":"
							<< std::hex << std::setfill('0') << std::setw(2)
							<< (unsigned int)port->mac_addr[1] << ":"
							<< std::hex << std::setfill('0') << std::setw(2)
							<< (unsigned int)port->mac_addr[2] << ":"
							<< std::hex << std::setfill('0') << std::setw(2)
							<< (unsigned int)port->mac_addr[3] << ":"
							<< std::hex << std::setfill('0') << std::setw(2)
							<< (unsigned int)port->mac_addr[4] << ":"
							<< std::hex << std::setfill('0') << std::setw(2)
							<< (unsigned int)port->mac_addr[5] << endl;

		router->add_port(port);
	}

	router->set_ports_from_config();

	rte_eal_mp_remote_launch(traffic_lcore_thread, NULL, SKIP_MASTER);

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		ret |= rte_eal_wait_lcore(lcore_id);
	}

out:
	for (vector<Port*>::iterator it = ports_vector.begin() ; it != ports_vector.end(); ++it)
		delete *it;

	return 0;
}
