#!/usr/bin/bash

ipmctl dump -destination config.txt -system -config

awk -F',' 'BEGIN{OFS=","} NR==1{sub(/^#/, "", $1); print $1, $2, $7} NR>1{print $1, $2, $7}' config.txt >ipmctl_dump.csv

ndctl list -D | jq -r '.[] | [.handle, .dev] | @csv' >ndctl_map.csv

awk -F',' '
  # 读取第一个文件（ndctl_map.csv），构建 handle 到 dev 的映射
  NR==FNR {
    gsub(/"/, "", $2);  # 移除 dev 的引号（如果存在）
    handle[$1] = $2;
    next
  }
  
  # 处理第二个文件（ipmctl_dump.csv）
  {
    if (FNR == 1) {
      # 添加表头 "dev"
      print $0 ",dev";
    } else {
      # 根据 DimmHandle 查找 dev
      dimm_handle = $2;
      dev = handle[dimm_handle];
      # 输出原有行 + dev
      print $0 "," dev;
    }
  }
' ndctl_map.csv ipmctl_dump.csv >output.csv
