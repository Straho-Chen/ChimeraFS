#!/bin/bash

sudo -v

echo 32 >/proc/sys/kernel/watchdog_thresh

echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

timing=1

fs=(pmfs nova winefs odinfs parfs)

# Work around, will fix
sudo rm -rf /dev/pmem_ar*

for i in ${fs[@]}; do
    cd $i
    make clean && make -j
    sudo rmmod $i
    sudo insmod $i.ko measure_timing=$timing
    sudo insmod build/$i.ko measure_timing=$timing
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
