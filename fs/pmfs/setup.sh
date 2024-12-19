#!/bin/sh

if [ ! "$1" ]; then
    dev=/dev/pmem0
else
    dev=$1
fi

if [ ! "$2" ]; then
    mnt=/mnt/pmem0
else
    mnt=$2
fi

if [ ! "$3" ]; then
    make
else 
    make "$3"
fi

if [ ! "$4" ]; then
    measure_timing=0
else 
    measure_timing=$4
fi

sudo umount "$mnt"
sudo rmmod pmfs
sudo insmod pmfs.ko measure_timing="$measure_timing"

# sleep 1

sudo mount -t pmfs -o init "$dev" "$mnt"

#cp test1 /mnt/ramdisk/
#dd if=/dev/zero of=/mnt/ramdisk/test1 bs=1M count=1024 oflag=direct
