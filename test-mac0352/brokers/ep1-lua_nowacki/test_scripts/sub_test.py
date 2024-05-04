import sys
import os
import random

if len(sys.argv) < 4:
   print("Usage: <host_address> <number_of_sub_clients> <number_of_sub_topics>") 
   exit(1)

topics = []
with open('topics.txt', 'r') as f:
    topics = f.readlines()
    topics = [el.rstrip() for el in topics]

host_address = sys.argv[1]
number_of_sub_clients = int(sys.argv[2])
number_of_sub_topics = int(sys.argv[3])

for i in range(number_of_sub_clients):
    random_int = random.randint(0, number_of_sub_topics-1)
    topic = topics[random_int]
    os.system(f'mosquitto_sub -d -h {host_address} -t {topic} -i mosquitto_sub_{i} &')

