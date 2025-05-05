#!/usr/bin/bash

set -ex

source "../tools/common.sh"

sudo -v

ABS_PATH=$(where_is_script "$0")
TOOLS_PATH=$ABS_PATH/../tools

# FS=("nova")

# DELEGATION_FS=("parfs-single-pm" "parfs")
DELEGATION_FS=("parfs")

# in MB
TOTAL_FILE_SIZE=$((32 * 1024))

NUM_JOBS=(1 2 4 8 16 28 32)

# in B
BLK_SIZES=($((32 * 1024)))

DEL_THRDS=(12)

TABLE_NAME="$ABS_PATH/performance-comparison-table"

table_create "$TABLE_NAME" "fs ops filesz blksz numjobs bandwidth(MiB/s)"

loop=1
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
    local del_thrds=$6

    if [[ "$op" == "write" || "$op" == "randwrite" ]]; then
        grep_sign="WRITE:"
    else
        grep_sign="READ:"
    fi

    echo "FIO: $fs $op $fsize $bsz $job" >/dev/kmsg

    fs_raw=$fs

    # if $fs fall in delegation fs
    if [[ "${DELEGATION_FS[@]}" =~ "$fs" ]]; then
        bash "$TOOLS_PATH"/mount.sh "$fs" "$del_thrds"
        fs=$fs-$del_thrds
    else
        bash "$TOOLS_PATH"/mount.sh "$fs"
    fi

    BW=$(bash "$TOOLS_PATH"/fio.sh "$fpath" "$bsz" "$fsize" "$job" "$op" | grep "$grep_sign" | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)

    bash "$TOOLS_PATH"/umount.sh "$fs_raw"

    table_add_row "$TABLE_NAME" "$fs $op $fsize $bsz $job $BW"
    fs=$fs_raw
}

for fs in "${FS[@]}"; do
    compile_fs "$fs" "0"
    for bsz in "${BLK_SIZES[@]}"; do
        for job in "${NUM_JOBS[@]}"; do
            fsize=($(("$TOTAL_FILE_SIZE" / "$job")))
            for ((i = 1; i <= loop; i++)); do

                do_fio "$fs" "write" "$fsize" "$bsz" "$job"
                do_fio "$fs" "randwrite" "$fsize" "$bsz" "$job"

            done
        done
    done
done

for fs in "${DELEGATION_FS[@]}"; do
    if [[ "$fs" == "parfs-single-pm" ]]; then
        compile_fs "parfs" "0"
    else
        compile_fs "$fs" "0"
    fi
    for del_thrds in "${DEL_THRDS[@]}"; do
        for bsz in "${BLK_SIZES[@]}"; do
            for job in "${NUM_JOBS[@]}"; do
                fsize=($(("$TOTAL_FILE_SIZE" / "$job")))
                for ((i = 1; i <= loop; i++)); do

                    do_fio "$fs" "write" "$fsize" "$bsz" "$job" "$del_thrds"
                    do_fio "$fs" "randwrite" "$fsize" "$bsz" "$job" "$del_thrds"

                done
            done
        done
    done
done
