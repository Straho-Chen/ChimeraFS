#!/usr/bin/bash

function where_is_script() {
    local script=$1
    cd "$(dirname "$script")" && pwd
}

ABS_PATH=$(where_is_script "$0")

# mount script path

MOUNT_SCRIPT_PATH=$ABS_PATH/mount-scripts

mount_fs=$1

parfs_branch=(idel parfs-no-opt-append append_csum_whole_block append_csum_partial_block append_no_csum all_scan_recovery latest_trans_scan_recovery ckpt no_scan low-thread idel-low-thread optfs)

if [[ "$mount_fs" == "cknova" ]]; then
    mount_fs=nova
fi

if [[ " ${parfs_branch[@]} " =~ " ${mount_fs} " ]]; then
    mount_fs=parfs
fi

$MOUNT_SCRIPT_PATH/umount-$mount_fs.sh
