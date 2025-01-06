#!/bin/bash

set -x

source "./common.sh"

# give the numa node to run <0, 1>
mode=$1
numa=$2
fio_path=../benchmark/bin/fio/bin

log_dir=$3
mkdir -p $log_dir

log_prefix=$4

echo "run $numa"
fio_config=$(gen_config $mode $numa)
$fio_path/fio $fio_config >"$log_dir"/"$log_prefix"-"$numa".log
echo "run $numa done"
