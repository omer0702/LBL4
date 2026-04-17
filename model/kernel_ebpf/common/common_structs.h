#ifndef COMMON_STRUCTS_H
#define COMMON_STRUCTS_H


struct backend_info {
    __u32 ip;
    __u16 port;
    __u8 mac[6];
    __u32 ifindex;
    __u8 is_active;
};

struct session_key{
    __u32 src_ip;
    __u32 dst_ip;
    __u16 src_port;
    __u16 dst_port;
    __u8 protocol;
    __u8 pad[3];
};

struct session_value {
    __u32 backend_id;
    __u64 last_seen;
    __u8 tcp_state;//for fin
};

struct backend_stats{
    __u64 num_of_packets;
    __u64 num_of_bytes;
};

#endif 