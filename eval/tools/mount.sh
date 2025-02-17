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

fs=$1
del_thrds=$2

# if del_thrds is not null, pass it to the mount script
if [ -z "$del_thrds" ]; then
    $MOUNT_SCRIPT_PATH/mount-$fs.sh
else
    $MOUNT_SCRIPT_PATH/mount-$fs.sh $del_thrds
fi
