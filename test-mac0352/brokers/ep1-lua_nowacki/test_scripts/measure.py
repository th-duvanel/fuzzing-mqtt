import os
import psutil
import sys
import time

if len(sys.argv) < 3:
    print("Usage: <result_filename> <experiment_number>") 
    exit(1)

filename = sys.argv[1]
experiment_number = sys.argv[2]

name = 'mqtt_broker.out'
initial_timestamp = time.time()
initial_network_stats = psutil.net_io_counters()
initial_bytes_sent = initial_network_stats.bytes_sent
initial_bytes_recv = initial_network_stats.bytes_recv

process = 0

for p in psutil.process_iter(['name']):
    if p.info['name'] == name:
        process = p

if process == 0:
    print("mqtt_broker.out process not found")
    exit(1)

# print(process.connections())
with open(f'{filename}_{experiment_number}.csv', 'w') as f:
    f.write(f"timestamp, "
            f"cpu_percent, "
            f"bytes_sent, "
            f"bytes_recv, " 
            f"num_connection\n")
    print(f"timestamp_{experiment_number}, "
            f"cpu_percent_{experiment_number}, "
            f"bytes_sent_{experiment_number}, "
            f"bytes_recv_{experiment_number}, " 
            f"num_connection_{experiment_number}")
    while True:
        net_stats = psutil.net_io_counters()
        bytes_sent = net_stats.bytes_sent - initial_bytes_sent
        bytes_recv = net_stats.bytes_recv - initial_bytes_recv
        num_connections = len(process.connections())
        cpu_percent = process.cpu_percent()
        print(f"{(time.time() - initial_timestamp):.1f}, {cpu_percent}, "
              f"{bytes_sent}, {bytes_recv}, {num_connections}")
        f.write(f"{(time.time() - initial_timestamp):.1f}, {cpu_percent}, "
                f"{bytes_sent}, {bytes_recv}, {num_connections}\n")
        time.sleep(0.1)
	
