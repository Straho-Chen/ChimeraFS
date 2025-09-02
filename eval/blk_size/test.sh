#!/usr/bin/bash

set -ex

source "../tools/common.sh"

sudo -v

ABS_PATH=$(where_is_script "$0")
TOOLS_PATH=$ABS_PATH/../tools

# FS=("ext4-dax" "ext4-raid" "nova" "pmfs" "winefs")

# DELEGATION_FS=("parfs" "odinfs")
DELEGATION_FS=("parfs")

# in MB
TOTAL_FILE_SIZE=$((1 * 1024))

NUM_JOBS=(1)

# in B
# BLK_SIZES=($((4 * 1024)) $((8 * 1024)) $((16 * 1024)) $((32 * 1024)))
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
    local numa_node=$7

    if [[ "$op" == "write" || "$op" == "randwrite" || "$op" == "overwrite" || "$op" == "randoverwrite" ]]; then
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

    if [[ "$op" == "overwrite" ]]; then
        cmd="$TOOLS_PATH/fio.sh $fpath $bsz $fsize $job write 1"
    elif [[ "$op" == "randoverwrite" ]]; then
        cmd="$TOOLS_PATH/fio.sh $fpath $bsz $fsize $job randwrite 1"
    else
        cmd="$TOOLS_PATH/fio.sh $fpath $bsz $fsize $job $op"
    fi

    if [[ -n "$numa_node" ]]; then
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
                do_fio "$fs" "randwrite" "$fsize" "$bsz" "$job"
                do_fio "$fs" "read" "$fsize" "$bsz" "$job"
                do_fio "$fs" "randread" "$fsize" "$bsz" "$job"

            done
        done
    done
done

# for delegation fs

for fs in "${DELEGATION_FS[@]}"; do
    for del_thrds in "${DEL_THRDS[@]}"; do
        for bsz in "${BLK_SIZES[@]}"; do
            for job in "${NUM_JOBS[@]}"; do
                fsize=($(("$TOTAL_FILE_SIZE" / "$job")))
                for ((i = 1; i <= loop; i++)); do

                    if [[ "$fs" == "parfs" ]]; then
                        compile_fs "$fs" "0"
                        # compile_fs "low-thread" "0"
                        do_fio "$fs" "write" "$fsize" "$bsz" "$job" "$del_thrds" "1"
                        do_fio "$fs" "randwrite" "$fsize" "$bsz" "$job" "$del_thrds" "1"
                        # compile_fs "$fs" "0"
                        do_fio "$fs" "read" "$fsize" "$bsz" "$job" "$del_thrds" "1"
                        do_fio "$fs" "randread" "$fsize" "$bsz" "$job" "$del_thrds" "1"
                    else
                        compile_fs "$fs" "0"
                        do_fio "$fs" "write" "$fsize" "$bsz" "$job" "$del_thrds"
                        do_fio "$fs" "randwrite" "$fsize" "$bsz" "$job" "$del_thrds"
                        do_fio "$fs" "read" "$fsize" "$bsz" "$job" "$del_thrds"
                        do_fio "$fs" "randread" "$fsize" "$bsz" "$job" "$del_thrds"
                    fi

                done
            done
        done
    done
done
