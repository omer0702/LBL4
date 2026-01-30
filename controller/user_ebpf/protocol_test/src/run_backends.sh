#!/bin/bash

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
echo "SCRIPT_DIR is $SCRIPT_DIR"
CLIENT_BIN="$SCRIPT_DIR/../../../../build/debug/controller/user_ebpf/protocol_test/protocol_client"
if [ ! -f "$CLIENT_BIN" ]; then
    echo "protocol_client not found at $CLIENT_BIN"
    exit 1
fi

echo "adding VIP 10.0.0.100"
sudo ip addr add 10.0.0.100/32 dev lo 2>/dev/null



for i in {2..7}
do
    IP="127.0.0.$i"
    echo "configuring $IP"
    sudo ip addr add $IP/32 dev lo 2>/dev/null
    sudo "$CLIENT_BIN" $IP > "$SCRIPT_DIR/client_$i.log" 2>&1 &
    sleep 0.2
done

echo "running backend servers"