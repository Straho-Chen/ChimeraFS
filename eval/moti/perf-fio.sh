#!/usr/bin/bash

set -ex

source "../tools/common.sh"

sudo -v

ABS_PATH=$(where_is_script "$0")
TOOLS_PATH=$ABS_PATH/../tools

# FS=("pmfs" "nova" "cknova" "idel" "odinfs")
FS=("idel" "parfs")
# FS=("odinfs")

DELEGATION_FS=("nvodin" "nvodin-kubuf" "idel" "odinfs" "parfs")

del_thrds=(12)

WORKLOADS=("write" "randwrite")

TOTAL_FILE_SIZE=$((32 * 1024))
BLK_SIZES=($((4 * 1024)) $((32 * 1024)))
NUM_JOBS=(1 2 4 8 16 28 32)
# NUM_JOBS=(4)

TABLE_NAME="$ABS_PATH/performance-comparison-table"

table_create "$TABLE_NAME" "fs ops filesz blksz numjobs bandwidth(MiB/s)"

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

for job in "${NUM_JOBS[@]}"; do
    for bsz in "${BLK_SIZES[@]}"; do
        for fs in "${FS[@]}"; do
            if [[ "$job" -le 4 ]]; then
                if [[ "$fs" == "idel" ]]; then
                    compile_fs "idel-low-thread" "0"
                elif [[ "$fs" == "parfs" ]]; then
                    compile_fs "low-thread" "0"
                else
                    compile_fs "$fs" "0"
                fi

                if [[ "$job" == 4 ]] && [[ "$bsz" == 32768 ]]; then
                    compile_fs "$fs" "0"
                fi
            else
                compile_fs "$fs" "0"
            fi
            for workload in "${WORKLOADS[@]}"; do
                fsize=($(("$TOTAL_FILE_SIZE" / "$job")))

                if [[ "$job" -le 4 ]]; then

                    if [[ "$job" == 4 ]] && [[ "$bsz" == 32768 ]] && [[ "$fs" == "idel" ]]; then
                        do_fio "$fs" "$workload" "$fsize" "$bsz" "$job" "$del_thrds"
                    elif [[ "$job" == 4 ]] && [[ "$bsz" == 32768 ]] && [[ "$fs" == "parfs" ]]; then
                        do_fio "$fs" "$workload" "$fsize" "$bsz" "$job" "$del_thrds"
                    elif [[ "$fs" == "idel" ]] || [[ "$fs" == "parfs" ]]; then
                        do_fio "$fs" "$workload" "$fsize" "$bsz" "$job" "$del_thrds" "1"
                    else
                        do_fio "$fs" "$workload" "$fsize" "$bsz" "$job" "$del_thrds"
                    fi

                else
                    do_fio "$fs" "$workload" "$fsize" "$bsz" "$job" "$del_thrds"
                fi

            done
        done
    done
done
