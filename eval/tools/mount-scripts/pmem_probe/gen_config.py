import argparse
import csv
import subprocess

def check_non_interleaved():
    try:
        result = subprocess.run(
            ['ipmctl', 'show', '-region'],
            capture_output=True,
            text=True,
            check=True
        )
        return "NotInterleaved" in result.stdout
    except subprocess.CalledProcessError as e:
        print(f"Failed to check interleave status: {str(e)}")
        return False
    except FileNotFoundError:
        print("ipmctl command not found. Ensure Intel Optane DC Persistent Memory Tools are installed")
        return False

def probe_numa():
    try:
        result = subprocess.run(
            ['./numa_probe.sh'],
            capture_output=True,
            text=True,
            check=True
        )
        return result.stdout
    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"Failed to probe pm topology: {str(e)}")
    except FileNotFoundError:
        raise RuntimeError("miss numa_probe.sh, please check the path")

def generate_pm_config(socket_num):
    non_interleaved = check_non_interleaved()

    pm_config = []
    
    if non_interleaved:
        entries = []
        probe_numa()
        with open('output.csv', 'r') as csvfile:
            csvreader = csv.reader(csvfile)
            next(csvreader)

            for row in csvreader:
                if len(row) < 4:
                    continue
                socket_id = int(row[0])
                dev = row[3]
                entries.append((socket_id, dev))
        print("Detected non-interleaved mode")
        socket_dimm_counts = {}
        for socket_id, dev in entries:
            if socket_id < socket_num:
                socket_dimm_counts[socket_id] = socket_dimm_counts.get(socket_id, 0) + 1
        
        available_sockets = len(socket_dimm_counts)
        if available_sockets < socket_num:
            raise ValueError(f"Only {available_sockets} sockets available, but requested {socket_num}")

        for socket_id, dev in entries:
            if socket_id < socket_num:
                pm_index = dev.replace("nmem", "")
                pm_config.append(f"/dev/pmem{pm_index} {socket_id}")
    else:
        print("Detected interleaved mode")
        for i in range(socket_num):
            pm_config.append(f"/dev/pmem{i} {i}")

    with open('pm_config.txt', 'w') as f:
        for line in pm_config:
            f.write(line + "\n")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Generate PM configuration file')
    parser.add_argument('socket_num', type=int, 
                       help='Number of sockets to initialize (0-based)')
    
    try:
        args = parser.parse_args()
        generate_pm_config(args.socket_num)
        print(f"pm_config.txt generated successfully for {args.socket_num} sockets")
    except Exception as e:
        print(f"Error: {str(e)}")
        print("Possible solutions:")
        print("1. Run with sudo privileges")
        print("2. Install ipmctl: apt install ipmctl")
        print("3. Check output.csv format")