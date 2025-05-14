#!/usr/bin/bash

set -ex

source "../tools/common.sh"

sudo -v

ABS_PATH=$(where_is_script "$0")
TOOLS_PATH=$ABS_PATH/../tools

# FS=("ext4-dax" "ext4-raid" "nova" "pmfs" "winefs")

# DELEGATION_FS=("parfs" "odinfs")
DELEGATION_FS=("parfs")

# UFS=("madfs")

# in MB
TOTAL_FILE_SIZE=$((32 * 1024))

# NUM_JOBS=(1 2 4 8 16 28 32 48 56)
NUM_JOBS=(1 2 4 8 16 28 32)
# NUM_JOBS=(4)

# in B
BLK_SIZES=($((4 * 1024)) $((8 * 1024)) $((16 * 1024)) $((32 * 1024)))
# BLK_SIZES=($((4 * 1024)))
# BLK_SIZES=($((8 * 1024)))
# BLK_SIZES=($((16 * 1024)))
# BLK_SIZES=($((32 * 1024)))

# DEL_THRDS=(1 2 4 8 12)
DEL_THRDS=(12)
# DEL_THRDS=(1)

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
    local numa_node=$7

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

    cmd=""$TOOLS_PATH"/fio.sh "$fpath" "$bsz" "$fsize" "$job" "$op""

    if [[ "$numa_node" ]]; then
        BW=$(numactl -N "$numa_node" $cmd | grep "$grep_sign" | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)
    else
        BW=$(bash $cmd | grep "$grep_sign" | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)
    fi

    bash "$TOOLS_PATH"/umount.sh "$fs_raw"

    table_add_row "$TABLE_NAME" "$fs $op $fsize $bsz $job $BW"
    fs=$fs_raw
}

# for normal fs

for fs in "${FS[@]}"; do
    compile_fs "$fs" "0"
    for bsz in "${BLK_SIZES[@]}"; do
        for job in "${NUM_JOBS[@]}"; do
            fsize=($(("$TOTAL_FILE_SIZE" / "$job")))
            for ((i = 1; i <= loop; i++)); do

                do_fio "$fs" "write" "$fsize" "$bsz" "$job"
                do_fio "$fs" "read" "$fsize" "$bsz" "$job"
                do_fio "$fs" "randwrite" "$fsize" "$bsz" "$job"
                do_fio "$fs" "randread" "$fsize" "$bsz" "$job"

            done
        done
    done
done

# for delegation fs

for job in "${NUM_JOBS[@]}"; do
    for bsz in "${BLK_SIZES[@]}"; do
        for fs in "${DELEGATION_FS[@]}"; do

            if [[ "$job" -le 4 ]] && [[ "$bsz" -le 16384 ]]; then
                if [[ "$fs" == "idel" ]]; then
                    compile_fs "idel-low-thread" "0"
                elif [[ "$fs" == "parfs" ]]; then
                    compile_fs "low-thread" "0"
                else
                    compile_fs "$fs" "0"
                fi
            else
                compile_fs "$fs" "0"
            fi

            for del_thrds in "${DEL_THRDS[@]}"; do
                fsize=($(("$TOTAL_FILE_SIZE" / "$job")))
                for ((i = 1; i <= loop; i++)); do

                    if [[ "$job" -le 4 ]] && [[ "$fs" == "parfs" ]] && [[ "$bsz" -le 16384 ]]; then
                        do_fio "$fs" "write" "$fsize" "$bsz" "$job" "$del_thrds" "1"
                        do_fio "$fs" "randwrite" "$fsize" "$bsz" "$job" "$del_thrds" "1"
                    else
                        do_fio "$fs" "write" "$fsize" "$bsz" "$job" "$del_thrds"
                        do_fio "$fs" "randwrite" "$fsize" "$bsz" "$job" "$del_thrds"
                    fi

                done
            done
        done
    done
done

for fs in "${DELEGATION_FS[@]}"; do
    # if [[ "$fs" == "idel" ]]; then
    #     continue
    # fi
    compile_fs "$fs" "0"
    for del_thrds in "${DEL_THRDS[@]}"; do
        for bsz in "${BLK_SIZES[@]}"; do
            for job in "${NUM_JOBS[@]}"; do
                fsize=($(("$TOTAL_FILE_SIZE" / "$job")))
                for ((i = 1; i <= loop; i++)); do

                    do_fio "$fs" "read" "$fsize" "$bsz" "$job" "$del_thrds"
                    do_fio "$fs" "randread" "$fsize" "$bsz" "$job" "$del_thrds"

                done
            done
        done
    done
done

do_fio_ufs() {
    local fs=$1
    local op=$2
    local fsize=$3
    local bsz=$4
    local job=$5
    local ulib_path=$6

    if [[ "$op" == "write" || "$op" == "randwrite" ]]; then
        grep_sign="WRITE:"
    else
        grep_sign="READ:"
    fi

    echo "FIO: $fs $op $fsize $bsz $job" >/dev/kmsg

    bash "$TOOLS_PATH"/mount.sh "ext4-dax"

    BW=$(bash LD_PRELOAD=$ulib_path "$TOOLS_PATH"/fio.sh "$fpath" "$bsz" "$fsize" "$job" "$op" | grep "$grep_sign" | awk '{print $2}' | sed 's/bw=//g' | "$TOOLS_PATH"/converter/to_MiB_s)

    bash "$TOOLS_PATH"/umount.sh "ext4-dax"

    table_add_row "$TABLE_NAME" "$fs $op $fsize $bsz $job $BW"
}

for fs in "${UFS[@]}"; do
    ulib_path=$(ufs_lib_path "$fs")
    for bsz in "${BLK_SIZES[@]}"; do
        for job in "${NUM_JOBS[@]}"; do
            fsize=($(("$TOTAL_FILE_SIZE" / "$job")))
            for ((i = 1; i <= loop; i++)); do

                do_fio_ufs "$fs" "write" "$fsize" "$bsz" "$job" "$ulib_path"
                do_fio_ufs "$fs" "read" "$fsize" "$bsz" "$job" "$ulib_path"
                do_fio_ufs "$fs" "randwrite" "$fsize" "$bsz" "$job" "$ulib_path"
                do_fio_ufs "$fs" "randread" "$fsize" "$bsz" "$job" "$ulib_path"

            done
        done
    done
done
