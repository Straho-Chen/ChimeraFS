#!/usr/bin/bash

config_dir=./config/

function gen_config() {
    mode=$1
    numa=$2
    numjobs=$3
    fio_config=$(mktemp)
    if [ "$mode" == "read" ]; then
        cat $config_dir/read/config-2m-"$numa".fio >$fio_config
        echo "rw=read" >>$fio_config
    elif [ "$mode" == "randread" ]; then
        cat $config_dir/read/config-2m-"$numa".fio >$fio_config
        echo "rw=randread" >>$fio_config
    elif [ "$mode" == "write" ]; then
        cat $config_dir/write/config-2m-"$numa".fio >$fio_config
        echo "rw=write" >>$fio_config
    elif [ "$mode" == "randwrite" ]; then
        cat $config_dir/write/config-2m-"$numa".fio >$fio_config
        echo "rw=randwrite" >>$fio_config
    else
        echo "Invalid mode"
        exit 1
    fi
    cat $config_dir/numa/$numa.fio >>$fio_config
    cat "numjobs=$numjobs" >>$fio_config
    echo "[libpmem]" >>$fio_config
    echo $fio_config
}
