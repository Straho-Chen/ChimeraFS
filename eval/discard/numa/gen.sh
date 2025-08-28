#!/bin/bash

rm -rf config/read/remote.fio config/read/local.fio

rm -rf config/write/remote.fio config/write/local.fio

./numa-info.py