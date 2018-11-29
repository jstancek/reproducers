obj-m += sha1_test.o

PHONY: all

all: build

build: sha1_test.c
	make M=`pwd` -C /usr/src/kernels/`uname -r` modules

clean:
	make M=`pwd` -C /usr/src/kernels/`uname -r` clean


