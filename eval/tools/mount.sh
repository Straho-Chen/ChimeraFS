#!/usr/bin/bash

function where_is_script() {
    local script=$1
    cd "$(dirname "$script")" && pwd
}

ABS_PATH=$(where_is_script "$0")

# mount script path

MOUNT_SCRIPT_PATH=$ABS_PATH/mount-script

# free memory at first
echo "Drop caches..."
sync
echo 1 | sudo tee /proc/sys/vm/drop_caches
sync
echo 1 | sudo tee /proc/sys/vm/drop_caches
sync
echo 1 | sudo tee /proc/sys/vm/drop_caches

fs=$1
# for odinfs
del_thrds=$2

# if fs is odinfs, we need to specify the number of deletion threads
if [ "$fs" == odinfs* ]; then
    # check del_thrds not null
    if [ -z "$del_thrds" ]; then
        echo "Please specify the number of deletion threads for odinfs"
        exit 1
    fi
    $MOUNT_SCRIPT_PATH/mount-$fs.sh $del_thrds
else
    $MOUNT_SCRIPT_PATH/mount-$fs.sh
fi
