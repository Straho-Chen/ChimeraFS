#!/usr/bin/bash

run_test() {
    local dir=$1
    cd "$dir" && ./test.sh && cd -
}

run_test "moti"
run_test "dele_size" 
run_test "blk_size"
run_test "fio"
run_test "filebench"
run_test "leveldb"
run_test "breakdown"
run_test "opt_append"
run_test "io_dispatch"
run_test "spm"
run_test "recovery_time"