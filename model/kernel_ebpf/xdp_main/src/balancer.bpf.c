#include "../../core/vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>
#include "../../common/maps.h"

#define ETH_P_IP 0x0800

static __always_inline void update_checksum(void* data_start, int old_value, int new_value, __u16* csum){
    __u32 new_csum = *csum + old_value - new_value;
    new_csum = (new_csum & 0xFFFF) + (new_csum >> 16);
    *csum = (__u16)new_csum;
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

    __u32 hash = calculate_hash(ctx);
    __u32 offset = hash % SIZE;
    __u32* backend_id = bpf_map_lookup_elem(inner_map, &offset);
    if(!backend_id){
        return XDP_PASS;
    }

    struct backend_info* backend = bpf_map_lookup_elem(&backends_map, backend_id);
    if(!backend || !backend->is_active){
        return XDP_PASS;
    }

    bpf_printk("XDP: forwarding to backend ID: %d, IP: %pI4\n", *backend_id, &backend->ip);

    __u32 old_ip = ip->daddr;
    __u32 new_ip = backend->ip;
    ip->daddr = backend->ip;

    udp->check = 0;
    

    // for(int i = 0; i < 6; i++){
    //     eth->h_dest[i] = backend->mac[i];
    // }

    update_checksum(&ip->check, old_ip, new_ip, &ip->check);

    return XDP_TX;
}

char _license[] SEC("license") = "GPL";