#!/bin/bash

set -x

source "./common.sh"

fio_path=../benchmark/bin/fio/bin

# give the numa node to run <0, 1>
mode=$1
numa=$2
# in B
bsize=$3
# in MB
fsize=$4
numjobs=$5
log_dir=$6
mkdir -p $log_dir

log_prefix=$7

echo "run $numa"
fio_config=$(gen_config $mode $numa $bsize $fsize $numjobs)
$fio_path/fio $fio_config >"$log_dir"/"$log_prefix"-"$numa".log
echo "run $numa done"
