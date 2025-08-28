#!/usr/bin/env python3

import subprocess
import os

CUR_DIR = os.path.abspath(os.path.dirname(__file__))
NUMA_CONFIG_DIR = os.path.join(CUR_DIR, "config/numa")

NUMA0_MNT_PATH = "/mnt/pmem0"
NUMA1_MNT_PATH = "/mnt/pmem1"

def numa_mem_info(numa_node):
    info="numa_cpu_nodes={}\nnuma_mem_policy=bind:{}\n".format(numa_node, numa_node)
    return info

def get_numactl_info():
    # 执行numactl --hardware命令
    result = subprocess.run(['numactl', '--hardware'], capture_output=True, text=True)
    
    if result.returncode != 0:
        print("Failed to execute numactl command.")
        return None
    
    output = result.stdout
    
    # 解析输出信息
    cpu_cores = []
    numa_nodes = {}
    
    for line in output.splitlines():
        if 'node' in line and 'cpus' in line:
            parts = line.split(':')
            node_id = int(parts[0].split()[1])
            cores = [int(x) for x in parts[1].strip().split()]
            numa_nodes[node_id] = cores
            cpu_cores.extend(cores)
    
    return {
        'numa_nodes': numa_nodes,
        'total_cpu_cores': len(cpu_cores) // 2,
    }

if __name__ == "__main__":
    # 获取NUMA信息
    numa_info = get_numactl_info()
    if len(numa_info['numa_nodes']) == 2:
        numa_0_cpus = "cpus_allowed=" + ",".join([str(x) for x in numa_info['numa_nodes'][0]])
        numa_1_cpus = "cpus_allowed=" + ",".join([str(x) for x in numa_info['numa_nodes'][1]])

        f = open(os.path.join(NUMA_CONFIG_DIR, "numa0.fio"), "w")
        info= numa_mem_info(0)
        f.write(info)
        f.write(numa_0_cpus)
        f.write("\n")
        f.write("directory=" + NUMA0_MNT_PATH)
        f.write("\n")
        f.close()

        f = open(os.path.join(NUMA_CONFIG_DIR, "numa1.fio"), "w")
        info= numa_mem_info(1)
        f.write(info)
        f.write(numa_1_cpus)
        f.write("\n")
        f.write("directory=" + NUMA1_MNT_PATH)
        f.write("\n")
        f.close()
    else:
        print("Only one NUMA node is available.")
        exit(-1)