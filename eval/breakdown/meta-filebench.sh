#!/usr/bin/bash

set -ex

source "../tools/common.sh"

sudo -v

echo 0 >/proc/sys/kernel/randomize_va_space

ABS_PATH=$(where_is_script "$0")
TOOLS_PATH=$ABS_PATH/../tools

FSCRIPT_PRE_FIX=$TOOLS_PATH/fbscripts
FB_PATH=$ABS_PATH/../benchmark/bin/filebench/bin

FS=("pmfs" "nova" "cknova" "nvodin" "idel")

DELEGATION_FS=("nvodin" "idel")

del_thrds=(12)

FILE_BENCHES=("fileserver.f" "varmail.f" "webserver.f" "webproxy.f")

THREADS=(32)

TABLE_NAME_PMFS="$ABS_PATH/performance-comparison-table-pmfs"

TABLE_NAME_NOVA="$ABS_PATH/performance-comparison-table-nova"

TABLE_NAME_CKNOVA="$ABS_PATH/performance-comparison-table-cknova"

TABLE_NAME_NVODIN="$ABS_PATH/performance-comparison-table-nvodin"

TABLE_NAME_IDEL="$ABS_PATH/performance-comparison-table-idel"

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

mkdir -p "$ABS_PATH"/M_DATA/filebench

for fs in "${FS[@]}"; do
    compile_fs "$fs" "1"
    for thrd in "${THREADS[@]}"; do
        for workload in "${FILE_BENCHES[@]}"; do
            mkdir -p "$ABS_PATH"/DATA/"$workload"

            dmesg -C

            do_fb "$fs" "$workload" "$thrd" "$del_thrds"

            mkdir -p "$ABS_PATH"/M_DATA/filebench/${workload}
            dmesg -c >"$ABS_PATH"/M_DATA/filebench/${workload}/${fs}

            sleep 1

            total_time=$(dmesg_attr_time "$ABS_PATH"/M_DATA/filebench/${workload}/${fs} "write_total")
            meta_time=$(dmesg_attr_time "$ABS_PATH"/M_DATA/filebench/${workload}/${fs} "write_meta")
            data_time=$(dmesg_attr_time "$ABS_PATH"/M_DATA/filebench/${workload}/${fs} "write_data")

            if [[ "${fs}" == "nova" ]]; then
                table_add_row "$TABLE_NAME_NOVA" "$workload $total_time $meta_time $data_time"
            elif [[ "${fs}" == "cknova" ]]; then
                table_add_row "$TABLE_NAME_CKNOVA" "$workload $total_time $meta_time $data_time"
            elif [[ "${fs}" == "pmfs" ]]; then
                table_add_row "$TABLE_NAME_PMFS" "$workload $total_time $meta_time $data_time"
            elif [[ "${fs}" == "nvodin" ]]; then
                table_add_row "$TABLE_NAME_NVODIN" "$workload $total_time $meta_time $data_time"
            elif [[ "${fs}" == "idel" ]]; then
                table_add_row "$TABLE_NAME_IDEL" "$workload $total_time $meta_time $data_time"
            else
                echo "fs: ${fs}"
            fi

        done
    done
done
