import pandas as pd

# 读取数据
df = pd.read_csv('performance-comparison-table', sep=r'\s+', engine='python')

# 按fs和workload分组，计算平均值
grouped = df.groupby(['fs', 'workload'])['recovery_time(ns)'].mean().reset_index()

# 将纳秒转换为毫秒
grouped['recovery_time(ms)'] = grouped['recovery_time(ns)'] / 1_000_000

print("\n\nPivot Table Format:")
print("=" * 60)
pivot = grouped.pivot(index='fs', columns='workload', values='recovery_time(ms)')
print(pivot)