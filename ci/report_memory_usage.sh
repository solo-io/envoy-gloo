#!/bin/bash

SLEEP_TIME=10 # seconds
# LOOP_CYCLES=120 # 20 minutes total

while kill -0 $1 ; do
    echo "$1 still seems to be running"
    ps -ef | grep -w $1
    free -m
    sleep ${SLEEP_TIME}
done