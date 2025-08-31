# ChimeraFS Artifacts Evaluation

This repository contains the artifacts for the paper "Fast and Parallelized Crash Consistency with Opportunistic Order Elimination" (ChimeraFS) accepted by EuroSys'26.

- [ChimeraFS Artifacts Evaluation](#chimerafs-artifacts-evaluation)
  - [1. Artifact Overview](#1-artifact-overview)
  - [2. Primary Evaluation](#2-primary-evaluation)
    - [2.1 Quick Start](#21-quick-start)
      - [2.1.1 Get source code](#211-get-source-code)
      - [2.1.2 Install dependencies](#212-install-dependencies)
      - [2.1.3 One-click run](#213-one-click-run)
    - [2.2 Step-by-Step Reproducing](#22-step-by-step-reproducing)
      - [2.2.1 Output Results](#221-output-results)
      - [2.2.2 Reproducing Figures](#222-reproducing-figures)
      - [2.2.3 Reproducing Tables](#223-reproducing-tables)

## 1. Artifact Overview

Our artifacts involve one platforms: a server with dual-socket (8 in total) Optane DCPMM that conducts primary performance evaluation.  For reviewer's convenience, we have provided the isolated environments for this platform. 

**NOTE: The overall experiments can take around one day or less, please run experiments in the tmux or other terminal multiplexers to avoid losing AE progress.**

**Organization of repositories.** ChimeraFS project is composed of two repositories, which are all available. We now briefly introduce the usage of each repository as follows:

- ChimeraFS source code: https://github.com/Straho-Chen/parfs-fs-repo.
- Evaluation tools, scripts & other baseline fs source code: https://github.com/Straho-Chen/ChimeraFS.

## 2. Primary Evaluation

### 2.1 Quick Start

#### 2.1.1 Get source code

We put thr source code of ChimeraFS as a submodule in the repository. You can clone all source code by the following command:

```bash
git clone --recurse-submodules https://github.com/Straho-Chen/ChimeraFS.git
```

#### 2.1.2 Install dependencies

- **Kernel**: Linux kernel 6.6.32. We contian the kernel source code in the linux directory and provide a Makefile to help you build the kernel. Please follow the commands below to build and install the kernel.

```bash
# under the root directory of the repo
cd linux
mv config-chimerafs .config
cd mymake
# modify the path in the Makefile
# BACKUP_DIR=<where to place the backup dir, usually put it in tmpfs>
# MODULES_INSTALL_PREFIX=<where to place the modules output dir>
# KERNEL_INSTALL_PREFIX=<where to place the kernel output dir>
make build-deb
```

If you want to use your own kernel, please make sure that fs dir contains the linkage to target fs (i.e., ChimeraFS, PMFS, NOVA, and Odinfs, under fs directory). There might be some compatibility issues if you use other kernel versions.

- **Hardware**: At least one PM equipped (>64\,GiB), which can be either real PM or emulated PM (via `memmap` or QEMU `-device nvdimm` options). PM should be configured in `fsdax` mode.

- **Software**: We include our benchmarks into the repo and provide a `dep.sh` script to install the dependencies. The user should run this script to install the dependencies:

After installing the dependencies, enter the `eval/benchmark` directory and run `make` to build the benchmarks.

#### 2.1.3 One-click run

All the corresponding test scripts and results for reference are included in the `eval` directory. After setting up the experimental environment, the user changes the directory to where the scripts stand and runs the script by typing `./test.sh`. Note that the user is required to run in `sudo` mode. Also note that we have provided a one-click script `run.sh` in the root directory of "eval", which can automatically run all the experiments involved in the paper. The specific reproducing steps of each experiment are described in the following subsections.

### 2.2 Step-by-Step Reproducing

#### 2.2.1 Output Results

We focus on introducing the files in the directories with prefixes "FIG" and "TABLE". The raw output files are mostly named with the prefix "performance-comparison-table", which can be obtained by running `bash test.sh`. `plot.ipynb` script is provided for drawing figures, respectively.

Generally, typical workflows for reproducing figures and tables are presented as follows.

```bash
# General workflow to reproducing tables

cd <Your directory>/eval/xx
# Step 1. Run Experiment
bash ./test.sh
# Step 3. Building Table
ipython plot/ipynb
```

#### 2.2.2 Reproducing Figures

**Figure 1: Underutilized PM I/O parallelism of existing PM file systems.** The script is located at `eval/moti/perf-fio.sh`. The output performance results are presented in `performance-comparison-table-perf`. The user can use `plot-perf.ipynb` to plot the figure: `FIG-MOTIVATIOIN.pdf`.

**Figure 2: Normalized I/O time breakdown.** The script is located at `eval/moti/meta-fio.sh`. The output performance results are presented in `performance-comparison-table-xx` ("xx" is the test fs name). The user can use `plot-breakdown.ipynb` to plot the figure: `FIG-Motivation-PerfBreakDown.pdf`. This plot script will generate a collection of test results, which is named `performance-comparison-table-combine`.

**Figure 4: Order elimination performance model.** The script is located at `eval/dele_size/test.sh`. The output performance results are presented in `performance-comparison-table`. The user can use `plot.ipynb` to plot the figure: `FIG-IO-THRESHOLD.pdf`.

**Figure 6: Single-threaded I/O performance comparison.** The script is located at `eval/blk_size/test.sh`. The output performance results are presented in `performance-comparison-table`. The user can use `plot.ipynb` to plot the figure: `FIG-BS.pdf`.

**Figure 7: Concurrency performance comparison.** The script is located at `eval/fio/test.sh`. The output performance results are presented in `performance-comparison-table`. The user can use `plot.ipynb` to plot the figure: `FIG-MT.pdf`.

**Figure 8: Filebench performance comparison.** The script is located at `eval/filebench/test.sh`. The output performance results are presented in `performance-comparison-table`. The user can use `plot.ipynb` to plot the figure: `Filebench.pdf`.

Note that WineFS might fail and cause a kernel panic (in an unknown probability) during multi-threaded Filebench tests. Please manually exclude WineFS from `test.sh` script, and repeat running the WineFS using another stand-alone script, such as `test-winefs.sh` provided in our repository.

**Figure 9: YCSB on LevelDB performance comparison.** The script is located at `eval/leveldb/test.sh`. The output performance results are presented in `performance-comparison-table`. The user can use `plot.ipynb` to plot the figure: `Leveldb.pdf`.

Note that Odinfs fail (in an unknown probability) during LevelDB tests. We remove it from test script.

**Figure 10: ChimeraFS write path breakdown.** The script is located at `eval/breakdown/test.sh`. The output performance results are presented in `performance-comparison-table`. The user can use `plot.ipynb` to plot the figure: `Leveldb.pdf`.

**Figure 11: The efficiency of the order elimination controller.** The script is located at `eval/opt_append/test.sh`. The output performance results are presented in `performance-comparison-table`. The user can use `plot.ipynb` to plot the figure: `FIO-OPT-APPEND.pdf`.

**Figure 12: The efficiency of topology-aware I/O dispatcher.** The script is located at `eval/io_dispatch/test.sh`. The output performance results are presented in `performance-comparison-table`. The user can use `plot.ipynb` to plot the figure: `FIO-IO-DISPATCH.pdf`.

**Figure 13: I/O performance comparison atop a single PM.** The script is located at `eval/spm/test.sh`. The output performance results are presented in `performance-comparison-table`. The user can use `plot.ipynb` to plot the figure: `FIO-SPM.pdf`.

#### 2.2.3 Reproducing Tables

**Table 3: Failure recovery time.** The corresponding script for Table 3 is presented in `eval/recovery_time/test.sh`. To reproduce this table, one can follow the commands:

```bash
cd eval/recovery_time/
bash ./test.sh
python3 table.py > table
```

The recovery time may appear to be unstable due to the timing tools. We expect the results to show that "all_scan_recovery" (which is named Naive in paper) is slowest, and "latest_trans_scan_recovery" (which is named Latest-verify in paper) is the second slowest. The result of "ckpt" (which is our approach) and "no-scan" (which represent NOVA) should be faster than the other two, and "ckpt" should be a little slower than "no-scan".
