#!/usr/bin/bash

dev_num=$1

json_data=$(sudo ndctl list -n namespace$dev_num.0)

mode=$(echo $json_data | jq -r '.[].mode')

if [ "$mode" = "fsdax" ]; then
    echo "PM already init as fsdax"
else
    echo "PM not init as fsdax"
    sudo ndctl disable-namespace namespace$dev_num.0
    sudo ndctl destroy-namespace namespace$dev_num.0 --force
    sudo ndctl create-namespace -m fsdax
fi
