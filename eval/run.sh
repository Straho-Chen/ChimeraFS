#!/usr/bin/bash

run_test() {
    local dir=$1
    cd "$dir" && ./test.sh && cd -
}

run_plot() {
    local dir=$1
    cd "$dir" && ./plot.sh && cd -
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

run_plot "moti"
run_plot "dele_size" 
run_plot "blk_size"
run_plot "fio"
run_plot "filebench"
run_plot "leveldb"
run_plot "breakdown"
run_plot "opt_append"
run_plot "io_dispatch"
run_plot "spm"