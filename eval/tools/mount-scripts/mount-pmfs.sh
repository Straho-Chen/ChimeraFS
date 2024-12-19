#!/usr/bin/bash

mount_script_dir=$(dirname "$(readlink -f "$BASH_SOURCE[0]")")
$mount_script_dir/check-pm-fsdax.sh

sudo mount -t pmfs -o init /dev/pmem0 /mnt/pmem0/
sudo chown $USER /mnt/pmem0/
