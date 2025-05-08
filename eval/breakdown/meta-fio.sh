#!/usr/bin/bash

set -ex

source "../tools/common.sh"

sudo -v

ABS_PATH=$(where_is_script "$0")
TOOLS_PATH=$ABS_PATH/../tools

FS=("pmfs" "nova" "cknova" "nvodin" "idel" "parfs")
# FS=("parfs" "idel")
# FS=("pmfs")

DELEGATION_FS=("nvodin" "idel" "parfs")

del_thrds=(12)

WORKLOADS=("write" "randwrite")

TOTAL_FILE_SIZE=$((32 * 1024))
# BLK_SIZES=($((4 * 1024)) $((8 * 1024)) $((16 * 1024)) $((32 * 1024)))
BLK_SIZES=($((4 * 1024)))
NUM_JOBS=(32)

TABLE_NAME_PMFS="$ABS_PATH/performance-comparison-table-pmfs"
table_create "$TABLE_NAME_PMFS" "workloads total_time(ns) meta(ns) data(ns)"

TABLE_NAME_NOVA="$ABS_PATH/performance-comparison-table-nova"
table_create "$TABLE_NAME_NOVA" "workloads total_time(ns) meta(ns) data(ns)"

TABLE_NAME_CKNOVA="$ABS_PATH/performance-comparison-table-cknova"
table_create "$TABLE_NAME_CKNOVA" "workloads total_time(ns) meta(ns) data(ns)"

TABLE_NAME_NVODIN="$ABS_PATH/performance-comparison-table-nvodin"
table_create "$TABLE_NAME_NVODIN" "workloads total_time(ns) meta(ns) data(ns)"

TABLE_NAME_IDEL="$ABS_PATH/performance-comparison-table-idel"
table_create "$TABLE_NAME_IDEL" "workloads total_time(ns) meta(ns) data(ns) data_csum(ns) comu(ns)"

TABLE_NAME_PARFS="$ABS_PATH/performance-comparison-table-parfs"
table_create "$TABLE_NAME_PARFS" "workloads total_time(ns) meta(ns) data(ns) data_csum(ns) comu(ns)"

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

    fs=$fs_raw
}

mkdir -p "$ABS_PATH"/M_DATA/fio

for fs in "${FS[@]}"; do
    compile_fs "$fs" "1"
    for bsz in "${BLK_SIZES[@]}"; do
        for job in "${NUM_JOBS[@]}"; do
            for workload in "${WORKLOADS[@]}"; do
                fsize=($(("$TOTAL_FILE_SIZE" / "$job")))

                dmesg -C

                do_fio "$fs" "$workload" "$fsize" "$bsz" "$job" "$del_thrds"

                mkdir -p "$ABS_PATH"/M_DATA/fio/${workload}
                dmesg -c >"$ABS_PATH"/M_DATA/fio/${workload}/${fs}

                sleep 1

                total_time=$(dmesg_attr_time "$ABS_PATH"/M_DATA/fio/${workload}/${fs} "write_total")
                meta_time=$(dmesg_attr_time "$ABS_PATH"/M_DATA/fio/${workload}/${fs} "write_meta")
                data_time=$(dmesg_attr_time "$ABS_PATH"/M_DATA/fio/${workload}/${fs} "write_data")
                data_csum_time=$(dmesg_attr_time "$ABS_PATH"/M_DATA/fio/${workload}/${fs} "write_data_csum")
                comu_time=$(dmesg_attr_time "$ABS_PATH"/M_DATA/fio/${workload}/${fs} "write_comu")

                if [[ "${fs}" == "nova" ]]; then
                    table_add_row "$TABLE_NAME_NOVA" "$workload $total_time $meta_time $data_time"
                elif [[ "${fs}" == "cknova" ]]; then
                    table_add_row "$TABLE_NAME_CKNOVA" "$workload $total_time $meta_time $data_time"
                elif [[ "${fs}" == "pmfs" ]]; then
                    table_add_row "$TABLE_NAME_PMFS" "$workload $total_time $meta_time $data_time"
                elif [[ "${fs}" == "nvodin" ]]; then
                    table_add_row "$TABLE_NAME_NVODIN" "$workload $total_time $meta_time $data_time"
                elif [[ "${fs}" == "idel" ]]; then
                    table_add_row "$TABLE_NAME_IDEL" "$workload $total_time $meta_time $data_time $data_csum_time $comu_time"
                elif [[ "${fs}" == "parfs" ]]; then
                    table_add_row "$TABLE_NAME_PARFS" "$workload $total_time $meta_time $data_time $data_csum_time $comu_time"
                else
                    echo "fs: ${fs}"
                fi

            done
        done
    done
done
