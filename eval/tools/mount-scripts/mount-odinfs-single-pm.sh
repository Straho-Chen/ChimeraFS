#!/usr/bin/bash

del_thrds=$1

if [ -z "$del_thrds" ]; then
    echo "Usage: $0 <dele_thrds>"
    exit 1
fi

mount_script_dir=$(dirname "$(readlink -f "$BASH_SOURCE[0]")")

pmem_num=$(ndctl list -R | grep "dev" | wc -l)

for i in $(seq 0 $((pmem_num - 1))); do
    $mount_script_dir/check-pm-fsdax.sh $i
done

# gen pm config
pmem_probe_dir=$mount_script_dir/pmem_probe
if [ ! -d "$pmem_probe_dir" ]; then
    echo "error: $pmem_probe_dir not found"
    exit -1
fi

pmem_probe_file=$pmem_probe_dir/pm_config.txt

cd $pmem_probe_dir && python3 gen_config.py 1

sudo parradm create /dev/odinfs_pmem_ar0 $pmem_probe_file

sudo mount -t odinfs -o init,dele_thrds=$del_thrds /dev/odinfs_pmem_ar0 /mnt/pmem0/
sudo chown $USER /mnt/pmem0/
