#!/bin/bash

echo "cleaning up..."
sudo ip netns del client-ns 2>/dev/null
sudo ip link del veth-client 2>/dev/null

for i in {2..7}; do
    NS="ns$i"
    VETH="veth$i"

    sudo ip netns del $NS 2>/dev/null
    sudo ip link del $VETH 2>/dev/null
done

sudo ip addr del 10.0.0.100/32 dev lo 2>/dev/null
sudo ip route del 10.0.0.100 dev br-lb
sudo ip link set br-lb down 2>/dev/null
sudo ip link del br-lb 2>/dev/null

sudo pkill -f protocol_client 

echo "cleanup done."