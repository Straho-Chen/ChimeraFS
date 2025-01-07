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

theoretical_bandwidth = {
    "read": 49.7,
    "write": 10.7,
    "randread": 51.1,
    "randwrite": 10.7
}

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
    
    ax.axhline(y=theoretical_bandwidth[ops], color='r', linestyle='--', label='Theoretical Performance')
    ax.set_title(ops)
    ax.set_xlabel("Number of Threads")
    ax.set_ylabel("Bandwidth (GiB/s)")
    ax.legend()
    ax.grid(True)

plt.tight_layout()
output_pdf = "fio.pdf"
plt.savefig(output_pdf, format="pdf")
