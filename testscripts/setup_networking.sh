if [ "$1" = "ips" ] ; then
        sudo ip addr add 10.1.0.1/24 dev tap0
        sudo ip link set dev tap0 up
        sudo ip addr add 10.2.0.1/24 dev tap3
        sudo ip link set dev tap3 up
fi

if [ "$1" = "bridge" ] ; then
        sudo brctl addbr br1
        sudo brctl addbr br2
        sudo brctl addif br1 tap1
        sudo brctl addif br1 tap4
        sudo brctl addif br2 tap2
        sudo brctl addif br2 tap5
        sudo ip link set dev tap1 up
        sudo ip link set dev tap2 up
        sudo ip link set dev tap4 up
        sudo ip link set dev tap5 up
        sudo ip link set dev br1 up
        sudo ip link set dev br2 up
fi

if [ "$1" = "host1" ] ; then
        ip addr add 10.1.0.2/24 dev eth0
        ip link set dev eth0 up
fi

if [ "$1" = "host2" ] ; then
        ip addr add 10.2.0.2/24 dev eth0
        ip link set dev eth0 up
	ip link add name ipip6 type ip6tnl local 2001:2:0:0:0:0:0:2 remote 2001:2:0:0:0:0:0:1 mode any dev eth2
	ip addr add 168.196.2.2/24 dev ipip6
	ip -6 addr add 2001:2:0:0:0:0:0:2/64 dev eth2
	ip link set dev eth2 up
	ip link set dev ipip6 up
	ip link set dev eth1 up
	ip addr add 168.196.1.2/24 dev eth1
	TOIP=168.196.1.1
	FROMIP=200.1.2.3

	tc qdisc add dev ipip6 root handle 1: htb
	tc qdisc add dev ipip6 ingress

	tc filter add dev ipip6 parent ffff: protocol ip prio 1 u32 match ip dst $FROMIP action nat ingress $FROMIP $TOIP
	tc filter add dev ipip6 parent 1: protocol ip prio 1 u32 match ip src $TOIP action nat egress $TOIP $FROMIP
	ip route add 168.196.2.1 via 168.196.2.2
	ip route add 168.196.1.1 via 168.196.1.2
	arp -s 168.196.1.1 52:54:00:12:34:02
	ip -6 neigh add 2001:2::1 lladdr 52:54:09:11:24:04 dev eth2
	ip -6 neigh change 2001:2::1 lladdr 52:54:09:11:24:04 dev eth2
	sysctl -w net.ipv4.ip_forward=1
fi
