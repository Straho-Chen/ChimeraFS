#!/usr/bin/bash

function table_create() {
    local TABLE_NAME
    local COLUMNS
    TABLE_NAME=$1
    COLUMNS=$2
    echo "$COLUMNS" >"$TABLE_NAME"
}

function table_add_row() {
    local TABLE_NAME
    local ROW
    TABLE_NAME=$1
    ROW=$2
    echo "$ROW" >>"$TABLE_NAME"
}

function where_is_script() {
    local script=$1
    cd "$(dirname "$script")" && pwd
}

function filebench_attr_iops() {
    FILE_BENCH_OUT=$1
    awk '$7=="ops/s" {print $6}' "$FILE_BENCH_OUT"
}

function filebench_attr_breakdown() {
    FILE_BENCH_OUT=$1
    ATTR_BD=$2
    a=$(cat "$FILE_BENCH_OUT" | grep -w "$ATTR_BD" | awk '{print $5}' | sed 's#ms/op##g')
    echo "$a"
}

function clean_mnt_dir() {
    MNT_DIR=$1
    rm -rf "$MNT_DIR"/*
}

function drop_cache() {
    echo "Drop caches..."
    sync
    echo 3 | sudo tee /proc/sys/vm/drop_caches
    sync
    echo 3 | sudo tee /proc/sys/vm/drop_caches
    sync
    echo 3 | sudo tee /proc/sys/vm/drop_caches
}

function ufs_lib_path() {
    local fs=$1
    ABS_PATH=$(where_is_script "$0")
    FS_PATH=$ABS_PATH/../../fs
    if [[ "$fs" == "madfs" ]]; then
        echo "$FS_PATH/Madfs/build-release/libmadfs.so"
    else
        echo ""
    fi
}

function dmesg_attr_time() {
    local output="$1"
    local stat="$2"

    cat "$output" | grep -w "$stat" | awk -F: '{print $3}' | sed 's/ //g'
}

function compile_fs() {
    local fs=$1
    local bd=$2
    local cow=$3

    ABS_PATH=$(where_is_script "$0")
    FS_DIR=$ABS_PATH/../../fs
    dir_change() (
        cd "$FS_DIR" || exit
        if [[ "$fs" == "madfs" ]]; then
            cd "$FS_DIR/Madfs" || exit
            make BUILD_TARGETS="madfs"
            cd "$FS_DIR" || exit
        else
            bash compile.sh "$fs" "$bd" "$cow"
        fi
    )
    dir_change
}
