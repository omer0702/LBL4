#!/bin/bash

# --- הגדרות כלליות ---
VIP="10.0.0.100"
BRIDGEIP="172.16.0.1"
BRIDGE_NAME="br-lb"
SCRIPT_DIR="$(dirname "$(realpath "$0")")"
CLIENT_BIN="$SCRIPT_DIR/../../../../build/debug/controller/user_ebpf/protocol_test/protocol_client"

# ניקוי VIP ישן מכל מקום
sudo ip addr del 10.0.0.100/32 dev lo 2>/dev/null
sudo ip addr del 10.0.0.100/32 dev br-lb 2>/dev/null

# ניקוי שאריות מהרצה קודמת
sudo ip link set $BRIDGE_NAME down 2>/dev/null
sudo ip link del $BRIDGE_NAME 2>/dev/null
for i in {2..7}; do sudo ip netns del "ns$i" 2>/dev/null; done

# 1. יצירת ה-Bridge
sudo ip link add $BRIDGE_NAME type bridge
sudo ip addr add $BRIDGEIP/24 dev $BRIDGE_NAME
sudo ip link set $BRIDGE_NAME up

# 2. ביטול חסימות Firewall על ה-Bridge (קריטי ל-IPIP)
sudo modprobe br_netfilter
sudo sysctl -w net.bridge.bridge-nf-call-iptables=0
sudo sysctl -w net.bridge.bridge-nf-call-arptables=0

# 3. הגדרות IP ב-Host לטובת ה-DSR
# sudo ip addr add 10.0.0.100/32 dev lo 2>/dev/null
# sudo ip addr add 192.168.1.50/24 dev lo 2>/dev/null
sudo sysctl -w net.ipv4.ip_forward=1
sudo ip addr add 10.0.0.100/32 dev br-lb
sudo ip addr add 192.168.1.50/24 dev br-lb
# ip route add 10.0.0.100 dev br-lb
rm -f /tmp/backend_ifindex_map

sudo sysctl -w net.ipv4.conf.all.accept_local=1
sudo sysctl -w net.ipv4.conf.lo.accept_local=1

sudo modprobe ipip

for i in {2..7}
do
    NS="ns$i"
    VETH="veth$i"
    PEER="peer$i"
    NS_IP="172.16.0.$i"

    sudo ip netns add $NS

    sudo ip link add $VETH type veth peer name $PEER
    sudo ip link set $PEER netns $NS

    # sudo ip addr add 172.16.0.1/24 dev $VETH
    sudo ip link set $VETH up
    sudo ip link set $VETH master $BRIDGE_NAME

    sudo ip netns exec $NS ip addr add $NS_IP/24 dev $PEER
    sudo ip netns exec $NS ip link set $PEER up
    sudo ip netns exec $NS ip link set lo up

    # VIP
    sudo ip netns exec $NS ip addr add 10.0.0.100/32 dev lo

    # Tunnel
    sudo ip netns exec $NS ip link set tunl0 down
    #sudo ip netns exec $NS ip tunnel del tunl0
    # sudo ip netns exec $NS ip link add tunl0 type ipip local $NS_IP remote 172.16.0.1
    # sudo ip netns exec $NS ip link set tunl0 up
    sudo ip netns exec $NS ip tunnel add tun$i mode ipip local $NS_IP remote 172.16.0.1
    sudo ip netns exec $NS ip link set tun$i up

    sudo ip netns exec $NS sysctl -w net.ipv4.conf.all.rp_filter=0

    sudo ip netns exec $NS sysctl -w net.ipv4.conf.$PEER.rp_filter=0

    #sudo ip netns exec $NS sysctl -w net.ipv4.conf.tunl0.rp_filter=0
    sudo ip netns exec $NS sysctl -w net.ipv4.conf.tun$i.rp_filter=0

    sudo ip netns exec $NS sysctl -w net.ipv4.conf.all.accept_local=1
    sudo ip netns exec $NS sysctl -w net.ipv4.conf.lo.accept_local=1
    
    sudo ip netns exec $NS sysctl -w net.ipv4.conf.$PEER.accept_local=1

    
    # #sudo ip netns exec $NS ip route add 172.16.0.1 dev tun$i
    # sudo ip netns exec $NS ip route add 172.16.0.1 dev $PEER
    # sudo ip netns exec $NS ip route add default via 172.16.0.1
    # --- Routing ---
    # Default route via veth to LB for normal traffic
    # sudo ip netns exec $NS ip route add default via $BRIDGEIP dev $PEER

    # # Create a separate table for tunnel routing
    # echo "200 tun$i" | sudo tee -a /etc/iproute2/rt_tables

    # # Rule: traffic destined for VIP goes through the tunnel
    # sudo ip netns exec $NS ip rule add to $VIP/32 table tun$i
    # sudo ip netns exec $NS ip route add $VIP/32 dev tun$i table tun$i

    # # Route to LB itself must stay via peer
    # sudo ip netns exec $NS ip route add $BRIDGEIP dev $PEER

    ##########################
    TUN_TABLE=$((100+i))
    sudo ip netns exec $NS ip route add local $VIP/32 dev tun$i table $TUN_TABLE
    sudo ip netns exec $NS ip rule add to $VIP/32 lookup $TUN_TABLE

    sudo ip netns exec $NS ip route add default via $BRIDGEIP dev $PEER
    
    # שליפת ifindex
    IFINDEX=$(cat /sys/class/net/$VETH/ifindex)
    J=$(($i - 2))

    MAC=$(cat /sys/class/net/$VETH/address)
    echo "$J $IFINDEX $MAC" >> /tmp/backend_ifindex_map

    # הרצת backend
    sudo ip netns exec $NS "$CLIENT_BIN" $NS_IP $J > "client_$i.log" 2>&1 &
done


# יצירת client namespace
sudo ip netns add client-ns

sudo ip link add veth-client type veth peer name peer-client
sudo ip link set peer-client netns client-ns

sudo ip addr add 172.16.1.1/24 dev veth-client
sudo ip link set veth-client up
sudo ip link set veth-client master br-lb

sudo ip netns exec client-ns ip addr add 172.16.1.2/24 dev peer-client
sudo ip netns exec client-ns ip link set peer-client up
sudo ip netns exec client-ns ip link set lo up

sudo ip netns exec client-ns ip route add default via 172.16.1.1