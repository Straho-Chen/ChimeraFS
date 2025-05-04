#!/bin/bash

source "../tools/common.sh"

ABS_PATH=$(where_is_script "$0")
TOOLS_PATH=$ABS_PATH/../tools

pmem_probe_file=$TOOLS_PATH/mount-scripts/pmem_probe/pm_config.txt

./test.sh

sleep 1

# compile_fs "parfs" "0"
# sudo parradm create /dev/parfs_pmem_ar0 $pmem_probe_file
# sudo mount -t parfs -o dele_thrds=12 /dev/parfs_pmem_ar0 /mnt/pmem0/

compile_fs "winefs" "0"
sudo mount -t winefs /dev/pmem0 /mnt/pmem0/

sudo chown $USER /mnt/pmem0/
