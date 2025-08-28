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

arg_debug=$3

if [ -z "$arg_debug" ]; then
    arg_debug=0
fi

timing=0
data_checksum=0
mcsum=0

chsm_fs=(nova parfs)

default_fs=(pmfs nova winefs odinfs parfs)

parfs_branch=(idel nvodin parfs-no-opt-append append_csum_whole_block append_csum_partial_block append_no_csum all_scan_recovery latest_trans_scan_recovery ckpt no_scan nvodin-kubuf low-thread idel-low-thread optfs)

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

if [[ "$arg_fs" == "cknova" ]]; then
    echo "arg_fs: $arg_fs set mcsum 1"
    mcsum=1
    fs=(nova)
elif [[ " ${parfs_branch[@]} " =~ " ${arg_fs} " ]]; then
    data_checksum=1
elif [[ "$arg_fs" == "parfs" ]]; then
    data_checksum=1
fi

if [[ " ${parfs_branch[@]} " =~ " ${arg_fs} " ]]; then
    echo "checkout branch $arg_fs"
    checkout_branch $arg_fs
    fs=($arg_fs)
fi

# Work around, will fix
sudo rm -rf /dev/*pmem_ar*

for i in ${default_fs[@]}; do
    sudo rmmod $i
done

for i in ${fs[@]}; do
    cd $i
    make clean && make -j
    # if fs fall in chsm_fs, then enable checksum
    if [[ " ${chsm_fs[@]} " =~ " ${i} " ]]; then
        sudo insmod build/$i.ko measure_timing=$timing measure_meta_timing=$meta_breakdown data_csum=$data_checksum nova_dbgmask=$arg_debug mcsum=$mcsum
        sudo insmod $i.ko measure_timing=$timing measure_meta_timing=$meta_breakdown data_csum=$data_checksum nova_dbgmask=$arg_debug mcsum=$mcsum
    elif [[ " ${parfs_branch[@]} " =~ " ${i} " ]]; then
        sudo insmod build/parfs.ko measure_timing=$timing measure_meta_timing=$meta_breakdown data_csum=$data_checksum
        sudo insmod parfs.ko measure_timing=$timing measure_meta_timing=$meta_breakdown data_csum=$data_checksum
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
