#!/usr/bin/bash

dev_num=$1

json_data=$(sudo ndctl list -n namespace$dev_num.0)

mode=$(echo $json_data | jq -r '.[].mode')

if [ "$mode" = "sector" ]; then
    echo "PM already init as sector"
else
    echo "PM not init as sector"
    sudo ndctl disable-namespace namespace$dev_num.0
    sudo ndctl destroy-namespace namespace$dev_num.0 --force
    sudo ndctl create-namespace --mode=sector
fi
