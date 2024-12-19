#!/usr/bin/env python3

import subprocess
import os

CUR_DIR = os.path.abspath(os.path.dirname(__file__))
READ_DIR = os.path.join(CUR_DIR, "config", "read")
WRITE_DIR = os.path.join(CUR_DIR, "config", "write")

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
    if numa_info is not None:
        read_threads = numa_info['total_cpu_cores'] // 2
        write_threads = 8
        local_cpus = "cpus_allowed=" + ",".join([str(x) for x in numa_info['numa_nodes'][0]])
        remote_cpus = "cpus_allowed=" + ",".join([str(x) for x in numa_info['numa_nodes'][1]])
        f = open(os.path.join(READ_DIR, "local.fio"), "w")
        f.write("numjobs=%d\n" % read_threads)
        f.write(local_cpus)
        f.close()

        f = open(os.path.join(READ_DIR, "remote.fio"), "w")
        f.write("numjobs=%d\n" % read_threads)
        f.write(remote_cpus)
        f.close()

        f = open(os.path.join(WRITE_DIR, "local.fio"), "w")
        f.write("numjobs=%d\n" % write_threads)
        f.write(local_cpus)
        f.close()

        f = open(os.path.join(WRITE_DIR, "remote.fio"), "w")
        f.write("numjobs=%d\n" % write_threads)
        f.write(remote_cpus)
        f.close()