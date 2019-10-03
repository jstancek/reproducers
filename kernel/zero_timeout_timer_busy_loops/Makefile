obj-m += repro.o

build:
	make -C /usr/src/kernels/`uname -r` M=`pwd` modules

clean:
	make -C /usr/src/kernels/`uname -r` M=`pwd` clean

test: build
	for i in `seq 1 10`; do \
	echo "Round $$i"; \
	rmmod repro; \
	echo 0 > /sys/devices/system/cpu/cpu1/online && echo 1 > /sys/devices/system/cpu/cpu1/online && sleep 1 && taskset -c 1 insmod repro.ko; \
	if [ $$? -eq 0 ]; then sleep 60; rmmod repro; fi; \
	dmesg -c | grep "soft lockup" && break; \
	done
