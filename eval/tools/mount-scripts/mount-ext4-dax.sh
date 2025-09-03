#!/usr/bin/bash

mount_script_dir=$(dirname "$(readlink -f "$BASH_SOURCE[0]")")
$mount_script_dir/check-pm-fsdax.sh 0

sleep 1

sudo mkfs.ext4 -F /dev/pmem0
sudo mount -o dax /dev/pmem0 /mnt/pmem0/
sudo chown $USER /mnt/pmem0/
