#!/usr/bin/bash

set -ex

source "../tools/common.sh"

sudo -v

ABS_PATH=$(where_is_script "$0")
TOOLS_PATH=$ABS_PATH/../tools

FS=("ext4" "ext4-dax" "ext4-raid" "nova" "pmfs" "winefs")
ODINFS=("odinfs" "odinfs-single-pm")
FILE_SIZES=($((1 * 1024)))
NUM_JOBS=(1 2 4 8 16 28 32 48 56 64)
BLK_SIZES=($((4 * 1024)))

ODINFS_DEL_THRDS=(1 2 4 8 12)

TABLE_NAME="$ABS_PATH/performance-comparison-table"

table_create "$TABLE_NAME" "fs ops filesz blksz numjobs bandwidth(MiB/s)"

loop=3
if [ "$1" ]; then
    loop=$1
fi

fpath="/mnt/pmem0/"

do_fio() {
    local fs=$1
    local op=$2
    local fsize=$3
    local bsz=$4
    local job=$5
    local del_thrd=$6

    if [[ "$op" == "write" || "$op" == "randwrite" ]]; then
        grep_sign="WRITE:"
    else
        grep_sign="READ:"
    fi

    if [[ "$fs" == odinfs* ]]; then
        bash "$TOOLS_PATH"/mount.sh "$fs" "$del_thrd"
    else
        bash "$TOOLS_PATH"/mount.sh "$fs"
    fi

    BW=$(bash "$TOOLS_PATH"/fio.sh "$fpath" "$bsz" "$fsize" "$job" "$op" | grep "$grep_sign" | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)

    bash "$TOOLS_PATH"/umount.sh "$fs"
    clean_mnt_dir "$fpath"

    table_add_row "$TABLE_NAME" "$fs-$del_thrd $op $fsize $bsz $job $BW"
}

# for normal fs

for ((i = 1; i <= loop; i++)); do
    for fs in "${FS[@]}"; do
        for fsize in "${FILE_SIZES[@]}"; do
            for bsz in "${BLK_SIZES[@]}"; do
                for job in "${NUM_JOBS[@]}"; do

                    do_fio "$fs" "write" "$fsize" "$bsz" "$job"
                    do_fio "$fs" "read" "$fsize" "$bsz" "$job"
                    do_fio "$fs" "randwrite" "$fsize" "$bsz" "$job"
                    do_fio "$fs" "randread" "$fsize" "$bsz" "$job"

                done
            done
        done
    done
done

# for odinfs

for ((i = 1; i <= loop; i++)); do
    for fs in "${ODINFS[@]}"; do
        for del_thrd in "${ODINFS_DEL_THRDS[@]}"; do
            for fsize in "${FILE_SIZES[@]}"; do
                for bsz in "${BLK_SIZES[@]}"; do
                    for job in "${NUM_JOBS[@]}"; do

                        do_fio "$fs" "write" "$fsize" "$bsz" "$job" "$del_thrd"
                        do_fio "$fs" "read" "$fsize" "$bsz" "$job" "$del_thrd"
                        do_fio "$fs" "randwrite" "$fsize" "$bsz" "$job" "$del_thrd"
                        do_fio "$fs" "randread" "$fsize" "$bsz" "$job" "$del_thrd"

                    done
                done
            done
        done
    done
done
