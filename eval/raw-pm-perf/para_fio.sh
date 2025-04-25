#!/usr/bin/bash

set -ex

source "../tools/common.sh"

ABS_PATH=$(where_is_script "$0")
TOOLS_PATH=$ABS_PATH/../tools

READ_LOG_DIR=$ABS_PATH/read-log
WRITE_LOG_DIR=$ABS_PATH/write-log

LOG_PREFIX="fio-output"

NUM_JOBS=(1 2 4 8 16 28 32 48)

BLK_SIZES=($((4 * 1024)) $((8 * 1024)) $((16 * 1024)) $((32 * 1024)))
# BLK_SIZES=($((4 * 1024)) $((32 * 1024)) $((2 * 1024 * 1024)))

FILE_SIZES=($((2 * 1024)))

max_jobs=${NUM_JOBS[0]}

# 遍历数组
for num in "${NUM_JOBS[@]}"; do
    if ((num > $max_jobs)); then
        max_jobs=$num
    fi
done

echo "max jobs: $max_jobs"

TABLE_NAME="$ABS_PATH/perf"

table_create "$TABLE_NAME" "fs ops filesz blksz numjobs bandwidth(MiB/s)"

do_fio() {
    local op=$1
    local bsize=$2
    local fsize=$3
    local job=$4

    if [[ "$op" == "write" || "$op" == "randwrite" ]]; then
        grep_sign="WRITE:"
    else
        grep_sign="READ:"
    fi

    drop_cache

    numactl -N 0 ./run-fio.sh $op numa0 $bsize $fsize $job $READ_LOG_DIR $LOG_PREFIX &
    numactl -N 1 ./run-fio.sh $op numa1 $bsize $fsize $job $READ_LOG_DIR $LOG_PREFIX &

    if wait; then
        echo "All fio tests are done"
    else
        echo "Some fio tests failed"
    fi

    # resolve output
    numa0_bw=$(cat $READ_LOG_DIR/$LOG_PREFIX-numa0.log | grep "$grep_sign" | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)
    numa1_bw=$(cat $READ_LOG_DIR/$LOG_PREFIX-numa1.log | grep "$grep_sign" | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)

    BW=$(echo "$numa0_bw + $numa1_bw" | bc)

    table_add_row "$TABLE_NAME" "raw $op $fsize $bsize $job $BW"
}

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

for fsize in "${FILE_SIZES[@]}"; do
    for bsize in "${BLK_SIZES[@]}"; do

        # init fio test
        numactl -N 0 ./run-fio.sh read numa0 $bsize $fsize $max_jobs $READ_LOG_DIR $LOG_PREFIX &
        numactl -N 1 ./run-fio.sh read numa1 $bsize $fsize $max_jobs $READ_LOG_DIR $LOG_PREFIX &

        if wait; then
            echo "All fio preparation are done"
        else
            echo "Some fio preparation failed"
        fi

        for numjobs in "${NUM_JOBS[@]}"; do
            # call fio test on dual socket
            # both write
            do_fio "write" $bsize $fsize $numjobs
            # both randwrite
            do_fio "randwrite" $bsize $fsize $numjobs
            # both read
            do_fio "read" $bsize $fsize $numjobs
            # both randread
            do_fio "randread" $bsize $fsize $numjobs
        done
    done
done

umount /mnt/pmem0
umount /mnt/pmem1
