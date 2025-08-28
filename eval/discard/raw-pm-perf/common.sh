#!/usr/bin/bash

config_dir=./config/

function gen_config() {
    mode=$1
    numa=$2
    bsize=$3
    fsize=$4
    numjobs=$5
    fio_config=$(mktemp)
    cat $config_dir/global.fio >$fio_config
    cat $config_dir/numa/$numa.fio >>$fio_config
    echo "[libpmem]" >>$fio_config
    echo "rw=$mode" >>$fio_config
    echo "bs=$bsize" >>$fio_config
    echo "size=${fsize}M" >>$fio_config
    echo "numjobs=$numjobs" >>$fio_config
    echo "stonewall" >>$fio_config
    echo $fio_config
}
