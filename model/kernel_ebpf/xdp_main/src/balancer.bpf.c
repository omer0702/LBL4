#include "../../core/vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>
#include "../../common/maps.h"

#define ETH_P_IP 0x0800
#define IPPROTO_IPIP 4

// static __always_inline void update_checksum2(void* data_start, int old_value, int new_value, __u16* csum){
//     __u32 new_csum = *csum + old_value - new_value;
//     new_csum = (new_csum & 0xFFFF) + (new_csum >> 16);
//     *csum = (__u16)new_csum;
// }

static __always_inline __u16 ip_checksum(struct iphdr* iph){
    __u32 csum = 0;
    __u16 *next_iph = (__u16*)iph;

    iph->check = 0;
    #pragma clang loop unroll(full)
    for(int i = 0; i < 10; i++){
        csum+=bpf_htons(next_iph[i]);
    }

    csum = (csum & 0xffff) + (csum >> 16);
    csum = (csum & 0xffff) + (csum >> 16);

    return bpf_htons(~csum);
}

static __always_inline void update_checksum(__u16* csum, __u32 old_ip, __u32 new_ip){
    __u32 sum = *csum;
    sum = ~sum & 0xFFFF;
    sum+= (~(old_ip >> 16) & 0xFFFF) + (~(old_ip & 0xFFFF) & 0xFFFF);
    sum+= (new_ip >> 16) + (new_ip & 0xFFFF);

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum = (sum >> 16) + (sum & 0xFFFF);

    *csum = ~sum & 0xFFFF;
}

static __always_inline __u32 calculate_hash(struct xdp_md *ctx){
    void* data = (void*)(long)ctx->data;
    void* data_end = (void*)(long)ctx->data_end;

    struct ethhdr *eth = data;
    if((void*)(eth + 1) > data_end) {
        return 0;
    }
    if(eth->h_proto != bpf_htons(ETH_P_IP)){
        return 0;
    }
    struct iphdr *ip = (void*)(eth + 1);
    if((void*)(ip + 1) > data_end) {
        return 0;
    }

    return bpf_ntohl(ip->saddr);
}

static __always_inline __u32 calculate_hash2(struct iphdr* ip, struct udphdr* udp){
    //MurmurHash3
    __u32 hash = ip->saddr;
    hash ^= ip->daddr;
    hash ^= (__u32)udp->source;

    hash ^= hash >>16;
    hash *= 0x85ebca6b;
    hash ^= hash >>13;
    hash *= 0xc2b2ae35;
    hash ^= hash >>16;

    return hash;
}

SEC("xdp")
int xdp_balancer_prog(struct xdp_md *ctx) {
    void* data = (void*)(long)ctx->data;
    void* data_end = (void*)(long)ctx->data_end;

    //parsing ethernet
    struct ethhdr *eth = data;
    if ((void*)(eth + 1) > data_end) {
        return XDP_PASS;
    }

    if(eth->h_proto != bpf_htons(ETH_P_IP)){
        return XDP_PASS;
    }

    //parsing ipv4
    struct iphdr *ip = (struct iphdr *)(eth + 1);
    if ((void*)(ip + 1) > data_end) {
        return XDP_PASS;
    }

    //only UDP
    if(ip->protocol != IPPROTO_UDP){
        return XDP_PASS;
    }


    struct udphdr *udp;
    udp = data + sizeof(struct ethhdr) + sizeof(struct iphdr);
    if((void *)(udp + 1) > data_end){
        return XDP_PASS;
    }

    //lookup service
    __u32 service_ip = ip->daddr;
    bpf_printk("XDP: UDP packet detected: dest_ip-raw_hex=0x%x, decimal=%u", service_ip, service_ip);

    void* inner_map = bpf_map_lookup_elem(&services_map, &service_ip);
    if(!inner_map){
        return XDP_PASS;
    }

    bpf_printk("XDP: incoming packet for service VIP: %pI4\n", &ip->daddr);

    __u32 hash = calculate_hash2(ip, udp);
    __u32 offset = hash % SIZE;
    __u32* backend_id = bpf_map_lookup_elem(inner_map, &offset);
    if(!backend_id){
        return XDP_PASS;
    }

    struct backend_info* backend = bpf_map_lookup_elem(&backends_map, backend_id);
    if(!backend || !backend->is_active){
        return XDP_PASS;
    }

    //DSR:
    if(bpf_xdp_adjust_head(ctx, 0 - (int)sizeof(struct iphdr))){
        return XDP_PASS;
    }

    //update the pointers(and check again)
    data = (void*)(long)ctx->data;
    data_end = (void*)(long)ctx->data_end;

    struct ethhdr *new_eth = data;
    if ((void*)(new_eth + 1) > data_end) {
        return XDP_PASS;
    }

    struct iphdr *outer_ip = (void*)(new_eth + 1);
    if ((void*)(outer_ip + 1) > data_end) {
        return XDP_PASS;
    }

    struct iphdr *inner_ip = (void*)(outer_ip + 1);
    if ((void*)(inner_ip + 1) > data_end) {
        return XDP_PASS;
    }

    struct udphdr *inner_udp = (void*)inner_ip + sizeof(struct iphdr);
    if ((void*)(inner_udp + 1) > data_end) {
        return XDP_PASS;
    }

    inner_udp->check = 0;

    //__builtin__memmove(new_eth, (void*)new_eth + sizeof(struct iphdr), sizeof(struct ethhdr));
    *(struct ethhdr *)new_eth = *(struct ethhdr *)((void*)new_eth + sizeof(struct iphdr));

    //build outer IP header:
    outer_ip->version = 4;
    outer_ip->ihl = 5;
    outer_ip->tos = inner_ip->tos;
    outer_ip->tot_len = bpf_htons(bpf_htons(inner_ip->tot_len) + sizeof(struct iphdr));
    outer_ip->id = inner_ip->id;
    outer_ip->frag_off = 0;
    outer_ip->ttl = 64;
    outer_ip->protocol = IPPROTO_IPIP;
    outer_ip->saddr = bpf_htonl(0x7F000001);
    outer_ip->daddr = backend->ip;
    outer_ip->check = ip_checksum(outer_ip);

    bpf_printk("encapsulated VIP %pI4 -> backend %pI4\n", &outer_ip->saddr, &outer_ip->daddr);

    // bpf_printk("XDP: forwarding to backend ID: %d, IP: %pI4\n", *backend_id, &backend->ip);
    // __u32 old_ip = ip->daddr;
    // __u32 new_ip = backend->ip;
    // ip->daddr = backend->ip;
    // //udp->check = 0;
    // // for(int i = 0; i < 6; i++){
    // //     eth->h_dest[i] = backend->mac[i];
    // // }
    // update_checksum(&ip->check, old_ip, new_ip);
    // if(udp->check != 0){
    //     update_checksum(&udp->check, old_ip, new_ip);
    // }
    // //update_checksum(&ip->check, old_ip, new_ip, &ip->check);

    return XDP_TX;
}

char _license[] SEC("license") = "GPL";