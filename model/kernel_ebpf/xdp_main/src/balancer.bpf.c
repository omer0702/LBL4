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
        csum+=next_iph[i];
    }
    while(csum >> 16){
        csum = (csum & 0xffff) + (csum >> 16);
    }

    return (__u16)~csum;
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


static __always_inline __u32 calculate_session_hash(struct session_key *key){
    __u32 hash = key->src_ip ^ key->dst_ip;
    hash ^= ((__u32)key->src_port << 16 | key->dst_port);
    hash ^= key->protocol;

    //murmur hash3
    hash ^= hash >> 16;
    hash *= 0x85ebca6b;
    hash ^= hash >> 13;
    hash *= 0xc2b2ae35;
    hash ^= hash >> 16;

    return hash;
}

static __always_inline int extract_session_key(struct xdp_md *ctx, struct session_key *key){
    void* data = (void*)(long)ctx->data;
    void* data_end = (void*)(long)ctx->data_end;

    struct ethhdr *eth = data;
    if ((void*)(eth + 1) > data_end) {
        return -1;
    }

    if(eth->h_proto != bpf_htons(ETH_P_IP)){
        return -1;
    }

    struct iphdr *ip = (struct iphdr *)(eth + 1);
    if ((void*)(ip + 1) > data_end) {
        return -1;
    }

    key->src_ip = ip->saddr;
    key->dst_ip = ip->daddr;
    key->protocol = ip->protocol;

    if(ip->protocol == IPPROTO_TCP){
        struct tcphdr *tcp = (void *)ip + sizeof(struct iphdr);
        if((void *)(tcp + 1) > data_end){
            return -1;
        }
        key->src_port = tcp->source;
        key->dst_port = tcp->dest;
    }
    else if(ip->protocol == IPPROTO_UDP){
        struct udphdr *udp = (void *)ip + sizeof(struct iphdr);
        if((void *)(udp + 1) > data_end){
            return -1;
        }
        key->src_port = udp->source;
        key->dst_port = udp->dest;
    }
    else{
        return -1;
    }

    return 0;
}


SEC("xdp")
int xdp_balancer_prog(struct xdp_md *ctx) {
    void* data = (void*)(long)ctx->data;
    void* data_end = (void*)(long)ctx->data_end;
    bpf_printk("XDP: packet received\n");

    struct session_key key = {0};
    if(extract_session_key(ctx, &key) != 0){
        return XDP_PASS;
    }
    bpf_printk("after parsing");

    //connection tracking code(ensure stateful TCP route):
    struct session_value *sess = bpf_map_lookup_elem(&sessions_map, &key);
    __u32 backend_id_val;

    if(sess){
        backend_id_val = sess->backend_id;
        sess->last_seen = bpf_ktime_get_ns();
        bpf_printk("XDP: existing session, backend_id: %u\n", backend_id_val);
    }
    else{
        void* inner_map = bpf_map_lookup_elem(&services_map, &key.dst_ip);
        if(!inner_map){
            bpf_printk("XDP: failed!!\n");
            return XDP_PASS;
        }
        
        bpf_printk("XDP: incoming packet for service VIP: %pI4\n", &key.dst_ip);

        __u32 hash = calculate_session_hash(&key);
        __u32 offset = hash % SIZE;
        __u32* backend_id = bpf_map_lookup_elem(inner_map, &offset);
        if(!backend_id){
            return XDP_PASS;
        }

        backend_id_val = *backend_id;

        struct session_value new_sess = {
            .backend_id = backend_id_val,
            .last_seen = bpf_ktime_get_ns()
        };
        bpf_map_update_elem(&sessions_map, &key, &new_sess, BPF_ANY);
    }


    // //parsing ethernet
    // struct ethhdr *eth = data;
    // if ((void*)(eth + 1) > data_end) {
    //     return XDP_PASS;
    // }

    // if(eth->h_proto != bpf_htons(ETH_P_IP)){
    //     return XDP_PASS;
    // }

    // //parsing ipv4
    // struct iphdr *ip = (struct iphdr *)(eth + 1);
    // if ((void*)(ip + 1) > data_end) {
    //     return XDP_PASS;
    // }

    // //only UDP
    // if(ip->protocol != IPPROTO_UDP){
    //     return XDP_PASS;
    // }


    // struct udphdr *udp;
    // udp = data + sizeof(struct ethhdr) + sizeof(struct iphdr);
    // if((void *)(udp + 1) > data_end){
    //     return XDP_PASS;
    // }

    //lookup service
    // __u32 service_ip = ip->daddr;
    // bpf_printk("XDP: looking for VIP: %pI4\n", &service_ip);



    struct backend_info* backend = bpf_map_lookup_elem(&backends_map, &backend_id_val);
    if(!backend || !backend->is_active){
        bpf_printk("XDP: failed2!!\n");
        return XDP_PASS;
    }
    //bpf_printk("XDP: routing to backend: %d\n", backend->ip);
    bpf_printk("XDP: the selected backend is with backend_id %u and ip %d", backend_id_val, &backend->ip);

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

    // struct udphdr *inner_udp = (void*)inner_ip + sizeof(struct iphdr);
    // if ((void*)(inner_udp + 1) > data_end) {
    //     return XDP_PASS;
    // }

    // inner_udp->dest= backend->port;
    //inner_udp->check = 0;

    //__builtin__memmove(new_eth, (void*)new_eth + sizeof(struct iphdr), sizeof(struct ethhdr));
    *(struct ethhdr *)new_eth = *(struct ethhdr *)((void*)new_eth + sizeof(struct iphdr));

    //build outer IP header:
    outer_ip->version = 4;
    outer_ip->ihl = 5;
    outer_ip->tos = inner_ip->tos;
    outer_ip->tot_len = bpf_htons(bpf_ntohs(inner_ip->tot_len) + sizeof(struct iphdr));
    outer_ip->id = inner_ip->id;
    outer_ip->frag_off = 0;
    outer_ip->ttl = 64;
    outer_ip->protocol = IPPROTO_IPIP;
    outer_ip->saddr = bpf_htonl(0x7F000001);
    outer_ip->daddr = backend->ip;
    outer_ip->check = ip_checksum(outer_ip);

    bpf_printk("encapsulated VIP %pI4 -> backend %pI4\n", &outer_ip->saddr, &outer_ip->daddr);

    if(inner_ip->protocol == IPPROTO_UDP){
        struct udphdr *inner_udp = (void*)inner_ip + sizeof(struct iphdr);
        if ((void*)(inner_udp + 1) <= data_end) {
            inner_udp->dest= backend->port;
            inner_udp->check = 0;
        }
    }

    if(inner_ip->protocol == IPPROTO_TCP){
        struct tcphdr *inner_tcp = (void*)inner_ip + sizeof(struct iphdr);
        if ((void*)(inner_tcp + 1) <= data_end) {
            __u16 old_port = inner_tcp->dest;
            __u16 new_port = backend->port;

            inner_tcp->dest = new_port;
            //checksum handling:
            __u32 csum = (__u32)inner_tcp->check;
            csum = bpf_csum_diff(&old_port, sizeof(old_port),&new_port, sizeof(new_port), ~csum);
            inner_tcp->check = ~((csum & 0xFFFF) + (csum >> 16));
        }
    }
    bpf_printk("XDP: forwarding to backend ID: %d, IP: %pI4, Port: %d\n", backend_id_val, &backend->ip, backend->port);
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

    //stats:
    struct backend_stats *stats = bpf_map_lookup_elem(&stats_map, &backend_id_val);
    bpf_printk("here in backend id= %u", backend_id_val);
    if(stats){
        bpf_printk("here2 in backend id= %u", backend_id_val);
        stats->num_of_packets++;
        stats->num_of_bytes += inner_ip->tot_len;
    }

   
    // __u8 target_mac[6] = {0x02, 0x42, 0xac, 0x10, 0x00, 0x00};
    // __u32 backend_ip_host = bpf_ntohl(backend->ip);
    // target_mac[5] = (__u8)(backend_ip_host & 0xFF);
    // __builtin_memcpy(new_eth->h_dest, target_mac, 6);

   
    // __u8 lb_mac[6] = {0xb6, 0xdc, 0x34, 0xf6, 0x0a, 0x1a}; 
    // __builtin_memcpy(new_eth->h_source, lb_mac, 6);

    
    // new_eth->h_proto = bpf_htons(ETH_P_IP);

    // bpf_printk("sending to IP %pI4 with MAC suffix: %x\n", &backend->ip, target_mac[5]);

    // bpf_printk("MAC address: %x:%x:%x:%x:%x:%x\n", backend->mac[0], backend->mac[1], backend->mac[2], backend->mac[3], backend->mac[4], backend->mac[5]);
    // __builtin_memcpy(new_eth->h_dest, backend->mac, 6);
    // if (backend->ifindex != 0) {
    //     bpf_printk("Redirecting to ifindex %d\n", backend->ifindex);
    //     return bpf_redirect(backend->ifindex, 0);
    // }

    // // fallback
    // bpf_printk("Fallback to XDP_TX\n");
    return XDP_TX;
}

char _license[] SEC("license") = "GPL";