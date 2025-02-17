#!/bin/bash

sudo -v

echo 32 >/proc/sys/kernel/watchdog_thresh
echo 0 >/proc/sys/kernel/lock_stat
echo 262144 >/proc/sys/vm/max_map_count

echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

timing=1
data_checksum=1
metadata_checksum=1

chsm_fs=(nova parfs)
# fs=(pmfs nova winefs odinfs parfs)
fs=(parfs)

# Work around, will fix
sudo rm -rf /dev/pmem_ar*

for i in ${fs[@]}; do
    cd $i
    make clean && make -j
    sudo rmmod $i
    # if fs fall in chsm_fs, then enable checksum
    if [[ " ${chsm_fs[@]} " =~ " ${i} " ]]; then
        sudo insmod build/$i.ko measure_timing=$timing data_csum=$data_checksum metadata_csum=$metadata_checksum
        sudo insmod $i.ko measure_timing=$timing data_csum=$data_checksum metadata_csum=$metadata_checksum
    else
        sudo insmod build/$i.ko measure_timing=$timing
        sudo insmod $i.ko measure_timing=$timing
    fi
    cd -
done

cd parradm
make clean && make -j
sudo make install
cd -

stat=0

echo ""
echo "------------------------------------------------------------------"
echo "Checking"
for i in ${fs[@]}; do
    lsmod | grep $i >/dev/null
    if [ $? -eq 0 ]; then
        echo $i installed successfully
    else
        echo $i *not* installed
        stat=1
    fi
done

echo ""

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
