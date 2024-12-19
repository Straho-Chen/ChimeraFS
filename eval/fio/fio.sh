#!/usr/bin/bash

source "../tools/common.sh"

sudo -v

ABS_PATH=$(where_is_script "$0")
TOOLS_PATH=$ABS_PATH/../tools

FS=("ext4" "ext4-dax" "nova" "pmfs" "winefs" "odinfs" "odinfs-single-pm")
FILE_SIZES=($((2 * 1024)))
# NUM_JOBS=(1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16)
NUM_JOBS=(1)
BLK_SIZES=($((4 * 1024)))

ODINFS_DEL_THRDS=1

TABLE_NAME="$ABS_PATH/performance-comparison-table"

table_create "$TABLE_NAME" "fs ops filesz blksz numjobs bandwidth(MiB/s)"

loop=1
if [ "$1" ]; then
    loop=$1
fi

fpath="/mnt/pmem0/"

for ((i = 1; i <= loop; i++)); do
    for fsize in "${FILE_SIZES[@]}"; do
        for bsz in "${BLK_SIZES[@]}"; do
            for job in "${NUM_JOBS[@]}"; do
                for fs in "${FS[@]}"; do

                    if [ "$fs" == odinfs* ]; then
                        bash "$TOOLS_PATH"/mount.sh "$fs" "$ODINFS_DEL_THRDS"
                    else
                        bash "$TOOLS_PATH"/mount.sh "$fs"
                    fi

                    clean_mnt_dir "$fpath"

                    BW=$(bash "$TOOLS_PATH"/fio.sh "$fpath" "$bsz" "$fsize" "$job" "write" | grep WRITE: | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)
                    table_add_row "$TABLE_NAME" "$fs seq-write $fsize $bsz $job $BW"

                    clean_mnt_dir "$fpath"

                    BW=$(bash "$TOOLS_PATH"/fio.sh $fpath "$bsz" "$fsize" "$job" "randwrite" | grep WRITE: | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)
                    table_add_row "$TABLE_NAME" "$fs rnd-write $fsize $bsz $job $BW"

                    clean_mnt_dir "$fpath"

                    BW=$(bash "$TOOLS_PATH"/fio.sh "$fpath" "$bsz" "$fsize" "$job" "read" | grep READ: | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)
                    table_add_row "$TABLE_NAME" "$fs seq-read $fsize $bsz $job $BW"

                    clean_mnt_dir "$fpath"

                    BW=$(bash "$TOOLS_PATH"/fio.sh "$fpath" "$bsz" "$fsize" "$job" "randread" | grep READ: | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)
                    table_add_row "$TABLE_NAME" "$fs rnd-read $fsize $bsz $job $BW"

                    bash "$TOOLS_PATH"/umount.sh "$fs"
                done
            done
        done
    done
done
