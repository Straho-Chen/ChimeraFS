#!/bin/bash

source "../tools/common.sh"

ABS_PATH=$(where_is_script "$0")
TOOLS_PATH=$ABS_PATH/../tools
output="$ABS_PATH/output"
result="$ABS_PATH/performance-comparison-table"
# rocksdb source from (compling with fallocate disabled): https://github.com/facebook/rocksdb/archive/refs/tags/v10.0.1.tar.gz
rocksdb_dbbench=$ABS_PATH/../benchmark/bin/rocksdb/dbbench
pmem_dir=/mnt/pmem0

FS=("ext4-dax" "ext4-raid" "nova" "pmfs" "winefs")

DELEGATION_FS=("odinfs" "parfs")

DEL_THRDS=(1 2 4 8 12)

NUM_JOBS=(1)

WORKLOADS=("fillseq" "fillrandom" "appendrandom" "updaterandom")

run_benchmark() {
    fs=$1
    threads=$2
    workload=$3

    echo ----------------------- RocksDB Workload: $3 ---------------------------

    mkdir -p $output/$fs/$threads

    $rocksdb_dbbench --db=$pmem_dir/rocksdb --num_levels=6 --key_size=20 --prefix_size=20 \
        --keys_per_prefix=0 --value_size=4076 --cache_size=17179869184 --cache_numshardbits=6 \
        --compression_type=none --compression_ratio=1 --min_level_to_compress=-1 --disable_seek_compaction=1 \
        --write_buffer_size=134217728 --max_write_buffer_number=2 \
        --level0_file_num_compaction_trigger=8 --target_file_size_base=134217728 \
        --max_bytes_for_level_base=1073741824 --disable_wal=0 --wal_dir=$pmem_dir/rocksdb/WAL_LOG \
        --sync=1 --verify_checksum=1 --delete_obsolete_files_period_micros=314572800 \
        --max_background_compactions=4 --max_background_flushes=0 --level0_slowdown_writes_trigger=16 \
        --level0_stop_writes_trigger=24 --statistics=0 --stats_per_interval=0 --stats_interval=1048576 \
        --histogram=0 --use_plain_table=1 --open_files=-1 --mmap_read=0 --mmap_write=0 \
        --bloom_bits=10 --bloom_locality=1 \
        --benchmarks="$workload" --use_existing_db=0 --num=1000000 --threads="$threads" 2>&1 | tee $output/$fs/$threads/rocksdb
    throughput=$(cat $output/$fs/$threads/rocksdb | grep -oE '[0-9]+\.[0-9]+ MB/s' | sed 's/ MB\/s//')
    echo -n " $throughput" >>$result

    echo Sleeping for 1 seconds . .
    sleep 1
}

echo "fs num_job fill_seq(MB/s) fillrandom(MB/s) appendrandom(MB/s) updaterandom(MB/s)" >$result

for job in "${NUM_JOBS[@]}"; do
    for fs in "${FS[@]}"; do
        echo -n $fs >>$result
        echo -n " $job" >>$result
        echo "Running with $job threads"
        for workload in "${WORKLOADS[@]}"; do
            bash "$TOOLS_PATH"/mount.sh "$fs"
            run_benchmark $fs $job $workload
            bash "$TOOLS_PATH"/umount.sh "$fs"
        done

        echo "" >>$result
    done
done

for job in "${NUM_JOBS[@]}"; do
    for file_system in "${DELEGATION_FS[@]}"; do
        for del_thrds in "${DEL_THRDS[@]}"; do
            echo -n $file_system-$del_thrds >>$result
            echo -n " $job" >>$result
            echo "Running with $job threads"
            for workload in "${WORKLOADS[@]}"; do
                bash "$TOOLS_PATH"/mount.sh "$file_system" "$del_thrds"
                run_benchmark $file_system-$del_thrds $job $workload
                bash "$TOOLS_PATH"/umount.sh "$file_system"
            done

            echo "" >>$result
        done
    done
done
