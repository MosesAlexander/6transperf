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

using namespace std;

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
	int socket_id = rte_socket_id();
	Port *port = ports_vector[socket_id];

	port->send(10);
}

int main(int argc, char **argv)
{
	int diag, ret, num_ports;
	uint16_t port_id;
	int lcore_id;
	int transperf_logtype;

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
		int arg;

		if (argv[i] == string("--num-ports"))
		{
			arg = atoi(argv[i+1]);
			num_ports = arg;
		}
	}

	num_sockets = rte_socket_count();

	for (int i = 0; i < num_sockets; i++) {
		string pool_name = string("pool_");
		pool_name = pool_name + to_string(i);
		struct rte_mempool *pool = rte_pktmbuf_pool_create(pool_name.c_str(),
								NUM_BUFFERS, RTE_CACHE_LINE_SIZE,
						0, RTE_MBUF_DEFAULT_BUF_SIZE, i);
		if (pool == nullptr)
			rte_exit(EXIT_FAILURE, "Failed to init memory pool\n");

		mempools_vector.push_back(pool);
	}


	for (int i = 0; i < num_ports; i++)
	{
		Port *port = new Port(i);
		PortConfig *port_config = new PortConfig("./config_file", i, TESTER_CONFIG);
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
		ports_vector.push_back(port);
	}

	rte_eal_mp_remote_launch(traffic_lcore_thread, NULL, SKIP_MASTER);

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		ret |= rte_eal_wait_lcore(lcore_id);
	}

out:
	for (vector<Port*>::iterator it = ports_vector.begin() ; it != ports_vector.end(); ++it)
		delete *it;

	return 0;
}
