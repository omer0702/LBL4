#ifndef COMMON_STRUCTS_H
#define COMMON_STRUCTS_H


struct backend_info {
    __u32 ip;
    __u16 port;
    __u8 mac[6];
    __u8 is_active;
};

struct session_key{
    __u32 src_ip;
    __u32 dst_ip;
    __u16 src_port;
    __u16 dst_port;
    __u8 porotocol;
};

#endif 