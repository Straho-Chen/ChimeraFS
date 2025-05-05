#!/usr/bin/bash

function where_is_script() {
    local script=$1
    cd "$(dirname "$script")" && pwd
}

ABS_PATH=$(where_is_script "$0")

# mount script path

MOUNT_SCRIPT_PATH=$ABS_PATH/mount-scripts

mount_fs=$1

parfs_branch=(idel nvodin parfs-no-opt-append parfs-no-meta-sep)

if [[ "$mount_fs" == "cknova" ]]; then
    mount_fs=nova
fi

if [[ " ${parfs_branch[@]} " =~ " ${mount_fs} " ]]; then
    mount_fs=parfs
fi

$MOUNT_SCRIPT_PATH/umount-$mount_fs.sh
