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

FS=("ext4-dax" "ext4-raid" "nova" "pmfs" "winefs")

DELEGATION_FS=("odinfs" "parfs")

DEL_THRDS=(1 2 4 8 12)

NUM_JOBS=(1 2 4 8 16 28 32)

rm -rf $output

load_workload() {
    tracefile=$1
    fs=$2
    threads=$3
    echo ----------------------- YCSB Load $tracefile ---------------------------

    export trace_file=$workload_dir/$tracefile
    mkdir -p $output/$fs/$threads

    $leveldb_dbbench --use_existing_db=0 --benchmarks=ycsb,stats,printdb --db=$database_dir --threads="$threads" --open_files=10 2>&1 | tee $output/$fs/$threads/$tracefile

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

    export trace_file=$workload_dir/$tracefile
    mkdir -p $output/$fs/$threads/

    $leveldb_dbbench --use_existing_db=1 --benchmarks=ycsb,stats,printdb --db=$database_dir --threads="$threads" --open_files=1000 2>&1 | tee $output/$fs/$threads/$tracefile
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

for fs in "${DELEGATION_FS[@]}"; do
    for del_thrds in "${DEL_THRDS[@]}"; do
        for job in "${NUM_JOBS[@]}"; do
            echo "Running with $job threads"
            echo -n $fs-$del_thrds >>$result
            echo -n " $job" >>$result

            # mount
            bash "$TOOLS_PATH"/mount.sh "$fs" "$del_thrds"

            load_workload loada_1M $fs $job
            run_workload runa_1M_1M $fs $job
            run_workload runb_1M_1M $fs $job
            run_workload runc_1M_1M $fs $job
            run_workload rund_1M_1M $fs $job

            bash "$TOOLS_PATH"/umount.sh "$fs"

            # remount
            bash "$TOOLS_PATH"/mount.sh "$fs" "$del_thrds"

            load_workload loade_1M $fs $job
            run_workload rune_1M_1M $fs $job
            run_workload runf_1M_1M $fs $job

            bash "$TOOLS_PATH"/umount.sh "$fs"

            echo "" >>$result
        done
    done
done
