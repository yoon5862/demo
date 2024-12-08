#!/bin/sh
ON_TIME=${1}
OFF_TIME=${2}

while :
do
  stress --cpu 4 &
  sleep $ON_TIME
  pkill stress
  sleep $OFF_TIME
done

