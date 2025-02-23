#!/usr/bin/bash

del_thrds=$1

if [ -z "$del_thrds" ]; then
    echo "Usage: $0 <dele_thrds>"
    exit 1
fi

mount_script_dir=$(dirname "$(readlink -f "$BASH_SOURCE[0]")")
$mount_script_dir/check-pm-fsdax.sh 0

#Two sockets
sudo parradm create /dev/parfs_pmem_ar0 /dev/pmem0

sudo mount -t parfs -o init,dele_thrds=$del_thrds /dev/parfs_pmem_ar0 /mnt/pmem0/
sudo chown $USER /mnt/pmem0/
