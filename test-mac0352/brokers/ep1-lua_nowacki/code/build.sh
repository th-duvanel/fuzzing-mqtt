#!/bin/bash

code="$PWD"
opts="-O2"
cd ../build > /dev/null
gcc $opts $code/main.c -o mqtt_broker.out
cd $code > /dev/null
