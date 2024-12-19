#!/bin/bash

config_dir=./config/write
config="all-local pm-local pm-remote"
fio_path=../benchmark/fio/build/bin

echo "init"
$fio_path/fio $config_dir/config-2m-all-local.fio >/dev/null
echo "init done"

log_dir=./write-log/
mkdir $log_dir

for i in $config; do
    echo start $i
    sudo pcm 1>$log_dir/pcm-$i.txt 2>&1 &
    pid=$!

    $fio_path/fio $config_dir/config-2m-$i.fio >$log_dir/fio-output-$i.txt

    sudo kill -9 $pid
    echo end $i
done
