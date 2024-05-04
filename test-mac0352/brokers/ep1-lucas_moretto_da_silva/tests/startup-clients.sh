#!/bin/bash

SUBSCRIBERS_PIDS=()
CLIENTS_NUM=$1

start_subscribers() {
  for client in $(seq 1 "$CLIENTS_NUM"); do
    mosquitto_sub -t test &
    SUBSCRIBERS_PIDS+=($!)
  done
}

start_publishers() {
  for client in $(seq 1 "$CLIENTS_NUM"); do
    mosquitto_pub -t test -m "test message"
  done
}

kill_subscribers() {
  for pid in ${SUBSCRIBERS_PIDS[@]}; do
    kill "$pid"
  done
}

main() {
  start_subscribers
  start_publishers
  kill_subscribers
}

main
