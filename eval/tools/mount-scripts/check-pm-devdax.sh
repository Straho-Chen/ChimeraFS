#!/usr/bin/bash

json_data=$(sudo ndctl list -n namespace0.0)

mode=$(echo $json_data | jq -r '.[].mode')

if [ "$mode" = "devdax" ]; then
    echo "PM already init as devdax"
else
    echo "PM not init as devdax"
    sudo ndctl disable-namespace namespace0.0
    sudo ndctl destroy-namespace namespace0.0 --force
    sudo ndctl create-namespace -m devdax
fi
