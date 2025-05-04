#!/bin/bash

sudo -v

echo 32 >/proc/sys/kernel/watchdog_thresh
echo 0 >/proc/sys/kernel/lock_stat
echo 262144 >/proc/sys/vm/max_map_count

echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

function checkout_branch() (
    local branch=$1
    rm -rf $branch
    cd parfs
    git stash
    git checkout $branch
    cd ..
    cp -r parfs $branch
    cd $branch
    rm -rf ./.git
    cd ..
    cd parfs
    git checkout dev
    git stash pop
    cd ..
    chown -R $USER parfs
)

arg_fs=$1
arg_bd=$2
cow=$3

timing=0
if [[ "$cow" == "1" ]]; then
    data_checksum=1
    metadata_checksum=1
else
    data_checksum=0
    metadata_checksum=0
fi

chsm_fs=(nova parfs)

default_fs=(pmfs nova winefs odinfs parfs)

parfs_branch=(idel nvodin)

if [ -z "$arg_fs" ]; then
    echo "compile all fs"
    fs=(${default_fs[@]})
elif [[ "$arg_fs" == "default" ]]; then
    echo "compile all fs"
    fs=(${default_fs[@]})
else
    fs=($arg_fs)
fi

if [ -z "$arg_bd" ]; then
    echo "set meta breakdown to default 0"
    meta_breakdown=0
elif [[ "$arg_bd" == "1" ]]; then
    echo "set meta breakdown to 1"
    meta_breakdown=1
else
    echo "set meta breakdown to default 0"
    meta_breakdown=0
fi

if [[ " ${parfs_branch[@]} " =~ " ${arg_fs} " ]]; then
    echo "checkout branch $arg_fs"
    checkout_branch $arg_fs
    fs=($arg_fs)
elif [[ " ${default_fs[@]} " =~ " ${arg_fs} " ]]; then
    fs=($arg_fs)
else
    exit 0
fi

# Work around, will fix
sudo rm -rf /dev/*pmem_ar*

for i in ${fs[@]}; do
    cd $i
    make clean && make -j
    sudo rmmod $i
    # if fs fall in chsm_fs, then enable checksum
    if [[ " ${chsm_fs[@]} " =~ " ${i} " ]]; then
        sudo insmod build/$i.ko measure_timing=$timing measure_meta_timing=$meta_breakdown data_csum=$data_checksum metadata_csum=$metadata_checksum
        sudo insmod $i.ko measure_timing=$timing measure_meta_timing=$meta_breakdown data_csum=$data_checksum metadata_csum=$metadata_checksum
    elif [[ " ${parfs_branch[@]} " =~ " ${i} " ]]; then
        sudo rmmod parfs
        sudo insmod build/parfs.ko measure_timing=$timing measure_meta_timing=$meta_breakdown data_csum=$data_checksum metadata_csum=$metadata_checksum
        sudo insmod parfs.ko measure_timing=$timing measure_meta_timing=$meta_breakdown data_csum=$data_checksum metadata_csum=$metadata_checksum
    else
        sudo insmod build/$i.ko measure_timing=$timing measure_meta_timing=$meta_breakdown
        sudo insmod $i.ko measure_timing=$timing measure_meta_timing=$meta_breakdown
    fi
    cd -
done

cd parradm
make clean && make -j
sudo make install
cd -

stat=0

# echo ""
# echo "------------------------------------------------------------------"
# echo "Checking"
# for i in ${fs[@]}; do
#     lsmod | grep $i >/dev/null
#     if [ $? -eq 0 ]; then
#         echo $i installed successfully
#     else
#         echo $i *not* installed
#         stat=1
#     fi
# done

# echo ""

which parradm
ret=$?

if [ $ret -eq 0 ]; then
    echo "Parradm installed successfully!"

else
    echo "Parradm *not* installed"
    stat=1
fi

if [ $stat -eq 0 ]; then
    echo "All succeed!"
    echo "------------------------------------------------------------------"
    exit 0
else
    echo "Errors occur, please check the above message"
    echo "------------------------------------------------------------------"
    exit $stat
fi
