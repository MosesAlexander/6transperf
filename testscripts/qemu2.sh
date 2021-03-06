sudo qemu-system-x86_64 -device virtio-net-pci,netdev=net3,mac=52:54:00:12:34:02 -netdev tap,id=net3,ifname=tap3,script=no,downscript=no \
			-drive file=/home/obsrwr/games/images/rootfs2.ext4,if=virtio,format=raw \
			-show-cursor -usb -device usb-tablet -object rng-random,filename=/dev/urandom,id=rng0 \
			-device virtio-rng-pci,rng=rng0  -nographic  -cpu host -smp 5,sockets=2,cores=2 -enable-kvm -m 2048 \
			-numa node,nodeid=0,cpus=0-2 -numa node,nodeid=1,cpus=3-4 \
			-device virtio-net-pci,netdev=net4,mac=52:54:09:11:24:04 -netdev tap,id=net4,ifname=tap4,script=no,downscript=no \
			-device virtio-net-pci,netdev=net5,mac=52:54:02:12:31:05 -netdev tap,id=net5,ifname=tap5,script=no,downscript=no \
			-serial mon:stdio -serial null -kernel /home/obsrwr/games/images/bzImage \
			-append 'root=/dev/vda rw highres=off  console=ttyS0 hugepages=512 ip=192.168.7.4::192.168.7.1:255.255.255.0::eth0 vga=0 uvesafb.mode_option=640x480-32 oprofile.timer=1 uvesafb.task_timeout=-1 isolcpus=1,2,3,4 nohz_full=1,2,3,4'
