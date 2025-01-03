import matplotlib.pyplot as plt
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
filename = "./performance-comparison-table-compare"  # 替换为您的文件路径
data = parse_data(filename)

# 按操作类型分组
ops_list = ["write", "randwrite", "read", "randread"]
fig, axs = plt.subplots(2, 2, figsize=(12, 10))
axs = axs.flatten()

# 绘制每个操作类型的子图
for i, ops in enumerate(ops_list):
    ax = axs[i]
    subset = data[data["ops"] == ops]

    for fs in subset["fs"].unique():
        fs_data = subset[subset["fs"] == fs]
        ax.plot(
            fs_data["numjobs"],
            fs_data["bandwidth"],
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
