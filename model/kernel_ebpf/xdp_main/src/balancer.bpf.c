#include "../core/vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>
#include "../common/maps.h"

static __always_inline __u32 calculate_hash(struct xdp_md *ctx){
    void* data = (void*)(long)ctx->data;
    void* data_end = (void*)(long)ctx->data_end;

    struct ethhdr *eth = data;
    if((void*)(eth + 1) > data_end) {
        return 0;
    }
    // if(eth->h_proto != bpf_htons(ETH_P_IP)){
    //     return 0;
    // }
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
    // if(eth->h_proto != bpf_htons(ETH_P_IP)){
    //     return XDP_PASS;
    // }

    //parsing ipv4
    struct iphdr *ip = (struct iphdr *)(eth + 1);
    if ((void*)(ip + 1) > data_end) {
        return XDP_PASS;
    }

    //only UDP
    if(ip->protocol != IPPROTO_UDP){
        return XDP_PASS;
    }

    //lookup service
    __u32 service_ip = ip->daddr;
    void* inner_map = bpf_map_lookup_elem(&services_map, &service_ip);
    if(!inner_map){
        return XDP_PASS;
    }

    __u32 hash = calculate_hash(ctx);
    __u32 offset = hash % SIZE;
    __u32* backend_id = bpf_map_lookup_elem(inner_map, &offset);
    if(!backend_id){
        return XDP_ABORTED;
    }

    struct backend_info* backend = bpf_map_lookup_elem(&backends_map, backend_id);
    if(!backend || !backend->is_active){
        return XDP_ABORTED;
    }

    ip->daddr = backend->ip;

    for(int i = 0; i < 6; i++){
        eth->h_dest[i] = backend->mac[i];
    }


    return XDP_TX;
}

char _license[] SEC("license") = "GPL";