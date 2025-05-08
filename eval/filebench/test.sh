#!/usr/bin/bash

set -ex

source "../tools/common.sh"

sudo -v

echo 0 >/proc/sys/kernel/randomize_va_space

ABS_PATH=$(where_is_script "$0")
TOOLS_PATH=$ABS_PATH/../tools
FSCRIPT_PRE_FIX=$TOOLS_PATH/fbscripts
FB_PATH=$ABS_PATH/../benchmark/bin/filebench/bin

# FS=("ext4-dax" "ext4-raid" "nova" "pmfs" "winefs")
FS=("nova")

# DELEGATION_FS=("odinfs" "parfs")
DELEGATION_FS=("parfs")

FILE_BENCHES=("fileserver.f" "varmail.f" "webserver.f" "webproxy.f")
# FILE_BENCHES=("varmail.f")

THREADS=(1 2 4 8 16 28 32)
# THREADS=(4)

# DEL_THRDS=(1 2 4 8 12)
DEL_THRDS=(12)

TABLE_NAME="$ABS_PATH/performance-comparison-table"

table_create "$TABLE_NAME" "fs fbench threads iops create delete close read write IO"

loop=1
if [ "$1" ]; then
    loop=$1
fi

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

    iops=$(filebench_attr_iops "$ABS_PATH"/DATA/"$fbench"/"$fs"-"$thread")
    # Retreive the breakdown
    case "${fbench}" in
    "fileserver.f")
        create=$(filebench_attr_breakdown "$ABS_PATH"/DATA/"$fbench"/"$fs"-"$thread" "openfile1")
        delete=$(filebench_attr_breakdown "$ABS_PATH"/DATA/"$fbench"/"$fs"-"$thread" "deletefile1")
        close=$(filebench_attr_breakdown "$ABS_PATH"/DATA/"$fbench"/"$fs"-"$thread" "closefile1")
        read=$(filebench_attr_breakdown "$ABS_PATH"/DATA/"$fbench"/"$fs"-"$thread" "readfile1")
        write=$(filebench_attr_breakdown "$ABS_PATH"/DATA/"$fbench"/"$fs"-"$thread" "wrtfile1")
        ;;
    "varmail.f")
        create=$(filebench_attr_breakdown "$ABS_PATH"/DATA/"$fbench"/"$fs"-"$thread" "openfile3")
        delete=$(filebench_attr_breakdown "$ABS_PATH"/DATA/"$fbench"/"$fs"-"$thread" "deletefile1")
        close=$(filebench_attr_breakdown "$ABS_PATH"/DATA/"$fbench"/"$fs"-"$thread" "closefile2")
        read=$(filebench_attr_breakdown "$ABS_PATH"/DATA/"$fbench"/"$fs"-"$thread" "readfile4")
        write=$(filebench_attr_breakdown "$ABS_PATH"/DATA/"$fbench"/"$fs"-"$thread" "appendfilerand2")
        ;;
    "webserver.f")
        create=$(filebench_attr_breakdown "$ABS_PATH"/DATA/"$fbench"/"$fs"-"$thread" "openfile1")
        delete=0
        close=$(filebench_attr_breakdown "$ABS_PATH"/DATA/"$fbench"/"$fs"-"$thread" "closefile1")
        read=$(filebench_attr_breakdown "$ABS_PATH"/DATA/"$fbench"/"$fs"-"$thread" "readfile1")
        write=$(filebench_attr_breakdown "$ABS_PATH"/DATA/"$fbench"/"$fs"-"$thread" "appendlog")
        ;;
    "webproxy.f")
        create=$(filebench_attr_breakdown "$ABS_PATH"/DATA/"$fbench"/"$fs"-"$thread" "createfile1")
        delete=$(filebench_attr_breakdown "$ABS_PATH"/DATA/"$fbench"/"$fs"-"$thread" "deletefile1")
        close=$(filebench_attr_breakdown "$ABS_PATH"/DATA/"$fbench"/"$fs"-"$thread" "closefile1")
        read=$(filebench_attr_breakdown "$ABS_PATH"/DATA/"$fbench"/"$fs"-"$thread" "readfile2")
        write=$(filebench_attr_breakdown "$ABS_PATH"/DATA/"$fbench"/"$fs"-"$thread" "appendfilerand1")
        ;;
    *)
        echo "default (none of above)"
        ;;
    esac
    IO=$(echo $read $write | awk '{print $1 + $2}')

    bash "$TOOLS_PATH"/umount.sh "$fs_raw"

    table_add_row "$TABLE_NAME" "$fs $fbench $thread $iops $create $delete $close $read $write $IO"
    fs=$fs_raw
}

for fs in "${FS[@]}"; do
    compile_fs "$fs" "0"
    for fbench in "${FILE_BENCHES[@]}"; do
        mkdir -p "$ABS_PATH"/DATA/"$fbench"
        for thrd in "${THREADS[@]}"; do
            for ((i = 1; i <= loop; i++)); do

                do_fb "$fs" "$fbench" "$thrd"

            done
        done
    done
done

# for delegation fs

for fs in "${DELEGATION_FS[@]}"; do
    compile_fs "$fs" "0"
    for del_thrds in "${DEL_THRDS[@]}"; do
        for fbench in "${FILE_BENCHES[@]}"; do
            mkdir -p "$ABS_PATH"/DATA/"$fbench"
            for thrd in "${THREADS[@]}"; do
                for ((i = 1; i <= loop; i++)); do

                    do_fb "$fs" "$fbench" "$thrd" "$del_thrds"

                done
            done
        done
    done
done
