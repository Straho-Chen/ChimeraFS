#!/usr/bin/bash

parfs_branch=(idel parfs-no-opt-append append_csum_whole_block append_csum_partial_block append_no_csum all_scan_recovery latest_trans_scan_recovery ckpt no_scan low-thread idel-low-thread optfs)

for branch in "${parfs_branch[@]}"; do
    if [ -d "$branch" ]; then
        echo "Deleting folder: $branch"
        rm -rf "$branch"
    else
        echo "Folder not found: $branch"
    fi
done