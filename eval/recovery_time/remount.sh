#!/bin/bash

source "../tools/common.sh"

ABS_PATH=$(where_is_script "$0")
TOOLS_PATH=$ABS_PATH/../tools

pmem_probe_file=$TOOLS_PATH/mount-scripts/pmem_probe/pm_config.txt

sudo parradm create /dev/parfs_pmem_ar0 $pmem_probe_file
sudo mount -t parfs -o dele_thrds=1 /dev/parfs_pmem_ar0 /mnt/pmem0/

sudo chown $USER /mnt/pmem0/
