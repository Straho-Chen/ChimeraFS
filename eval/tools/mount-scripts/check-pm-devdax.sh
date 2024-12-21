#!/usr/bin/bash

dev_num=$1

json_data=$(sudo ndctl list -n namespace$dev_num.0)

mode=$(echo $json_data | jq -r '.[].mode')

if [ "$mode" = "devdax" ]; then
    echo "PM already init as devdax"
else
    echo "PM not init as devdax"
    sudo ndctl disable-namespace namespace$dev_num.0
    sudo ndctl destroy-namespace namespace$dev_num.0 --force
    sudo ndctl create-namespace -m devdax
fi
