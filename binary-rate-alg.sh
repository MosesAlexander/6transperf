#!/bin/bash
#Parameters
impl="repotop-10G-n012-IP4-routing" # name of the tested implementation (used for logging)
dir="b" # valid values: b,f,r; b: bidirectional, f: forward (Left to Right, 6 --> 4), r: reverse (Right to Left, 6 <-- 4)
max=14000000 # maximum packet rate
fs=1024 # IPv6 frame size; IPv4 frame size is always 20 bytes less
xpts=10 # duration (in seconds) of an experiment instance
to=2000 # timeout in milliseconds
n=2 # foreground traffic, if ( frame_counter % n < m )
m=2 # E.g. n=m=2 is all foreground traffic; n=2,m=0 is all background traffic; n=10,m=9 is 90% fg and 10% bg
sleept=10 # sleeping time between the experiments
e=1 # measurement error: the difference betwen the values of the higher and the lower bound of the binary search, when finishing
no_exp=20 # number of experiments
res_dir="results" # base directory for the results (they will be copied there at the end)

############################

# Generate log/csv file headers
echo "#############################" > ratetest.log
echo "Tested Implementation: $impl" >> ratetest.log
echo "Frame size: $fs:" >> ratetest.log
echo "Direction $dir:" >> ratetest.log
echo "Value of n: $n" >> ratetest.log
echo "Value of m: $m" >> ratetest.log
echo "Duration (sec): $xpts" >> ratetest.log
echo "Starting rate (QPS): $((max/2))" >> ratetest.log
echo "Timeout value (sec): $to"  >> ratetest.log
echo "Measurement Error: $e"  >> ratetest.log
echo "Sleep Time (sec): $sleept" >> ratetest.log
date +'Date&Time: %Y-%m-%d %H:%M:%S.%N' >> ratetest.log
echo "#############################" >> ratetest.log
echo "No, Size, Dir, n, m, Duration, Initial Rate, Timeout, Error, Date, Iterations needed, rate" > rate.csv

# Execute $no_exp number of experiments
for (( N=1; N <= $no_exp; N++ ))
do
	# Execute a binary search in the [l, h] interval
	l=0 #Initialize lower bound to 0
	h=$max #Initialize higher bound with maximum query rate
	for (( i=1; $((h-l)) > $e; i++ ))
	do
		# Log some information about this step
		echo --------------------------------------------------- >> ratetest.log
		date +'%Y-%m-%d %H:%M:%S.%N Iteration no. '"$N"'-'"$i" >> ratetest.log
		echo ---------------------------------------------------  >> ratetest.log
		r=$(((h+l)/2)) # initialize the test rate with (h+l)/2
		echo "Testing rate: $r fps."
		echo "Testing rate: $r fps." >> ratetest.log
		echo "Command line is: ./6transperf --master-lcore 8 -l 0-3,8 -n1 -- --num-ports 2 --port0-lcoremask 0xc --port1-lcoremask 0x03 --num-queues 1 --mode dslite --dslite-testmode selftest --fps $r --packet-len $fs --duration $xpts --rxdelay 15 --txdelay 15"
		# Execute the test program
		sret=255
		while [[ "$sret" == "255" ]]; do
			./6transperf --master-lcore 8 -l 0-3,8 -n1 -- --num-ports 2 --port0-lcoremask 0xc --port1-lcoremask 0x03 --num-queues 1 --mode dslite --dslite-testmode selftest --fps $r --packet-len $fs --duration $xpts --rxdelay 15 --txdelay 15 > temp.out 2>&1
			#./build/siitperf-tp $fs $r $xpts $to $n $m > temp.out 2>&1
			sret=$?
		done
		# Log and print out info
		cat temp.out >> ratetest.log
		cat temp.out | tail
		# Check for any errors
		error=$(grep 'Error:' temp.out)
                if [ -n "$error" ]; then
			echo "Error occurred, testing must stop."
			exit -1;
		fi
		# Collect and evaluate the results (depending on the direction of the test)
		if [ "$dir" == "b" ]; then
			port0_rec=$(grep "Port 0 total" -A 2 temp.out  | grep rx | awk '{print $2}')
			port1_rec=$(grep "Port 1 total" -A 2 temp.out  | grep rx | awk '{print $2}')
			echo Port 0: $port0_rec frames were received from the required $((xpts*r)) frames
			echo Port 0: $port0_rec frames were received from the required $((xpts*r)) frames >> ratetest.log
			echo Port 1: $port1_rec frames were received from the required $((xpts*r)) frames
			echo Port 1: $port1_rec frames were received from the required $((xpts*r)) frames >> ratetest.log
			if [ $port0_rec -eq $((xpts*r)) ] && [ $port1_rec -eq $((xpts*r)) ]; then
				l=$r
				echo TEST PASSED
				echo TEST PASSED >> ratetest.log
			# Otherwise we choose the lower half interval
			else
				h=$r
				echo TEST FAILED
				echo TEST FAILED >> ratetest.log
	        	fi
		fi
		if [ "$dir" == "f" ]; then
			port0_rec=$(grep "Port 0 total" -A 2 temp.out  | grep rx | awk '{print $2}')
			echo Port 0: $port0_rec frames were received from the required $((xpts*r)) frames
			echo Port 0: $port0_rec frames were received from the required $((xpts*r)) frames >> ratetest.log
			if [ $port0_rec -eq $((xpts*r)) ]; then
				l=$r
				echo TEST PASSED
				echo TEST PASSED >> ratetest.log
			# Otherwise we choose the lower half interval
			else
				h=$r
				echo TEST FAILED
				echo TEST FAILED >> ratetest.log
	        	fi
		fi
		if [ "$dir" == "r" ]; then
			port1_rec=$(grep "Port 1 total" -A 2 temp.out  | grep rx | awk '{print $2}')
			echo Port 1: $port1_rec frames were received from the required $((xpts*r)) frames
			echo Port 1: $port1_rec frames were received from the required $((xpts*r)) frames >> ratetest.log
			if [ $port1_rec -eq $((xpts*r)) ]; then
				l=$r
				echo TEST PASSED
				echo TEST PASSED >> ratetest.log
			# Otherwise we choose the lower half interval
			else
				h=$r
				echo TEST FAILED
				echo TEST FAILED >> ratetest.log
	        	fi
		fi
		# Some further logging (and preparing results)
		echo "New diff: $((h-l))"
		echo "New diff: $((h-l))" >> ratetest.log
		date +'%Y-%m-%d %H:%M:%S, '"$i"', '"$r"  > temprate.csv
		echo "Sleeping for $sleept seconds..."
		echo "Sleeping for $sleept seconds..." >> ratetest.log
		# Sleep for $sleept time to give DUT a chance to relax
		sleep $sleept
	done # (end of the binary search)
	# Collect results
	summary=$(cat temprate.csv)
	echo "$N, $fs, $dir, $n, $m, $xpts, $(($max/2)), $to, $e, $summary" >> rate.csv
	rm temp*
done # (end of the $no_exp number of experiments)
# Save the results (may be commented out if not needed)
dirname="$res_dir/$(date +$impl'-'$dir'-'$fs'-'$n'-'$m'-'$xpts'-'$to'-'$e'-%F-%H%M')"
mkdir -p $dirname
mv ratetest.log $dirname/
mv rate.csv $dirname/
mv nohup.out $dirname/
cp -a siitperf.conf $dirname/
