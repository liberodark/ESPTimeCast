#!/usr/bin/env bash

URL="http://192.168.0.216/webhook"
KEY="SECRET123"

cpu_usage() {
    awk '{u=$2+$4; t=$2+$4+$5; if (NR==1){u1=u; t1=t;} else print int((u-u1)*100/(t-t1))}' \
        <(grep 'cpu ' /proc/stat; sleep 1; grep 'cpu ' /proc/stat)
}

while true; do
    CPU=$(cpu_usage)
    RAM=$(awk '/MemTotal/{t=$2} /MemAvailable/{a=$2} END{print int(100*(t-a)/t)}' /proc/meminfo)
    DISK=$(df / | awk 'END{print int($3*100/$2)}')

    echo "CPU: ${CPU}%, RAM: ${RAM}%, DISK: ${DISK}%"

    curl -X POST $URL -d "key=$KEY" -d "message=CPU ${CPU}%" -d "priority=0"
    sleep 3
    curl -X POST $URL -d "key=$KEY" -d "message=RAM ${RAM}%" -d "priority=0"
    sleep 3
    curl -X POST $URL -d "key=$KEY" -d "message=DISK ${DISK}%" -d "priority=0"

    sleep 300
done
