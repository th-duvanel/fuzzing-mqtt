#!/bin/sh

for i in `seq 1 $1`
do
  mosquitto_pub -h 172.17.0.2 -t "topico" -m "mensagem" &
  echo "Published message number $i"
  sleep 1
done

