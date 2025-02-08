#!/usr/bin/bash

set -ex

source "../tools/common.sh"

ABS_PATH=$(where_is_script "$0")
TOOLS_PATH=$ABS_PATH/../tools

READ_LOG_DIR=$ABS_PATH/read-log
WRITE_LOG_DIR=$ABS_PATH/write-log

LOG_PREFIX="fio-output"

FILE_SYSTEMS=("EXT4-DAX")

NUM_JOBS=(1 2 4 8 16)

TABLE_NAME="$ABS_PATH/perf"

table_create "$TABLE_NAME" "fs ops filesz blksz numjobs bandwidth(MiB/s)"

do_fio() {
    local fs=$1
    local op=$2
    local job=$3

    if [[ "$op" == "write" || "$op" == "randwrite" ]]; then
        grep_sign="WRITE:"
    else
        grep_sign="READ:"
    fi

    drop_cache

    ./run-fio.sh $op numa0 $job $READ_LOG_DIR $LOG_PREFIX &
    ./run-fio.sh $op numa1 $job $READ_LOG_DIR $LOG_PREFIX &

    if wait; then
        echo "All fio tests are done"
    else
        echo "Some fio tests failed"
    fi

    # resolve output
    numa0_bw=$(cat $READ_LOG_DIR/$LOG_PREFIX-numa0.log | grep "$grep_sign" | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)
    numa1_bw=$(cat $READ_LOG_DIR/$LOG_PREFIX-numa1.log | grep "$grep_sign" | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)

    BW=$(echo "$numa0_bw + $numa1_bw" | bc)

    table_add_row "$TABLE_NAME" "$fs $op 1024 2048 $job $BW"
}

for file_system in "${FILE_SYSTEMS[@]}"; do

    # mount pmem0 and pmem1
    sudo ndctl disable-namespace namespace0.0
    sudo ndctl destroy-namespace namespace0.0 --force
    sudo ndctl create-namespace -m fsdax
    sudo mkfs.ext4 -F /dev/pmem0
    sudo mount -o dax /dev/pmem0 /mnt/pmem0/
    sudo chown $USER /mnt/pmem0/

    sudo ndctl disable-namespace namespace1.0
    sudo ndctl destroy-namespace namespace1.0 --force
    sudo ndctl create-namespace -m fsdax
    sudo mkfs.ext4 -F /dev/pmem1
    sudo mount -o dax /dev/pmem1 /mnt/pmem1/
    sudo chown $USER /mnt/pmem1/

    # init numa info
    python3 "$ABS_PATH"/numa-info.py

    # init fio test
    ./run-fio.sh read numa0 16 $READ_LOG_DIR $LOG_PREFIX &
    ./run-fio.sh read numa1 16 $READ_LOG_DIR $LOG_PREFIX &

    if wait; then
        echo "All fio preparation are done"
    else
        echo "Some fio preparation failed"
    fi

    for numjobs in "${NUM_JOBS[@]}"; do
        # call fio test on dual socket
        # both read
        do_fio "$file_system" "read" $numjobs

        # both randread
        do_fio "$file_system" "randread" $numjobs

        # both write
        do_fio "$file_system" "write" $numjobs

        # both randwrite
        do_fio "$file_system" "randwrite" $numjobs

    done

    umount /mnt/pmem0
    umount /mnt/pmem1

done
