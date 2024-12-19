#!/usr/bin/bash

json_data=$(sudo ndctl list -n namespace0.0)

mode=$(echo $json_data | jq -r '.[].mode')

if [ "$mode" = "sector" ]; then
    echo "PM already init as sector"
else
    echo "PM not init as sector"
    sudo ndctl disable-namespace namespace0.0
    sudo ndctl destroy-namespace namespace0.0 --force
    sudo ndctl create-namespace --force --reconfig=namespace0.0 --mode=sector
fi
