#!/usr/bin/bash

mount_script_dir=$(dirname "$(readlink -f "$BASH_SOURCE[0]")")
$mount_script_dir/check-pm-sector.sh 0

sleep 1

sudo mkfs.ext4 -F /dev/pmem0s
sudo mount /dev/pmem0s /mnt/pmem0/
sudo chown $USER /mnt/pmem0/
