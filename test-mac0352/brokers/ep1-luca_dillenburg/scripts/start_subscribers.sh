#!/bin/bash

# cd /opt/homebrew/Cellar/mosquitto/2.0.14/bin
cd /snap/bin

echo "Starting $1 subscribers in $2 topics"

i=1
while [ "$i" -le "$1" ]; do
    m="$(($i % $2))"
    echo "Started mosquitto_sub in topic 'topic_$m'"
    ./mosquitto_sub -t "topic_$m" -h 10.0.0.101 &
    i=$((i + 1))
done

