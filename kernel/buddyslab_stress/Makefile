obj-m += buddyslab.o

PHONY: all

build: buddyslab.c
	make M=`pwd` -C /usr/src/kernels/`uname -r` modules

clean:
	make M=`pwd` -C /usr/src/kernels/`uname -r` clean

test: build
	insmod buddyslab.ko
	time cat /proc/buddyslab 2>/dev/null || true
	rmmod buddyslab

