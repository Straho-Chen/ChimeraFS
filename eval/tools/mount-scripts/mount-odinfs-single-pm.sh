#!/usr/bin/bash

del_thrds=$1

mount_script_dir=$(dirname "$(readlink -f "$BASH_SOURCE[0]")")
$mount_script_dir/check-pm-fsdax.sh

#Two sockets
rm -rf /dev/pmem_ar0
sudo parradm create /dev/pmem0

sudo mount -t odinfs -o init,dele_thrds=$del_thrds /dev/pmem_ar0 /mnt/pmem0/
sudo chown $USER /mnt/pmem0/
