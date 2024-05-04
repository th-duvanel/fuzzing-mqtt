import sys
import os
import random

if len(sys.argv) < 5:
   print("Usage: <host_address> <number_of_pub_clients> <number_of_pub_topics> <size_of_messages>") 
   exit(1)

topics = []
with open('topics.txt', 'r') as f:
    topics = f.readlines()
    topics = [el.rstrip() for el in topics]

host_address = sys.argv[1]
number_of_pub_clients = int(sys.argv[2])
number_of_pub_topics = int(sys.argv[3])
size_of_messages = int(sys.argv[4])

string = "".join(['a' for _ in range(0, size_of_messages)])
for i in range(number_of_pub_clients):
    random_int = random.randint(0, number_of_pub_topics-1)
    topic = topics[random_int]
    os.system(f'mosquitto_pub -d -h {host_address} -t {topic} -m {string} -i mosquitto_pub_{i} &')
