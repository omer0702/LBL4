#!/bin/bash

echo "stopping backend servers"
sudo pkill protocol_client

echo "removing VIP address"
sudo ip addr del 10.0.0.100/32 dev lo 2>/dev/null

for i in {2..7}
do
    sudo ip addr del 127.0.0.$i/32 dev lo 2>/dev/null
done

echo "cleanup complete"