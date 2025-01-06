#!/usr/bin/bash

set -ex

source "../tools/common.sh"

ABS_PATH=$(where_is_script "$0")
TOOLS_PATH=$ABS_PATH/../tools

READ_LOG_DIR=$ABS_PATH/read-log
WRITE_LOG_DIR=$ABS_PATH/write-log

LOG_PREFIX="fio-output"

FILE_SYSTEMS=("EXT4-DAX")

TABLE_NAME="$ABS_PATH/perf"

for file_system in "${FILE_SYSTEMS[@]}"; do

    table_create "$TABLE_NAME" "file_system test_mode numa_node bandwidth(MiB/s)"

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
    ./run-fio.sh read numa0 $READ_LOG_DIR $LOG_PREFIX &
    ./run-fio.sh read numa1 $READ_LOG_DIR $LOG_PREFIX &

    if wait; then
        echo "All fio preparation are done"
    else
        echo "Some fio preparation failed"
    fi

    # call fio test on dual socket
    # both read
    drop_cache

    ./run-fio.sh read numa0 $READ_LOG_DIR $LOG_PREFIX &
    ./run-fio.sh read numa1 $READ_LOG_DIR $LOG_PREFIX &

    if wait; then
        echo "All fio tests are done"
    else
        echo "Some fio tests failed"
    fi

    # resolve output
    numa0_bw=$(cat $READ_LOG_DIR/$LOG_PREFIX-numa0.log | grep READ: | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)
    numa1_bw=$(cat $READ_LOG_DIR/$LOG_PREFIX-numa1.log | grep READ: | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)

    table_add_row "$TABLE_NAME" "$file_system all-read 0 $numa0_bw"
    table_add_row "$TABLE_NAME" "$file_system all-read 1 $numa1_bw"
    table_add_row "$TABLE_NAME" "$file_system all-read total $(echo "$numa0_bw + $numa1_bw" | bc)"

    # both randread
    drop_cache

    ./run-fio.sh randread numa0 $READ_LOG_DIR $LOG_PREFIX &
    ./run-fio.sh randread numa1 $READ_LOG_DIR $LOG_PREFIX &

    if wait; then
        echo "All fio tests are done"
    else
        echo "Some fio tests failed"
    fi

    # resolve output
    numa0_bw=$(cat $READ_LOG_DIR/$LOG_PREFIX-numa0.log | grep READ: | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)
    numa1_bw=$(cat $READ_LOG_DIR/$LOG_PREFIX-numa1.log | grep READ: | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)

    table_add_row "$TABLE_NAME" "$file_system all-randread 0 $numa0_bw"
    table_add_row "$TABLE_NAME" "$file_system all-randread 1 $numa1_bw"
    table_add_row "$TABLE_NAME" "$file_system all-randread total $(echo "$numa0_bw + $numa1_bw" | bc)"

    # both write
    drop_cache
    ./run-fio.sh write numa0 $WRITE_LOG_DIR $LOG_PREFIX &
    ./run-fio.sh write numa1 $WRITE_LOG_DIR $LOG_PREFIX &

    if wait; then
        echo "All fio tests are done"
    else
        echo "Some fio tests failed"
    fi

    # resolve output
    numa0_bw=$(cat $WRITE_LOG_DIR/$LOG_PREFIX-numa0.log | grep WRITE: | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)
    numa1_bw=$(cat $WRITE_LOG_DIR/$LOG_PREFIX-numa1.log | grep WRITE: | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)

    table_add_row "$TABLE_NAME" "$file_system all-write 0 $numa0_bw"
    table_add_row "$TABLE_NAME" "$file_system all-write 1 $numa1_bw"
    table_add_row "$TABLE_NAME" "$file_system all-write total $(echo "$numa0_bw + $numa1_bw" | bc)"

    # both randwrite
    drop_cache
    ./run-fio.sh randwrite numa0 $WRITE_LOG_DIR $LOG_PREFIX &
    ./run-fio.sh randwrite numa1 $WRITE_LOG_DIR $LOG_PREFIX &

    if wait; then
        echo "All fio tests are done"
    else
        echo "Some fio tests failed"
    fi

    # resolve output
    numa0_bw=$(cat $WRITE_LOG_DIR/$LOG_PREFIX-numa0.log | grep WRITE: | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)
    numa1_bw=$(cat $WRITE_LOG_DIR/$LOG_PREFIX-numa1.log | grep WRITE: | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)

    table_add_row "$TABLE_NAME" "$file_system all-randwrite 0 $numa0_bw"
    table_add_row "$TABLE_NAME" "$file_system all-randwrite 1 $numa1_bw"
    table_add_row "$TABLE_NAME" "$file_system all-randwrite total $(echo "$numa0_bw + $numa1_bw" | bc)"

    umount /mnt/pmem0
    umount /mnt/pmem1

done
