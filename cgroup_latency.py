import subprocess
import threading
import time

def get_client_time(filename):
    with open(filename, 'r') as file:
        lines = file.readlines()
    index = len(lines) - 1
    value = float(lines[index].strip())
    return value

client_time_name = "cgroup_time"

sum = 0

for i in range(0, 20):
    subprocess.run("sudo build/cgroup_load 2>> cgroup_time", shell=True)
    result = get_client_time(client_time_name)
    sum += result

if result is not None:
    print(f"average runtime: {sum / 20}")
else:
    print("No second 'ok' found or the value below the second 'ok' is not a valid number.")

