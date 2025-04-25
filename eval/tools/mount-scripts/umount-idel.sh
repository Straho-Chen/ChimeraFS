#!/usr/bin/bash

sudo umount /mnt/pmem0/
sudo parradm delete /dev/parfs_pmem_ar0
