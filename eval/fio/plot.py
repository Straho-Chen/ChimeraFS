import os
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

# 解析文件并将数据组织为DataFrame
def parse_data(filename):
    # 用于存储解析结果
    data = []
    
    with open(filename, 'r') as file:
        lines = file.readlines()
        for line in lines[1:]:
            if line.strip():  # 跳过空行
                parts = line.split()
                if len(parts) == 6:  # 确保行包含正确数量的列
                    fs, ops, filesz, blksz, numjobs, bandwidth = parts
                    data.append({
                        "fs": fs,
                        "ops": ops,
                        "filesz": int(filesz),
                        "blksz": int(blksz),
                        "numjobs": int(numjobs),
                        "bandwidth": float(bandwidth) / 1024
                    })
    
    return pd.DataFrame(data)

# 读取数据
filename = "performance-comparison-table-compare"  # 替换为您的文件路径
data = parse_data(filename)

cur_dir = os.path.abspath(os.path.dirname(__file__))
parent_dir = os.path.dirname(cur_dir)
raw_perf_file = os.path.join(parent_dir, "raw-pm-perf", "perf-saved")

raw_data = parse_data(raw_perf_file)

# 找到raw_data中每个操作的最大带宽，以及对应的numjobs
max_bandwidth = {ops: 0 for ops in set(raw_data["ops"])}
max_numjobs = {ops: 0 for ops in set(raw_data["ops"])}
for ops in set(raw_data["ops"]):
    subset = raw_data[raw_data["ops"] == ops]
    max_bandwidth[ops] = subset["bandwidth"].max()
    max_numjobs[ops] = subset[subset["bandwidth"] == max_bandwidth[ops]]["numjobs"].values[0]

new_raw_data = []
# 从raw_data拷贝每个操作的最大带宽对应的numjobs之前的数据
for ops in set(raw_data["ops"]):
    subset = raw_data[raw_data["ops"] == ops]
    max_numjob = max_numjobs[ops]
    new_raw_data.extend(subset[subset["numjobs"] <= max_numjob].to_dict("records"))

# 扩展new_raw_data中每个操作的最大带宽对应的numjobs之后的数据
for ops in set(raw_data["ops"]):
    subset = raw_data[raw_data["ops"] == ops]
    max_numjob = max_numjobs[ops]
    max_bandwidth_val = max_bandwidth[ops]
    for numjobs in range(max_numjob + 1, 65, 1):
        new_raw_data.append({
            "fs": subset["fs"].values[0],
            "ops": ops,
            "filesz": subset["filesz"].values[0],
            "blksz": subset["blksz"].values[0],
            "numjobs": numjobs,
            "bandwidth": max_bandwidth_val
        })

# 根据data中的numjobs值，将new_raw_data中不在data的numjobs值的数据删除
data_numjobs = set(data["numjobs"])
new_raw_data = [d for d in new_raw_data if d["numjobs"] in data_numjobs]

# 将raw_data中的数据合并到data中
data = pd.concat([data, pd.DataFrame(new_raw_data)], ignore_index=True)

# 按操作类型分组
fs_list = sorted(set(data["fs"]))
ops_list = sorted(set(data["ops"]))
numjobs_list = sorted(set(data["numjobs"]))
avg_bandwidth={fs: {op: [] for op in ops_list} for fs in fs_list}

for fs in fs_list:
    for op in ops_list:
        for nj in numjobs_list:
            bandwidth_values = [
                bw for f, o, nj_val, bw in zip(data["fs"], data["ops"], data["numjobs"], data["bandwidth"])
                if f == fs and o == op and nj_val == nj
            ]
            avg_bandwidth[fs][op].append(np.mean(bandwidth_values))

fig, axs = plt.subplots(2, 2, figsize=(12, 10))
axs = axs.flatten()

# 绘制每个操作类型的子图
for i, ops in enumerate(ops_list):
    ax = axs[i]
    subset = data[data["ops"] == ops]

    for fs in subset["fs"].unique():
        fs_data = subset[subset["fs"] == fs]
        ax.plot(
            numjobs_list,
            avg_bandwidth[fs][ops],
            label=fs,
            marker='o'
        )
    
    ax.set_title(ops)
    ax.set_xlabel("Number of Threads")
    ax.set_ylabel("Bandwidth (GiB/s)")
    ax.legend()
    ax.grid(True)

plt.tight_layout()
output_pdf = "fio.pdf"
plt.savefig(output_pdf, format="pdf")
