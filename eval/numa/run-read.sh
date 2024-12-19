#!/bin/bash

config_dir=./config/read/
config="all-local pm-local pm-remote"
fio_path=../benchmark/fio/build/bin

echo "init"
$fio_path/fio $config_dir/config-2m-all-local.fio >/dev/null
echo "init done"

log_dir=./read-log/
mkdir $log_dir

for i in $config; do
    echo start $i
    sudo pcm 1>$log_dir/pcm-$i.txt 2>&1 &
    pid=$!

    $fio_path/fio $config_dir/config-2m-$i.fio >$log_dir/fio-output-$i.txt

    sudo kill -9 $pid
    echo end $i
done

echo "init 2nd"
$fio_path/fio $config_dir/config-2m-pm-remote.fio >/dev/null
echo "init done"

echo start remote-2nd

sudo pcm 1>$log_dir/pcm-pm-remote-2nd.txt 2>&1 &
pid=$!

$fio_path/fio $config_dir/config-2m-pm-remote.fio >$log_dir/fio-output-pm-remote-2nd.txt

sudo kill -9 $pid
echo end remote-2nd
