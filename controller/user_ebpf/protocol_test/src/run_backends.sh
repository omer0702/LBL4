#!/bin/bash

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
echo "SCRIPT_DIR is $SCRIPT_DIR"
CLIENT_BIN="$SCRIPT_DIR/../../../../build/debug/controller/user_ebpf/protocol_test/protocol_client"

if [ ! -f "$CLIENT_BIN" ]; then
    echo "protocol_client not found at $CLIENT_BIN"
    exit 1
fi

sudo modprobe ipip

echo "adding VIP 10.0.0.100"
sudo ip addr add 10.0.0.100/32 dev lo 2>/dev/null

sudo ip link add tunl_lb type ipip local any remote 127.0.0.1 
sudo ip link set tunl_lb up
sudo sysctl -w net.ipv4.conf.tunl_lb.rp_filter=0
sudo sysctl -w net.ipv4.conf.tunl_lb.accept_local=1
sudo sysctl -w net.ipv4.conf.lo.rp_filter=0

sudo sysctl -w net.ipv4.conf.all.rp_filter=0

# IP="127.0.0.2"
# echo "configuring $IP"
# sudo ip addr add $IP/32 dev lo 2>/dev/null
# sudo "$CLIENT_BIN" $IP > "$SCRIPT_DIR/client_$i.log" 2>&1 &
# sleep 0.2
for i in {2..7}
do
    IP="127.0.0.$i"
    echo "configuring $IP"
    sudo ip addr add $IP/32 dev lo 2>/dev/null
    sudo "$CLIENT_BIN" $IP > "$SCRIPT_DIR/client_$i.log" 2>&1 &
    sleep 0.2
done

echo "running backend servers with DSR support"