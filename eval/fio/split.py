import os

# 获取当前脚本目录和父目录
cur_dir = os.path.abspath(os.path.dirname(__file__))

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

def split_test_result(file_name, blksize):
    input_file_path = os.path.join(cur_dir, file_name)
    output_file_path=f"{input_file_path}-{blksize}"
    with open(input_file_path, 'r') as infile, open(output_file_path, 'w') as outfile:
        for line in infile:
            if line.startswith("fs ops filesz blksz numjobs bandwidth(MiB/s)"):
                outfile.write(line)

            if line.strip() and not line.startswith("fs ops filesz blksz numjobs bandwidth(MiB/s)"):
                parts = line.split()
                if len(parts) > 4 and parts[3] == str(size_in_bytes(blksize)):
                    outfile.write(line)
    print(f"Cleaned data saved to {output_file_path}")
    
if __name__ == "__main__":
    blksize = ["4k", "8k", "16k", "32k"]
    for bs in blksize:
        print(f"Processing block size: {bs}")
        # 调用函数
        split_test_result("performance-comparison-table-restore1", bs)
    print("Data splitting completed.")