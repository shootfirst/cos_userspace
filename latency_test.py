import subprocess
import threading
import time

def get_time(filename):
    with open(filename, 'r') as file:
        lines = file.readlines()
    index = len(lines) - 1
    value = int(lines[index].strip())
    return value

client_time_name = "client_time"
agent_time_name = "agent_time" 

result_name = "latency_result"

sum = 0
max = 0
min = 0

skip = 0
for i in range(0, 50):
    subprocess.run("sudo build/latency_load >> client_time", shell=True)
    result = get_time(client_time_name) - get_time(agent_time_name)
    with open(result_name, 'a') as file2:
        file2.write(str(result) + '\n')
    if result < 10 * 1000 or result > 100 * 1000:
        skip += 1
        continue
    sum += result
    if max < result:
        max = result
    if min == 0 or min > result :
        min = result

if result is not None:
    print(f"average latency: {sum / (50 - skip)}")
    print(f"max latency: {max}")
    print(f"min latency: {min}")
else:
    print("No second 'ok' found or the value below the second 'ok' is not a valid number.")

