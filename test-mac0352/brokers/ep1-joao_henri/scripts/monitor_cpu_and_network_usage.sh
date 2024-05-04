#!/bin/sh

start=$(date +%s)
for i in `seq 1 100`
do
  current=$(date +%s)
  echo "$(($current-$start)) $(docker stats ep1-joao-henri --format "{{.CPUPerc}}" --no-stream)" >> "$1"
  echo "$(($current-$start)) $(docker stats ep1-joao-henri --format "{{.NetIO}}" --no-stream)" >> "$2"
done
