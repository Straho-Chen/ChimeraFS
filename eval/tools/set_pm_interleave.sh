#!/usr/bin/bash

interleaved=$1

delete_all_namespace() {
    namespaces=$(ndctl list -N | grep -oP '"dev":"\K[^"]+')
    for ns in $namespaces; do
        echo "Delete namespace $ns"
        sudo ndctl destroy-namespace -f "$ns"
    done
}

# delete all namespace
delete_all_namespace

# deleta all pervious not-applied goal
ipmctl delete -goal

# ipmctl create -goal PersistentMemoryType=AppDirectNotInterleaved
if [[ $interleaved -eq 1 ]]; then
    echo "Create AppDirect interleaved"
    ipmctl create -goal PersistentMemoryType=AppDirect
else
    echo "Create AppDirect not interleaved"
    ipmctl create -goal PersistentMemoryType=AppDirectNotInterleaved
fi
