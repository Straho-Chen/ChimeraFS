#!/usr/bin/bash

set -ex

source "../tools/common.sh"

sudo -v

echo 0 >/proc/sys/kernel/randomize_va_space

ABS_PATH=$(where_is_script "$0")
TOOLS_PATH=$ABS_PATH/../tools

FSCRIPT_PRE_FIX=$TOOLS_PATH/fbscripts
FB_PATH=$ABS_PATH/../benchmark/bin/filebench/bin

FS=("parfs" "idel")

DELEGATION_FS=("idel" "parfs")

del_thrds=(12)

FILE_BENCHES=("fileserver.f" "varmail.f" "webserver.f" "webproxy.f")

THREADS=(32)

TABLE_NAME_IDEL="$ABS_PATH/performance-comparison-table-idel"

TABLE_NAME_PARFS="$ABS_PATH/performance-comparison-table-parfs"

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

            # if $fs fall in delegation fs
            if [[ "${DELEGATION_FS[@]}" =~ "$fs" ]]; then
                iops=$(filebench_attr_iops "$ABS_PATH"/DATA/"$workload"/"$fs-$del_thrds"-"$thrd")
            else
                iops=$(filebench_attr_iops "$ABS_PATH"/DATA/"$workload"/"$fs"-"$thrd")
            fi

            echo "iops: $iops"

            mkdir -p "$ABS_PATH"/M_DATA/filebench/${workload}
            dmesg -c >"$ABS_PATH"/M_DATA/filebench/${workload}/${fs}

            sleep 1

            total_time=$(echo "scale=4; $(dmesg_attr_time "$ABS_PATH"/M_DATA/filebench/${workload}/${fs} "write_total") / $iops" | bc)
            meta_time=$(echo "scale=4; $(dmesg_attr_time "$ABS_PATH"/M_DATA/filebench/${workload}/${fs} "write_meta") / $iops" | bc)
            data_time=$(echo "scale=4; $(dmesg_attr_time "$ABS_PATH"/M_DATA/filebench/${workload}/${fs} "write_data") / $iops" | bc)
            data_csum_time=$(echo "scale=4; $(dmesg_attr_time "$ABS_PATH"/M_DATA/filebench/${workload}/${fs} "write_data_csum") / $iops" | bc)
            comu_time=$(echo "scale=4; $(dmesg_attr_time "$ABS_PATH"/M_DATA/filebench/${workload}/${fs} "write_comu") / $iops" | bc)
            wait_complete_time=$(echo "scale=4; $(dmesg_attr_time "$ABS_PATH"/M_DATA/filebench/${workload}/${fs} "write_complete") / $iops" | bc)
            sync_data_time=$(echo "scale=4; $(dmesg_attr_time "$ABS_PATH"/M_DATA/filebench/${workload}/${fs} "write_sync_data") / $iops" | bc)

            if [[ "${fs}" == "idel" ]]; then
                table_add_row "$TABLE_NAME_IDEL" "$workload $total_time $meta_time $data_time $data_csum_time $comu_time $wait_complete_time $sync_data_time"
            elif [[ "${fs}" == "parfs" ]]; then
                table_add_row "$TABLE_NAME_PARFS" "$workload $total_time $meta_time $data_time $data_csum_time $comu_time $wait_complete_time $sync_data_time"
            else
                echo "fs: ${fs}"
            fi

        done
    done
done
