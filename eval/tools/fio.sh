#!/usr/bin/bash

function where_is_script() {
    local script=$1
    cd "$(dirname "$script")" && pwd
}

ABS_PATH=$(where_is_script "$0")

FIO=$ABS_PATH/../benchmark/bin/fio/bin/fio

DIR=$1
BS=$2   # in B
SIZE=$3 # in MB
THREADS=$4
MODE=$5

# create new tmp file for fio config
fio_config=$(mktemp)

cat $ABS_PATH/global.fio >$fio_config

echo "directory=$DIR" >>$fio_config
echo "fallocate=none" >>$fio_config
echo "ioengine=sync" >>$fio_config

echo "[tmp]" >>$fio_config
echo "new_group" >>$fio_config
echo "bs=$BS" >>$fio_config
echo "size=${SIZE}M" >>$fio_config
echo "numjobs=$THREADS" >>$fio_config
echo "rw=$MODE" >>$fio_config
echo "stonewall" >>$fio_config

$FIO $fio_config
