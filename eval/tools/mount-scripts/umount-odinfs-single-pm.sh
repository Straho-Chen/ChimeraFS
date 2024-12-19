#!/usr/bin/bash

sudo umount /mnt/pmem0/
sudo parradm delete

rm -rf /dev/pmem_ar0
