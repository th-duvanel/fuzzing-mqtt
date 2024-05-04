#!/bin/sh

echo "There will be $1 clients"
for i in `seq 1 $1`
do
  mosquitto_sub -h 172.17.0.2 -t "topico" &
  echo "Created client number $i"
done

while true; do
  echo "[$(date +%s)]: Checkpoint!"
  sleep 10
done