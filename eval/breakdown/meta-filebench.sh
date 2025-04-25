#!/usr/bin/bash

set -ex

source "../tools/common.sh"

sudo -v

echo 0 >/proc/sys/kernel/randomize_va_space

ABS_PATH=$(where_is_script "$0")
TOOLS_PATH=$ABS_PATH/../tools

FSCRIPT_PRE_FIX=$TOOLS_PATH/fbscripts
FB_PATH=$ABS_PATH/../benchmark/bin/filebench/bin

# FS=("nova" "odinfs" "parfs" "nvodin")
FS=("nova" "parfs" "nvodin")

DELEGATION_FS=("odinfs" "parfs" "nvodin")

del_thrds=(12)

FILE_BENCHES=("fileserver.f" "varmail.f" "webserver.f" "webproxy.f")

THREADS=(1)

TABLE_NAME_NOVA="$ABS_PATH/performance-comparison-table-nova"

TABLE_NAME_ODINFS="$ABS_PATH/performance-comparison-table-odinfs"

TABLE_NAME_PARFS="$ABS_PATH/performance-comparison-table-parfs"

TABLE_NAME_NVODIN="$ABS_PATH/performance-comparison-table-nvodin"

fpath="/mnt/pmem0/"

do_fb() {
    local fs=$1
    local fbench=$2
    local thread=$3
    local del_thrds=$4

    echo "filebench: $fs $fbench $thread" >/dev/kmsg

    fs_raw=$fs

    # if $fs fall in delegation fs
    if [[ "${DELEGATION_FS[@]}" =~ "$fs" ]]; then
        bash "$TOOLS_PATH"/mount.sh "$fs" "$del_thrds" "1"
        fs=$fs-$del_thrds
    else
        bash "$TOOLS_PATH"/mount.sh "$fs" "cow=1"
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

mkdir -p "$ABS_PATH"/M_DATA/filebench

for fs in "${FS[@]}"; do
    compile_fs "$fs" "1" "1"
    for thrd in "${THREADS[@]}"; do
        for workload in "${FILE_BENCHES[@]}"; do
            mkdir -p "$ABS_PATH"/DATA/"$workload"

            dmesg -C

            if [[ "${fs}" == "nova" ]]; then
                do_fb "$fs" "$workload" "$thrd"
            else
                do_fb "$fs" "$workload" "$thrd" "$del_thrds"
            fi

            mkdir -p "$ABS_PATH"/M_DATA/filebench/${workload}
            dmesg -c >"$ABS_PATH"/M_DATA/filebench/${workload}/${fs}

            sleep 1

            total_time=$(dmesg_attr_time "$ABS_PATH"/M_DATA/filebench/${workload}/${fs} "write_total")
            meta_time=$(dmesg_attr_time "$ABS_PATH"/M_DATA/filebench/${workload}/${fs} "write_meta")
            data_time=$(dmesg_attr_time "$ABS_PATH"/M_DATA/filebench/${workload}/${fs} "write_data")

            if [[ "${fs}" == "nova" ]]; then
                table_add_row "$TABLE_NAME_NOVA" "$workload $total_time $meta_time $data_time"
            elif [[ "${fs}" == "odinfs" ]]; then
                table_add_row "$TABLE_NAME_ODINFS" "$workload $total_time $meta_time $data_time"
            elif [[ "${fs}" == "parfs" ]]; then
                table_add_row "$TABLE_NAME_PARFS" "$workload $total_time $meta_time $data_time"
            elif [[ "${fs}" == "nvodin" ]]; then
                table_add_row "$TABLE_NAME_NVODIN" "$workload $total_time $meta_time $data_time"
            else
                echo "fs: ${fs}"
            fi

        done
    done
done
