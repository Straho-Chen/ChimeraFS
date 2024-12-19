#!/bin/bash

config_dir=./config/write/
# give the numa node to run <0, 1>
config=$1
fio_path=../benchmark/bin/fio/bin

log_dir=$2
mkdir -p $log_dir

log_prefix=$3

echo "run $config"
$fio_path/fio $config_dir/config-2m-"$config".fio >"$log_dir"/"$log_prefix"-"$config".log
echo "run $config done"
