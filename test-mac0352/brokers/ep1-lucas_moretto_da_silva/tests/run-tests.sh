#!/bin/bash

TEST_FOLDER=tests
TEST_SCENARIOS=(0 100 1000)
TEST_REPEAT=1

prepare_test_folder() {
  mkdir -p "$TEST_FOLDER"

  for TEST_SIZE in ${TEST_SCENARIOS[@]}; do
    TEST_PATH="$TEST_FOLDER/$TEST_SIZE"
    rm -rf "$TEST_PATH"
    mkdir "$TEST_PATH"
  done
}

print_metric() {
  docker stats --no-stream --format "{{.CPUPerc}} | {{.NetIO}}" "$1" >> "$TEST_FOLDER"/"$2"/"$3"
}

read_metrics() {
  CONTAINER_ID=$1
  SCENARIO=$2
  TEST_PID=$3
  TEST_INDEX=$4
  TEST_FILENAME=results-$TEST_INDEX.txt

  rm -f "$TEST_FOLDER"/"$SCENARIO"/"$TEST_FILENAME"
  touch "$TEST_FOLDER"/"$SCENARIO"/"$TEST_FILENAME"

  print_metric "$CONTAINER_ID" "$SCENARIO" "$TEST_FILENAME"

  while ps -p "$TEST_PID" > /dev/null; do
    print_metric "$CONTAINER_ID" "$SCENARIO" "$TEST_FILENAME"
  done
}

run_test() {
  docker-compose -f "../docker-compose.yml" up -d
  CONTAINER_ID=$(docker ps -q)
  sleep 1;

  bash startup-clients.sh "$2" &
  PID=$!

  read_metrics "$CONTAINER_ID" "$2" "$PID" "$1"

  docker-compose down
}

main() {
  prepare_test_folder

  for TEST_NUM in $(seq 1 $TEST_REPEAT); do
    for SCENARIO in ${TEST_SCENARIOS[@]}; do
      run_test "$TEST_NUM" "$SCENARIO"
    done
  done
}

main
