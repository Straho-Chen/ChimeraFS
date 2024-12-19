#!/usr/bin/bash

set -ex

source "../tools/common.sh"

ABS_PATH=$(where_is_script "$0")
TOOLS_PATH=$ABS_PATH/../tools
DEVICE_PATH=$1
MOUNT_PATH=$2

echo "DEVICE_PATH: $DEVICE_PATH"
echo "MOUNT_PATH: $MOUNT_PATH"

FILE_SYSTEMS=("EXT4-DAX")
FILE_SIZES=($((2 * 1024)))
NUM_JOBS=(8)
BLK_SIZES=($((4 * 1024)))

IOENGINE=("sync" "libpmem")

TABLE_NAME="$ABS_PATH/performance-comparison-table-$DEVICE_PATH-$MOUNT_PATH"

table_create "$TABLE_NAME" "file_system ops ioengine filesz blksz numjobs bandwidth(MiB/s)"

loop=1
if [ "$1" ]; then
    loop=$1
fi

for ((i = 1; i <= loop; i++)); do
    for fsize in "${FILE_SIZES[@]}"; do
        for bsz in "${BLK_SIZES[@]}"; do
            for job in "${NUM_JOBS[@]}"; do

                if ((job == 1)); then
                    fpath="$MOUNT_PATH"test
                else
                    fpath="$MOUNT_PATH"
                fi

                for ioengine in "${IOENGINE[@]}"; do

                    for file_system in "${FILE_SYSTEMS[@]}"; do

                        bash "$TOOLS_PATH"/mount.sh "$file_system" "main" "$MOUNT_PATH" "$DEVICE_PATH"

                        BW=$(bash "$TOOLS_PATH"/fio.sh "$fpath" "$bsz" "$fsize" "$job" "write" "$ioengine" | grep WRITE: | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)
                        table_add_row "$TABLE_NAME" "$file_system seq-write $ioengine $fsize $bsz $job $BW"

                        bash "$TOOLS_PATH"/mount.sh "$file_system" "main" "$MOUNT_PATH" "$DEVICE_PATH"

                        BW=$(bash "$TOOLS_PATH"/fio.sh $fpath "$bsz" "$fsize" "$job" "randwrite" "$ioengine" | grep WRITE: | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)
                        table_add_row "$TABLE_NAME" "$file_system rnd-write $ioengine $fsize $bsz $job $BW"

                        bash "$TOOLS_PATH"/mount.sh "$file_system" "main" "$MOUNT_PATH" "$DEVICE_PATH"

                        BW=$(bash "$TOOLS_PATH"/fio.sh "$fpath" "$bsz" "$fsize" "$job" "read" "$ioengine" | grep READ: | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)
                        table_add_row "$TABLE_NAME" "$file_system seq-read $ioengine $fsize $bsz $job $BW"

                        bash "$TOOLS_PATH"/mount.sh "$file_system" "main" "$MOUNT_PATH" "$DEVICE_PATH"

                        BW=$(bash "$TOOLS_PATH"/fio.sh "$fpath" "$bsz" "$fsize" "$job" "randread" "$ioengine" | grep READ: | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)
                        table_add_row "$TABLE_NAME" "$file_system rnd-read $ioengine $fsize $bsz $job $BW"
                    done
                done
            done
        done
    done
done
