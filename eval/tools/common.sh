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
    echo 1 | sudo tee /proc/sys/vm/drop_caches
    sync
    echo 1 | sudo tee /proc/sys/vm/drop_caches
    sync
    echo 1 | sudo tee /proc/sys/vm/drop_caches
}
