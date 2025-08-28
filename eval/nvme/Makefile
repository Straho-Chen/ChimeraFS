ABS_PATH=$(shell pwd)

all: main

main: libkp.so
	gcc -o main main.c common.c io_stats.c uring.c -mavx512f -DBITS_PER_LONG=64 -lkp -L$(ABS_PATH)/linux -Wl,-rpath=$(ABS_PATH)/linux

libkp.so:
	cd linux && $(MAKE)
