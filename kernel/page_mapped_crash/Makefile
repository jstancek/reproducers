obj-m += repro.o
obj-m += repro2.o

all:
	 make -C /usr/src/kernels/`uname -r` M=`pwd` modules
