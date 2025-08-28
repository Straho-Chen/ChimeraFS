#!/bin/bash

# 使用 ps aux | grep 'pcm' 来查找所有名为 pcm 的进程
# grep -v grep 用来排除 grep 进程本身
processes=$(ps aux | grep 'pcm' | grep -v grep)

# 如果没有找到任何进程，则退出脚本
if [ -z "$processes" ]; then
	echo "No processes named 'pcm' found."
	exit 0
fi

# 初始化一个空数组来存储 PIDs
pids=()

# 逐行读取进程信息，并提取 PID
while IFS= read -r line; do
	pid=$(echo "$line" | awk '{print $2}')
	pids+=("$pid")
done <<<"$processes"

# 检查是否有进程需要终止
if [ ${#pids[@]} -eq 0 ]; then
	echo "No processes named 'pcm' found."
else
	# 显示将要终止的进程列表
	echo "Processes to be killed:"
	printf '%s\n' "${pids[@]}"

	# 终止进程
	for pid in "${pids[@]}"; do
		kill -SIGTERM "$pid"
		echo "Terminated process with PID: $pid"
	done

	# 检查进程是否已成功终止
	for pid in "${pids[@]}"; do
		if ps aux | grep -qw "pcm"; then
			echo "Process with PID $pid did not terminate gracefully. Sending SIGKILL..."
			kill -SIGKILL "$pid"
			echo "Forced termination of process with PID: $pid"
		else
			echo "Process with PID $pid terminated successfully."
		fi
	done
fi
