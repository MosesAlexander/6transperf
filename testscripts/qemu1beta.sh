sudo qemu-system-x86_64 -device virtio-net-pci,netdev=net0,mac=52:54:01:22:34:39 -netdev tap,id=net0,ifname=tap0,script=no,downscript=no \
			-show-cursor -usb -device usb-tablet -object rng-random,filename=/dev/urandom,id=rng0 \
			-device virtio-rng-pci,rng=rng0  -nographic  -cpu host -smp 9,sockets=2,cores=2 -enable-kvm -m 5048 \
			-numa node,nodeid=0,cpus=0-4 -numa node,nodeid=1,cpus=5-8 \
			-serial mon:stdio -serial null -kernel /home/obsrwr/games/images/bzImage \
			-append 'root=/dev/sdb rw highres=off  console=ttyS0 hugepages=1536 ip=192.168.7.3::192.168.7.1:255.255.255.0::eth0 vga=0 uvesafb.mode_option=640x480-32 oprofile.timer=1 uvesafb.task_timeout=-1 isolcpus=1,2,3,4,5,6,7,8 nohz_full=1,2,3,4,5,6,7,8' \
			-machine q35 \
			-drive file=/home/obsrwr/games/images/rootfs1.ext4,if=ide,format=raw \
			-device pxb-pcie,id=bridge1,bus=pcie.0,numa_node=1,bus_nr=20  \
			-device x3130-upstream,bus=bridge1,id=upstream1 \
			-device pxb-pcie,id=bridge2,bus=pcie.0,numa_node=0,bus_nr=1  \
			-device x3130-upstream,bus=bridge2,id=upstream2 \
			-device virtio-net-pci,netdev=net1,mac=52:54:01:32:34:02,bus=upstream1,mq=on,vectors=6 -netdev tap,id=net1,ifname=tap2,script=no,downscript=no,vhost=on,queues=2 \
			-device virtio-net-pci,netdev=net2,mac=52:54:09:62:34:01,bus=upstream2,mq=on,vectors=6 -netdev tap,id=net2,ifname=tap1,script=no,downscript=no,vhost=on,queues=2 \
			-gdb tcp::9000

