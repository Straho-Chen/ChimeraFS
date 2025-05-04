import matplotlib.pyplot as plt
import pandas as pd
import numpy as np

# Example data loading (replace with actual file paths)
frag_files = {
    "odinfs": "performance-comparison-table-odinfs-frag",
    "parfs": "performance-comparison-table-parfs-frag",
    "parfs-new8": "performance-comparison-table-parfs-frag-new8",
    "parfs-noninterleaved": "performance-comparison-table-parfs-frag-noninterleaved",
    "parfs_no_entry": "performance-comparison-table-parfs-no-entry-append-frag",
}

# Load data into a dictionary of DataFrames
data = {}
for frag, file in frag_files.items():
    data[frag] = pd.read_csv(file, delim_whitespace=True)

# Define blksize values
blksizes = [4096, 8192, 16384, 32768]

# Create subplots
fig, axes = plt.subplots(2, 2, figsize=(14, 10))
axes = axes.flatten()  # Flatten to 1D array for easier indexing

# Plot data for each blksize
for i, blksize in enumerate(blksizes):
    ax = axes[i]
    for frag, df in data.items():
        # Filter data for the current blksize
        blksize_data = df[df["blksz"] == blksize]
        # Plot bandwidth vs numjobs
        ax.plot(blksize_data["numjobs"], blksize_data["bandwidth(MiB/s)"], label=frag)
    
    # Customize the subplot
    ax.set_title(f"Block Size: {blksize} bytes")
    ax.set_xlabel("Number of Jobs (numjobs)")
    ax.set_ylabel("Bandwidth (MiB/s)")
    ax.legend()
    ax.grid(True)

# Adjust layout and display
plt.tight_layout()
output_pdf = "fio-frag.pdf"
plt.savefig(output_pdf, format="pdf")