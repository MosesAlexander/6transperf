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
#include "router.h"

using namespace std;

config_type_t op_mode = TESTER_CONFIG; // tester config is default
dslite_test_mode_t dslite_test_mode = AFTR;

Tester *tester;
int num_queues = 1;

// 1Gbps rate is the default
uint64_t target_rate_fps = 1000 * 1000 * 1000;

// size of an mbuf, total size of a packet, headers included
uint64_t buffer_length = 1024;

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

	if (signum == SIGUSR1)
	{
		for (int i = 0; i < num_queues; i++)
		{
			cout<<"Port 0 queue "<<i<<": rx: " <<std::dec<<tester->port0_stats[i].rx_frames;
			cout<<" tx: "<<std::dec<<tester->port0_stats[i].tx_frames<<endl;
			cout<<"Port 1 queue "<<i<<": rx: " <<std::dec<<tester->port1_stats[i].rx_frames;
			cout<<" tx: " <<std::dec<<tester->port1_stats[i].tx_frames<<endl;
		}
	}
}

int
traffic_lcore_thread(void *arg __rte_unused)
{
	switch (op_mode)
	{
	case TESTER_CONFIG:
		dynamic_cast<DSLiteTester*>(tester)->runtest(target_rate_fps, buffer_length, dslite_test_mode);
		break;
	case B4_CONFIG:
		dynamic_cast<DSLiteTester*>(tester)->runtest(target_rate_fps, buffer_length, dslite_test_mode);
		break;
	case AFTR_CONFIG:
		dynamic_cast<DSLiteTester*>(tester)->runtest(target_rate_fps, buffer_length, dslite_test_mode);
		break;
	}

	return 0;
}

//TODO: Should move all the latency calculation code into its own class

void sort_latencies_array(uint64_t *lat_arr, uint64_t num_elems)
{
	std::sort(lat_arr, lat_arr+num_elems);
}

void sort_ipdv_array(int64_t *lat_arr, uint64_t num_elems)
{
	std::sort(lat_arr, lat_arr+(int64_t)num_elems);
}

uint64_t get_merged_array_size(uint64_t *queue_pair_index, int num_queues)
{
	uint64_t merged_array_size = 0;

	for (int array = 0; array < num_queues; array++)
	{
		merged_array_size += queue_pair_index[array];
	}

	return merged_array_size;
}

inline uint64_t select_latency_by_minimum_tx_time(struct timestamp_pair **tsc_pairs_array,
		uint64_t *queue_pair_index, uint64_t *local_queue_pair_index, int num_queues)
{
	uint64_t current_min = ULONG_MAX;
	uint64_t tmp;
	int current_winner = 0;
	uint64_t ret_latency;

	for (int qp_index = 0; qp_index < num_queues; qp_index++)
	{
		// Did we reach the end of the current array?
		if (local_queue_pair_index[qp_index] != queue_pair_index[qp_index])
		{
			if (current_min > tsc_pairs_array[qp_index][local_queue_pair_index[qp_index]].tx_tsc_value)
			{
				current_min = tsc_pairs_array[qp_index][local_queue_pair_index[qp_index]].tx_tsc_value;
				current_winner = qp_index;
			}
		} 
	}

	ret_latency = tsc_pairs_array[current_winner][local_queue_pair_index[current_winner]].rx_tsc_value -
		tsc_pairs_array[current_winner][local_queue_pair_index[current_winner]].tx_tsc_value;

	local_queue_pair_index[current_winner]++;

	return ret_latency;
}

// This function takes a table of arrays containing timestamps
// and returns an array of latencies, merging the tsc arrays consumes
// them.
uint64_t *merge_tsc_arrays_into_latencies(struct timestamp_pair **tsc_pairs_array, uint64_t *queue_pair_index, int num_queues)
{
	// So we need to get the mean of the latencies
	// We have up to num_queues arrays, do we merge these?
	uint64_t merged_array_size;
	uint64_t *merged_array;
	uint64_t tsc_idx = 0;
	uint64_t merged_idx = 0;
	uint64_t *local_index_per_qp;

	merged_array_size = get_merged_array_size(queue_pair_index, num_queues);

	merged_array = (uint64_t*) malloc(sizeof(uint64_t) * merged_array_size);
	local_index_per_qp = (uint64_t*) malloc(sizeof(uint64_t) * num_queues);

	memset(local_index_per_qp, 0, sizeof(uint64_t) * num_queues);

	for (int i = 0; i < merged_array_size; i++)
	{
		merged_array[i] = select_latency_by_minimum_tx_time(tsc_pairs_array, queue_pair_index, local_index_per_qp, num_queues);
	}

	for (int array = 0; array < num_queues; array++)
	{
		// We don't need this tsc_pairs_array anymore
		munmap(tsc_pairs_array[array], queue_pair_index[array] * sizeof(struct timestamp_pair));
	}

	free(local_index_per_qp);

	// Return array of latencies sorted according to transmit time
	return merged_array;
}

template<class T>
double median_of_latencies(T *latencies_array, uint64_t merged_array_size)
{
	double mean, median;
	uint64_t idx;

	idx = merged_array_size / 2;
	if ((merged_array_size % 2) == 0)
	{
		median = ((double)latencies_array[idx] + (double)latencies_array[idx-1])/2.0;
	}
	else
	{
		median = latencies_array[idx];
	}

	return median;
}


double rand_double(double min, double max)
{
    return  (max - min) * ((((double) rand()) / (double) RAND_MAX)) + min ;
}

// 99.9th percentile of the dataset
uint64_t calculate_wcl(uint64_t *latencies_array, uint64_t size)
{
	uint64_t idx;

	idx = size * 0.999 - 1;

	return latencies_array[idx];
}

// 99.9th percentile of the dataset - minimum of the dataset
uint64_t calculate_pdv(uint64_t *latencies_array, uint64_t size)
{
	uint64_t idx;

	idx = size * 0.999 - 1;

	// Since the array is sorted element 0 is the minimum
	return latencies_array[idx] - latencies_array[0];
}

typedef enum {
	TEST_LAT,
	TEST_PDV,
	TEST_IPDV,
} test_type_t;

// Consumes latencies array
int64_t *generate_ipdv_array_from_latencies_array(uint64_t *latencies_array, uint64_t size)
{
	int64_t *ipdv_array = (int64_t *)malloc(sizeof(int64_t)*(size-1));

	for (int i = 0 ; i < (size - 1); i++)
	{
		ipdv_array[i] = (int64_t)latencies_array[i+1] - (int64_t)latencies_array[i];
	}

	free(latencies_array);
	return ipdv_array;
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
	uint32_t duration = 10; // seconds
	uint32_t rx_delay = 2;
	uint32_t tx_delay = 2;
	bool timestamp_packets = false;
	test_type_t test_type = TEST_LAT;


	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGUSR1, signal_handler);

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

		if (argv[i] == string("--duration"))
		{
			unsigned long sec;
			sec = strtoul(argv[i+1], NULL, 10);
			duration = (uint32_t)sec;
		}

		if (argv[i] == string("--rxdelay"))
		{
			unsigned long sec;
			sec = strtoul(argv[i+1], NULL, 10);
			rx_delay = (uint32_t)sec;
		}
		if (argv[i] == string("--txdelay"))
		{
			unsigned long sec;
			sec = strtoul(argv[i+1], NULL, 10);
			tx_delay = (uint32_t)sec;
		}


		if (argv[i] == string("--num-queues"))
		{
			int arg;
			arg = atoi(argv[i+1]);
			num_queues = arg;
		}

		if (argv[i] == string("--fps"))
		{
			unsigned long long fps;
			fps = strtoull(argv[i+1], NULL, 10);
			target_rate_fps = (uint64_t)fps;
		}

		if (argv[i] == string("--timestamp-all-packets"))
		{
			timestamp_packets = true;
		}

		if (argv[i] == string("--packet-len"))
		{
			unsigned long long len;
			len = strtoull(argv[i+1], NULL, 10);
			buffer_length = (uint64_t) len;
		}

		if (argv[i] == string("--mode"))
		{
			mode_selected = true;
			if (string(argv[i+1]) == string("dslite"))
			{
				tester = new DSLiteTester();
			}
			else
			{
				cout<<"No mode selected, choosing Tester as default"<<endl;
				op_mode = TESTER_CONFIG;
				tester = new DSLiteTester();
			}
		}

		if (argv[i] == string("--dslite-testmode"))
		{
			if (string(argv[i+1]) == string("aftr"))
			{
				dslite_test_mode = AFTR;
			}
			else if (string(argv[i+1]) == string("b4"))
			{
				op_mode = B4_CONFIG;
				dslite_test_mode = B4;
			}
			else if (string(argv[i+1]) == string("both"))
			{
				dslite_test_mode = BOTH;
			}
			else if (string(argv[i+1]) == string("selftest"))
			{
				dslite_test_mode = SELFTEST;
			}
			else
			{
				cout<<"DSLite Test mode "<<string(argv[i+1])<<" not supported, choosing aftr as default"<<endl;
				dslite_test_mode = AFTR;
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

		if (argv[i] == string("--pdv"))
		{
			test_type = TEST_PDV;
		}

		if (argv[i] == string("--ipdv"))
		{
			test_type = TEST_IPDV;
		}

		if (argv[i] == string("--lat"))
		{
			test_type = TEST_LAT;
		}

	}

	if (!mode_selected)
	{
		op_mode = TESTER_CONFIG;
		tester = new DSLiteTester();

	}

	// preallocate buffer space for 
	if (timestamp_packets) {
		tester->timestamp_packets = timestamp_packets;
		tester->num_tsc_pairs_per_qp = (target_rate_fps * 1.25 * duration) / num_queues;
		tester->port0_tsc_pairs_array = (struct timestamp_pair **) malloc(num_queues * sizeof(struct timestamp_pair*));
		tester->port1_tsc_pairs_array = (struct timestamp_pair **) malloc(num_queues * sizeof(struct timestamp_pair*));

		for (int i = 0; i < num_queues; i++) {
			tester->port0_tsc_pairs_array[i] = (struct timestamp_pair *) mmap(NULL, tester->num_tsc_pairs_per_qp * sizeof(struct timestamp_pair),
					PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
			tester->port1_tsc_pairs_array[i] = (struct timestamp_pair *) mmap(NULL, tester->num_tsc_pairs_per_qp * sizeof(struct timestamp_pair),
					PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);

			memset(tester->port0_tsc_pairs_array[i], 0, tester->num_tsc_pairs_per_qp * sizeof(struct timestamp_pair));
			memset(tester->port1_tsc_pairs_array[i], 0, tester->num_tsc_pairs_per_qp * sizeof(struct timestamp_pair));
		}

		cout<<"Allocated " << tester->num_tsc_pairs_per_qp * sizeof(struct timestamp_pair) * 4 << " bytes in total for timestamps."<<endl;
	}

	// Spread the bandwith accross the queues
	target_rate_fps = target_rate_fps / num_queues;

	num_sockets = rte_socket_count();
	//TODO: Must support more lcores than 64
	tester->port0_lcore_mask = ports_lcore_mask[0];
	tester->port1_lcore_mask = ports_lcore_mask[1];

	for (int i = 0; i < num_ports; i++)
	{
		Port *port = new Port(i);
		PortConfig *port_config = new PortConfig("./config_file", i, op_mode);
		port->init(num_queues, port_config);
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

		tester->add_port(port);
	}

	tester->set_ports_from_config();
	tester->tester_duration = duration;
	tester->m_num_queues = num_queues;

	tx_running = true;
	rx_running = true;

	rte_eal_mp_remote_launch(traffic_lcore_thread, NULL, SKIP_MASTER);

	// Port needs some extra time before it can actually send, not waiting
	// may cause an overshoot in throughput
	usleep(tx_delay * 1000 * 1000);
	tester->tx_delay_flag = false;

	usleep(duration * 1000 * 1000);
	tx_running = false;
	usleep(rx_delay * 1000 * 1000);
	rx_running = false;

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		ret |= rte_eal_wait_lcore(lcore_id);
	}

	std::cout<<std::endl;
	// Port 0
	for (int i = 0; i < num_queues; i++)
	{
		cout<<"Port 0 queue "<<i<<":"<<endl<<"rx: " <<std::dec<<tester->port0_stats[i].rx_frames
			<<" frames ("<<tester->port0_stats[i].rx_frames * buffer_length<<" bytes, "
			<<tester->port0_stats[i].rx_frames * buffer_length * 8<<" bits)"<<endl;
		cout<<"tx: "<<std::dec<<tester->port0_stats[i].tx_frames
			<<" frames ("<<tester->port0_stats[i].tx_frames * buffer_length<<" bytes, "
			<<tester->port0_stats[i].tx_frames * buffer_length * 8<<" bits)"<<endl;
	}

	std::cout<<std::endl;
	// Port 1
	for (int i = 0; i < num_queues; i++)
	{
		cout<<"Port 1 queue "<<i<<":"<<endl<<"rx: " <<std::dec<<tester->port1_stats[i].rx_frames
			<<" frames ("<<std::dec<<tester->port1_stats[i].rx_frames * buffer_length<<" bytes, "
			<<tester->port1_stats[i].rx_frames * buffer_length * 8<<" bits)"<<endl;

		cout<<"tx: "<<std::dec<<tester->port1_stats[i].tx_frames
			<<" frames ("<<tester->port1_stats[i].tx_frames * buffer_length<<" bytes, "
			<<tester->port1_stats[i].tx_frames * buffer_length * 8<<" bits)"<<endl;

	}

	uint64_t total_rx_port0 = 0;
	uint64_t total_rx_port1 = 0;
	uint64_t total_tx_port0 = 0;
	uint64_t total_tx_port1 = 0;

	for (int i = 0; i < num_queues; i++)
	{
		total_rx_port0 += tester->port0_stats[i].rx_frames;
		total_tx_port0 += tester->port0_stats[i].tx_frames;
		total_rx_port1 += tester->port1_stats[i].rx_frames;
		total_tx_port1 += tester->port1_stats[i].tx_frames;
	}

	std::cout<<std::endl;

	cout<<"Port 0 total:"<<endl<<"rx: " <<std::dec<<total_rx_port0
		<<" frames ("<<std::dec<<total_rx_port0 * buffer_length<<" bytes, "
		<<total_rx_port0 * buffer_length * 8<<" bits)"<<endl;

	cout<<"tx: "<<std::dec<<total_tx_port0
		<<" frames ("<<total_tx_port0 * buffer_length<<" bytes, "
		<<total_tx_port0 * buffer_length * 8<<" bits)"<<endl;

	std::cout<<std::endl;

	cout<<"Port 1 total:"<<endl<<"rx: " <<std::dec<<total_rx_port1
		<<" frames ("<<std::dec<<total_rx_port1 * buffer_length<<" bytes, "
		<<total_rx_port1 * buffer_length * 8<<" bits)"<<endl;

	cout<<"tx: "<<std::dec<<total_tx_port1
		<<" frames ("<<total_tx_port1 * buffer_length<<" bytes, "
		<<total_tx_port1 * buffer_length * 8<<" bits)"<<endl;

	uint64_t port0_drop = total_tx_port1 - total_rx_port0;
	uint64_t port1_drop = total_tx_port0 - total_rx_port1;
	cout<<endl;
	cout<<"Port 0 dropped frames: "<<port0_drop<<"("<<(100*(double)port0_drop)/(double)total_tx_port1<<"%)"<<endl;
	cout<<"Port 1 dropped frames: "<<port1_drop<<"("<<(100*(double)port1_drop)/(double)total_tx_port0<<"%)"<<endl;

	// Calculate Latency, PDV, IPDV
	double median_port0;
	double median_port1;
	uint64_t *latencies_array_merged;
	int64_t *ipdv_array;
	uint64_t merged_array_size;
	uint64_t port0_wcl;
	uint64_t port1_wcl;
	uint64_t port0_pdv;
	uint64_t port1_pdv;
	int64_t port0_min_ipdv;
	int64_t port0_median_ipdv;
	int64_t port0_max_ipdv;
	int64_t port1_min_ipdv;
	int64_t port1_median_ipdv;
	int64_t port1_max_ipdv;


	
	if (timestamp_packets) {
		if (test_type == TEST_LAT || test_type == TEST_PDV)
		{
			latencies_array_merged = merge_tsc_arrays_into_latencies(tester->port0_tsc_pairs_array, tester->ports_vector[0]->port_pkt_identifier, num_queues);
			merged_array_size = get_merged_array_size(tester->ports_vector[0]->port_pkt_identifier, num_queues);
			sort_latencies_array(latencies_array_merged, merged_array_size);
			median_port0 = median_of_latencies<uint64_t>(latencies_array_merged, merged_array_size);
			port0_wcl = calculate_wcl(latencies_array_merged, merged_array_size);
			port0_pdv = calculate_pdv(latencies_array_merged, merged_array_size);
			free(latencies_array_merged);

			latencies_array_merged = merge_tsc_arrays_into_latencies(tester->port1_tsc_pairs_array, tester->ports_vector[1]->port_pkt_identifier, num_queues);
			merged_array_size = get_merged_array_size(tester->ports_vector[1]->port_pkt_identifier, num_queues);
			sort_latencies_array(latencies_array_merged, merged_array_size);
			median_port1 = median_of_latencies<uint64_t>(latencies_array_merged, merged_array_size);
			port1_wcl = calculate_wcl(latencies_array_merged, merged_array_size);
			port1_pdv = calculate_pdv(latencies_array_merged, merged_array_size);
			free(latencies_array_merged);

			uint64_t tsc_hz = rte_get_tsc_hz();

			cout<<"Port 0 median latency: "<<std::fixed<<(double)median_port0/(double)tsc_hz * 1000<<" msec"<<endl;
			cout<<"Port 0 worst-case latency: "<<(double)port0_wcl/(double)tsc_hz * 1000<<" msec"<<endl;
			cout<<"Port 0 PDV: "<<(double)port0_pdv/(double)tsc_hz * 1000<<" msec"<<endl;
			cout<<"Port 1 median latency: "<<(double)median_port1/(double)tsc_hz * 1000<<" msec"<<endl;
			cout<<"Port 1 worst-case latency: "<<(double)port1_wcl/(double)tsc_hz * 1000<<" msec"<<endl;
			cout<<"Port 1 PDV: "<<(double)port1_pdv/(double)tsc_hz * 1000<<" msec"<<endl;
		}
		else if (test_type == TEST_IPDV)
		{
			latencies_array_merged = merge_tsc_arrays_into_latencies(tester->port0_tsc_pairs_array, tester->ports_vector[0]->port_pkt_identifier, num_queues);
			merged_array_size = get_merged_array_size(tester->ports_vector[0]->port_pkt_identifier, num_queues);
			ipdv_array = generate_ipdv_array_from_latencies_array(latencies_array_merged, merged_array_size);
			sort_ipdv_array(ipdv_array, merged_array_size-1);
			port0_min_ipdv = ipdv_array[0];
			port0_median_ipdv = median_of_latencies<int64_t>(ipdv_array, merged_array_size-1); // IPDV array is 1 element smaller than the latencies array
			port0_max_ipdv = ipdv_array[merged_array_size-2]; // Last element in ipdv array is the max
			free(ipdv_array);

			latencies_array_merged = merge_tsc_arrays_into_latencies(tester->port1_tsc_pairs_array, tester->ports_vector[1]->port_pkt_identifier, num_queues);
			merged_array_size = get_merged_array_size(tester->ports_vector[1]->port_pkt_identifier, num_queues);
			ipdv_array = generate_ipdv_array_from_latencies_array(latencies_array_merged, merged_array_size);
			sort_ipdv_array(ipdv_array, merged_array_size-1);
			port1_min_ipdv = ipdv_array[0];
			port1_median_ipdv = median_of_latencies<int64_t>(ipdv_array, merged_array_size-1); // IPDV array is 1 element smaller than the latencies array
			port1_max_ipdv = ipdv_array[merged_array_size-2]; // Last element in ipdv array is the max
			free(ipdv_array);
			
			uint64_t tsc_hz = rte_get_tsc_hz();

			cout<<"Port 0 Min IPDV: "   <<std::fixed<<(double)port0_min_ipdv/(double)tsc_hz * 1000<<" msec"<<endl;
			cout<<"Port 0 Median IPDV: "<<(double)port0_median_ipdv/(double)tsc_hz * 1000<<" msec"<<endl;
			cout<<"Port 0 Max IPDV: "   <<(double)port0_max_ipdv/(double)tsc_hz * 1000<<" msec"<<endl;
			cout<<"Port 1 Min IPDV: "   <<(double)port1_min_ipdv/(double)tsc_hz * 1000<<" msec"<<endl;
			cout<<"Port 1 Median IPDV: "<<(double)port1_median_ipdv/(double)tsc_hz * 1000<<" msec"<<endl;
			cout<<"Port 1 Max IPDV: "   <<(double)port1_max_ipdv/(double)tsc_hz * 1000<<" msec"<<endl;
		}
	}

out:
	for (vector<Port*>::iterator it = ports_vector.begin() ; it != ports_vector.end(); ++it)
		delete *it;

	return 0;
}
