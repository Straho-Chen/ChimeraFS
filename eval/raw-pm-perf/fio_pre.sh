#!/bin/bash

config_write_dir=./config/write/
config_read_dir=./config/read/
# give the numa node to run <0, 1>
config=$1
fio_path=../benchmark/bin/fio/bin

echo "init"
$fio_path/fio $config_read_dir/config-2m-"$config".fio >/dev/null
$fio_path/fio $config_write_dir/config-2m-"$config".fio >/dev/null
echo "init done"
