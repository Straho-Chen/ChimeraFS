import os

# 获取当前脚本目录和父目录
cur_dir = os.path.abspath(os.path.dirname(__file__))

blksize="2m"

def size_in_bytes(blksize_str):
    unit = blksize_str[-1]
    size = int(blksize_str[:-1])
    if unit == "k":
        return size * 1024
    elif unit == "m":
        return size * 1024 * 1024
    elif unit == "g":
        return size * 1024 * 1024 * 1024
    else:
        raise ValueError(f"Invalid unit: {unit}")

# 数据文件路径
input_file_path = os.path.join(cur_dir, "perf")
out_name=f"perf-saved-{blksize}"
output_file_path = os.path.join(cur_dir, f"{out_name}")

with open(input_file_path, 'r') as infile, open(output_file_path, 'w') as outfile:
    for line in infile:
        if line.startswith("fs ops filesz blksz numjobs bandwidth(MiB/s)"):
            outfile.write(line)
        
        if line.strip() and not line.startswith("fs ops filesz blksz numjobs bandwidth(MiB/s)"):
            parts = line.split()
            if len(parts) > 4 and parts[3] == str(size_in_bytes(blksize)):
                outfile.write(line)

print(f"Cleaned data saved to {output_file_path}")