#!/bin/bash

source "../tools/common.sh"

ABS_PATH=$(where_is_script "$0")
TOOLS_PATH=$ABS_PATH/../tools
output="$ABS_PATH/output"
result="$ABS_PATH/performance-comparison-table"

pmem_dir=/mnt/pmem0
leveldb_dbbench=$ABS_PATH/../benchmark/bin/leveldb/dbbench
database_dir=$pmem_dir/leveldbtest
workload_dir=$ABS_PATH/workloads

# FS=("ext4-dax" "ext4-raid" "nova" "pmfs" "winefs")
# FS=("nova")

# DELEGATION_FS=("odinfs" "parfs")
DELEGATION_FS=("parfs-single-pm")
# DELEGATION_FS=("odinfs")

# UFS=("madfs")

# DEL_THRDS=(1 2 4 8 12)
DEL_THRDS=(12)

# NUM_JOBS=(1)
NUM_JOBS=(1 2 4 8 16 28 32)
# NUM_JOBS=(32)

rm -rf $output

load_workload() {
    tracefile=$1
    fs=$2
    threads=$3
    echo ----------------------- YCSB Load $tracefile ---------------------------

    echo "Leveldb: load $1" >/dev/kmsg

    export trace_file=$workload_dir/$tracefile
    mkdir -p $output/$fs/$threads

    $leveldb_dbbench --num_multi_db="$threads" --max_replay_entries=10000 --use_existing_db=0 --benchmarks=ycsb --db=$database_dir --max_file_size=$((128 * 1024 * 1024 * 1024)) --threads="$threads" --open_files=10000 2>&1 | tee $output/$fs/$threads/$tracefile

    ycsb_result=$(grep -oP 'ycsb\s*:\s*\K\d+(\.\d+)?' $output/$fs/$threads/$tracefile)
    echo -n " $ycsb_result" >>$result

    echo Sleeping for 1 seconds . .
    sleep 1
}

run_workload() {
    tracefile=$1
    fs=$2
    threads=$3
    echo ----------------------- LevelDB YCSB Run $tracefile ---------------------------

    echo "Leveldb: run $1" >/dev/kmsg

    export trace_file=$workload_dir/$tracefile
    mkdir -p $output/$fs/$threads/

    $leveldb_dbbench --num_multi_db="$threads" --max_replay_entries=10000 --use_existing_db=1 --benchmarks=ycsb --db=$database_dir --threads="$threads" --max_file_size=$((128 * 1024 * 1024 * 1024)) --open_files=10000 2>&1 | tee $output/$fs/$threads/$tracefile

    ycsb_result=$(grep -oP 'ycsb\s*:\s*\K\d+(\.\d+)?' $output/$fs/$threads/$tracefile)
    echo -n " $ycsb_result" >>$result

    echo Sleeping for 1 seconds . .
    sleep 1
}

echo "fs num_job loada(micros/op) runa(micros/op) runb(micros/op) runc(micros/op) rund(micros/op) loade(micros/op) rune(micros/op) runf(micros/op)" >$result

for fs in "${FS[@]}"; do
    for job in "${NUM_JOBS[@]}"; do
        echo "Running with $job threads"
        echo -n $fs >>$result
        echo -n " $job" >>$result

        # mount
        umount /mnt/pmem0
        dmesg -C
        compile_fs "$fs" "0"
        bash "$TOOLS_PATH"/mount.sh "$fs"

        load_workload loada_1M $fs $job
        run_workload runa_1M_1M $fs $job
        run_workload runb_1M_1M $fs $job
        run_workload runc_1M_1M $fs $job
        run_workload rund_1M_1M $fs $job

        bash "$TOOLS_PATH"/umount.sh "$fs"
        # remount
        bash "$TOOLS_PATH"/mount.sh "$fs"

        load_workload loade_1M $fs $job
        run_workload rune_1M_1M $fs $job
        run_workload runf_1M_1M $fs $job

        bash "$TOOLS_PATH"/umount.sh "$fs"

        echo "" >>$result
    done
done

for file_system in "${DELEGATION_FS[@]}"; do
    for del_thrds in "${DEL_THRDS[@]}"; do
        for job in "${NUM_JOBS[@]}"; do
            echo "Running with $job threads"
            echo -n $file_system-$del_thrds >>$result
            echo -n " $job" >>$result

            # mount
            dmesg -C
            umount /mnt/pmem0
            rmmod parfs
            compile_fs "$file_system" "0"
            bash "$TOOLS_PATH"/mount.sh "$file_system" "$del_thrds"

            load_workload loada_1M $file_system-$del_thrds $job
            run_workload runa_1M_1M $file_system-$del_thrds $job
            run_workload runb_1M_1M $file_system-$del_thrds $job
            run_workload runc_1M_1M $file_system-$del_thrds $job
            run_workload rund_1M_1M $file_system-$del_thrds $job

            bash "$TOOLS_PATH"/umount.sh "$file_system"

            # remount
            bash "$TOOLS_PATH"/mount.sh "$file_system" "$del_thrds"

            load_workload loade_1M $file_system-$del_thrds $job
            run_workload rune_1M_1M $file_system-$del_thrds $job
            run_workload runf_1M_1M $file_system-$del_thrds $job

            bash "$TOOLS_PATH"/umount.sh "$file_system"

            echo "" >>$result
        done
    done
done

load_workload_ufs() {
    tracefile=$1
    fs=$2
    threads=$3
    ulib_path=$4
    echo ----------------------- YCSB Load $tracefile ---------------------------

    export trace_file=$workload_dir/$tracefile
    mkdir -p $output/$fs/$threads

    LD_PRELOAD=$ulib_path $leveldb_dbbench --use_existing_db=0 --benchmarks=ycsb,stats,printdb --db=$database_dir --threads="$threads" --open_files=10 2>&1 | tee $output/$fs/$threads/$tracefile

    ycsb_result=$(grep -oP 'ycsb\s*:\s*\K\d+(\.\d+)?' $output/$fs/$threads/$tracefile)
    echo -n " $ycsb_result" >>$result

    echo Sleeping for 1 seconds . .
    sleep 1
}

run_workload_ufs() {
    tracefile=$1
    fs=$2
    threads=$3
    ulib_path=$4
    echo ----------------------- LevelDB YCSB Run $tracefile ---------------------------

    export trace_file=$workload_dir/$tracefile
    mkdir -p $output/$fs/$threads/

    LD_PRELOAD=$ulib_path $leveldb_dbbench --use_existing_db=1 --benchmarks=ycsb,stats,printdb --db=$database_dir --threads="$threads" --open_files=1000 2>&1 | tee $output/$fs/$threads/$tracefile
    ycsb_result=$(grep -oP 'ycsb\s*:\s*\K\d+(\.\d+)?' $output/$fs/$threads/$tracefile)
    echo -n " $ycsb_result" >>$result

    echo Sleeping for 1 seconds . .
    sleep 1
}

for fs in "${UFS[@]}"; do
    ulib_path=$(ufs_lib_path "$fs")
    for job in "${NUM_JOBS[@]}"; do
        echo "Running with $job threads"
        echo -n $fs >>$result
        echo -n " $job" >>$result

        # mount
        bash "$TOOLS_PATH"/mount.sh "ext4-dax"

        load_workload_ufs loada_1M $fs $job $ulib_path
        run_workload_ufs runa_1M_1M $fs $job $ulib_path
        run_workload_ufs runb_1M_1M $fs $job $ulib_path
        run_workload_ufs runc_1M_1M $fs $job $ulib_path
        run_workload_ufs rund_1M_1M $fs $job $ulib_path

        bash "$TOOLS_PATH"/umount.sh "ext4-dax"

        # remount
        bash "$TOOLS_PATH"/mount.sh "ext4-dax"

        load_workload_ufs loade_1M $fs $job $ulib_path
        run_workload_ufs rune_1M_1M $fs $job $ulib_path
        run_workload_ufs runf_1M_1M $fs $job $ulib_path

        bash "$TOOLS_PATH"/umount.sh "ext4-dax"

        echo "" >>$result
    done
done
