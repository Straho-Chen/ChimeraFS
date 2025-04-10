#!/usr/bin/bash

delete_all_namespace() {
    namespaces=$(ndctl list -N | grep -oP '"dev":"\K[^"]+')
    for ns in $namespaces; do
        echo "Delete namespace $ns"
        sudo ndctl destroy-namespace -f "$ns"
    done
}

get_dimm_ids_by_socket() {
    local socket=$1
    local dimm_ids=()

    if ! output=$(sudo ipmctl show -topology -socket "$socket" 2>&1); then
        echo "Command failed: $output" >&2
        return 1
    fi

    while IFS= read -r line; do
        if [[ $line =~ ^[[:space:]]*(0x[0-9A-Fa-f]+|N/A)[[:space:]]*\| ]]; then
            dimm_id=${BASH_REMATCH[1]}
            if [[ $dimm_id != "N/A" ]]; then
                dimm_ids+=("$dimm_id")
            fi
        fi
    done <<<"$output"

    echo "${dimm_ids[@]}"
}

join_array_with_comma() {
    local IFS=','
    local arr=("$@")
    echo "${arr[*]}"
}

# get per socket dimm

dimm_socket0=($(get_dimm_ids_by_socket 0))
dimm_socket1=($(get_dimm_ids_by_socket 1))

# split dimm
split0=0
split1=0

split_region0_socket0=$dimm_socket0
split_region1_socket0=$dimm_socket0

split_region0_socket1=$dimm_socket1
split_region1_socket1=$dimm_socket1

count=${#dimm_socket0[@]}
if [[ $count -eq 1 ]]; then
    # socket 0 only have one dimm
    split0=0
else
    split0=$((count / 2))
    split_region0_socket0=("${dimm_socket0[@]:0:split0}")
    split_region1_socket0=("${dimm_socket0[@]:split0}")
fi

count=${#dimm_socket1[@]}
if [[ $count -eq 1 ]]; then
    # socket 1 only have one dimm
    split1=0
else
    split1=$((count / 2))
    split_region0_socket1=("${dimm_socket1[@]:0:split1}")
    split_region1_socket1=("${dimm_socket1[@]:split1}")
fi

# delete all namespace
delete_all_namespace

# deleta all pervious not-applied goal
$(ipmctl delete -goal)

# create region
if [[ $split0 -eq 0 ]]; then
    echo "socket0 only have one region: ${split_region0_socket0[@]}"
    $(ipmctl create -goal -dimm ${split_region0_socket0[@]} PersistentMemoryType=AppDirect)
else
    split00=$(join_array_with_comma "${split_region0_socket0[@]}")
    split10=$(join_array_with_comma "${split_region1_socket0[@]}")
    echo "split socket0 to two region: ${split00}, ${split10}"
    $(ipmctl create -goal -dimm ${split00} PersistentMemoryType=AppDirect)
    $(ipmctl create -goal -dimm ${split10} PersistentMemoryType=AppDirect)
fi

if [[ $split1 -eq 0 ]]; then
    echo "socket1 only have one region: ${split_region0_socket1[@]}"
    $(ipmctl create -goal -dimm ${split_region0_socket1[@]} PersistentMemoryType=AppDirect)
else
    split01=$(join_array_with_comma "${split_region0_socket1[@]}")
    split11=$(join_array_with_comma "${split_region1_socket1[@]}")
    echo "split socket1 to two region: ${split01}, ${split11}"
    $(ipmctl create -goal -dimm ${split01} PersistentMemoryType=AppDirect)
    $(ipmctl create -goal -dimm ${split11} PersistentMemoryType=AppDirect)
fi
