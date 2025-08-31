#!/usr/bin/bash

set -ex

source "../tools/common.sh"

sudo -v

echo 0 >/proc/sys/kernel/randomize_va_space

ABS_PATH=$(where_is_script "$0")
TOOLS_PATH=$ABS_PATH/../tools

FSCRIPT_PRE_FIX=$TOOLS_PATH/fbscripts
FB_PATH=$ABS_PATH/../benchmark/bin/filebench/bin

DELEGATION_FS=("all_scan_recovery" "latest_trans_scan_recovery" "ckpt" "no_scan")
# DELEGATION_FS=("parfs")

del_thrds=(12)

FB_WORKLOADS=("fileserver.f")
FIO_WORKLOADS=("write")

TOTAL_FILE_SIZE=$((32 * 1024))
# BLK_SIZES=($((4 * 1024)) $((8 * 1024)) $((16 * 1024)) $((32 * 1024)))
BLK_SIZES=($((4 * 1024)))
NUM_JOBS=(32)

TABLE_NAME="$ABS_PATH/performance-comparison-table"
table_create "$TABLE_NAME" "fs workload recovery_time(ns)"

fpath="/mnt/pmem0/"

loop=3
if [ "$1" ]; then
    loop=$1
fi

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

    fs=$fs_raw
}

do_fb() {
    local fs=$1
    local fbench=$2
    local thread=$3
    local del_thrds=$4

    echo "filebench: $fs $fbench $thread" >/dev/kmsg

    fs_raw=$fs

    # if $fs fall in delegation fs
    if [[ "${DELEGATION_FS[@]}" =~ "$fs" ]]; then
        bash "$TOOLS_PATH"/mount.sh "$fs" "$del_thrds"
        fs=$fs-$del_thrds
    else
        bash "$TOOLS_PATH"/mount.sh "$fs"
    fi

    cp -f "$FSCRIPT_PRE_FIX"/"$fbench" "$ABS_PATH"/DATA/"$fbench"/"$thread"
    sed_cmd='s#set \$nthreads=.*#set $nthreads='$thread'#g'
    sed -i "$sed_cmd" "$ABS_PATH"/DATA/"$fbench"/"$thread"

    sed_cmd='s#set \$dir=.*#set $dir='$fpath'#g'
    sed -i "$sed_cmd" "$ABS_PATH"/DATA/"$fbench"/"$thread"

    "$FB_PATH"/filebench -f "$ABS_PATH"/DATA/"$fbench"/"$thread" | tee "$ABS_PATH"/DATA/"$fbench"/"$fs"-"$thread"

    bash "$TOOLS_PATH"/umount.sh "$fs_raw"

    fs=$fs_raw
}

for ((i = 1; i <= loop; i++)); do
    for fs in "${DELEGATION_FS[@]}"; do
        compile_fs "$fs" "0"
        for bsz in "${BLK_SIZES[@]}"; do
            for job in "${NUM_JOBS[@]}"; do
                for workload in "${FIO_WORKLOADS[@]}"; do
                    fsize=($(("$TOTAL_FILE_SIZE" / "$job")))

                    do_fio "$fs" "$workload" "$fsize" "$bsz" "$job" "$del_thrds"

                    dmesg -C

                    $ABS_PATH/remount.sh

                    dmesg -c >"$ABS_PATH"/recovery.log
                    # time=$( (time bash "$ABS_PATH"/remount.sh) 2>&1 | grep real | awk '{print $2}' )
                    time=$(dmesg_attr_time "$ABS_PATH"/recovery.log "recovery_time")

                    table_add_row "$TABLE_NAME" "$fs fio $time"

                    bash "$TOOLS_PATH"/umount.sh "$fs"

                done
            done
        done
    done
done

for ((i = 1; i <= loop; i++)); do
    for fs in "${DELEGATION_FS[@]}"; do
        compile_fs "$fs" "0"
        for bsz in "${BLK_SIZES[@]}"; do
            for job in "${NUM_JOBS[@]}"; do
                for workload in "${FB_WORKLOADS[@]}"; do
                    mkdir -p "$ABS_PATH"/DATA/"$workload"

                    do_fb "$fs" "$workload" "$job" "$del_thrds"

                    dmesg -C

                    $ABS_PATH/remount.sh

                    dmesg -c >"$ABS_PATH"/recovery.log
                    # time=$( (time bash "$ABS_PATH"/remount.sh) 2>&1 | grep real | awk '{print $2}' )
                    time=$(dmesg_attr_time "$ABS_PATH"/recovery.log "recovery_time")

                    table_add_row "$TABLE_NAME" "$fs filebench $time"

                    bash "$TOOLS_PATH"/umount.sh "$fs"
                done
            done
        done
    done
done
