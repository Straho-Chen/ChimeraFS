#!/usr/bin/bash

function where_is_script() {
    local script=$1
    cd "$(dirname "$script")" && pwd
}

ABS_PATH=$(where_is_script "$0")

# mount script path

MOUNT_SCRIPT_PATH=$ABS_PATH/mount-scripts

# free memory at first
echo "Drop caches..."
sync
echo 3 | sudo tee /proc/sys/vm/drop_caches
sync
echo 3 | sudo tee /proc/sys/vm/drop_caches
sync
echo 3 | sudo tee /proc/sys/vm/drop_caches

mount_fs=$1
del_thrds=$2
write_dele_size=$3

parfs_branch=(idel nvodin parfs-no-opt-append append_csum_whole_block append_csum_partial_block append_no_csum all_scan_recovery latest_trans_scan_recovery ckpt no_scan nvodin-kubuf low-thread idel-low-thread optfs)

if [[ "$mount_fs" == "cknova" ]]; then
    mount_fs=nova
fi

if [[ " ${parfs_branch[@]} " =~ " ${mount_fs} " ]]; then
    mount_fs=parfs
fi

# if del_thrds is not null, pass it to the mount script
if [ -z "$del_thrds" ]; then
    $MOUNT_SCRIPT_PATH/mount-$mount_fs.sh
else
    $MOUNT_SCRIPT_PATH/mount-$mount_fs.sh $del_thrds $write_dele_size
fi
