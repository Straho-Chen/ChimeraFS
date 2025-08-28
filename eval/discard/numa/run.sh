#!/bin/bash

sudo -v

source "../tools/common.sh"

ABS_PATH=$(where_is_script "$0")
TOOLS_PATH=$ABS_PATH/../tools

./gen.sh

fs=(ctfs)

#need for pcm
sudo modprobe msr
ulimit -n 1000000

for i in ${fs[@]}; do
    $TOOLS_PATH/mount.sh $i
    ./run-write.sh
    $TOOLS_PATH/umount.sh $i

    $TOOLS_PATH/mount.sh $i
    ./run-read.sh
    $TOOLS_PATH/umount.sh $i
done
