#!/usr/bin/bash

mount_script_dir=$(dirname "$(readlink -f "$BASH_SOURCE[0]")")
$mount_script_dir/check-pm-fsdax.sh 0

cow=$1

if [[ "$cow" == "1" ]]; then
    sudo mount -t NOVA -o init,data_cow /dev/pmem0 /mnt/pmem0/
else
    sudo mount -t NOVA -o init /dev/pmem0 /mnt/pmem0/
fi
sudo chown $USER /mnt/pmem0/
