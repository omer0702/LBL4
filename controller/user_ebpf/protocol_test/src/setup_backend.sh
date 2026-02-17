#!/bin/bash

VIP="10.0.0.100"
BACKEND_IP=$1

if [ $2 == "up"]; then
    sudo modprobe ipip
    sudo ip link add tunl_lb type ipip local $BACKEND_IP remote $VIP
    sudo ip link set tunl_lb up
    sudo ip addr add $VIP/32 dev lo
    echo "Backend DSR configuration is up"
elif [ $2 == "down"]; then
    sudo ip addr del $VIP/32 dev lo
    sudo ip link del tunl_lb
    echo "Backend DSR configuration is down"
else
    echo "Usage: $0 <backend_ip> <up|down>"
fi