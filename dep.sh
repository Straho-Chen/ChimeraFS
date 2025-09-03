#!/bin/bash

sudo apt update

sudo apt install sudo libdaxctl-dev libndctl-dev libipmctl-dev libudev-dev libkmod-dev uuid-dev openssh-server vim git make cmake automake gcc flex bison dpkg-dev libssl-dev htop ndctl libnuma-dev libtool libkmod-dev pkg-config jq zlib1g-dev libpmem-dev libaio-dev numactl

# for rocksdb
sudo apt install libgflags-dev libsnappy-dev zlib1g-dev libbz2-dev liblz4-dev libzstd-dev
