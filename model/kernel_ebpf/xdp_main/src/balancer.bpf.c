#include "../../core/vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>
#include "../../common/maps.h"

#define ETH_P_IP 0x0800
#define IPPROTO_IPIP 4
#define INVALID_BACKEND 99999
#define FIRST_FIN 1234

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

static __always_inline int extract_session_key(struct xdp_md *ctx, struct session_key *key, __u64 *l4_offset, __u8 *proto){
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
    *proto = ip->protocol;

    *l4_offset = sizeof(struct ethhdr) + sizeof(struct iphdr);

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

static __always_inline __u32 pick_backend(struct session_key *key){
    void* inner_map = bpf_map_lookup_elem(&services_map, &key->dst_ip);
    if(!inner_map){
        //bpf_printk("XDP: failed!!\n");
        return INVALID_BACKEND;
    }
        
    //bpf_printk("XDP: incoming packet for service VIP: %pI4\n", &key->dst_ip);
    #pragma clang loop unroll(full)
    for(int i = 0; i < MAX_BACKENDS; i++){
        __u32 hash = calculate_session_hash(key);//put outside of the loop
        __u32 real_hash = hash + i;
        __u32 offset = real_hash % SIZE;
        __u32* backend_id = bpf_map_lookup_elem(inner_map, &offset);
        if(!backend_id){
            continue;
        }

        struct backend_info *b = bpf_map_lookup_elem(&backends_map, backend_id);
        if(b && b->is_active){
            bpf_printk("XDP: in second if");
            return *backend_id;
        }
    }
    
    return INVALID_BACKEND;
}

static __always_inline int should_create_session(__u8 proto, void *l4_hdr){
    if(proto == IPPROTO_TCP){
        struct tcphdr *tcp = l4_hdr;
        return tcp->syn;
    }

    if(proto == IPPROTO_UDP){
        return 1;
    }

    return 0;
}

static __always_inline __u32 get_or_create_backend(struct session_key *key, void *l4_hdr, __u8 proto){
    struct session_value *sess = bpf_map_lookup_elem(&sessions_map, key);
    bpf_printk("lookup session: src=%x, sport=%u, dst=%x, dport=%u, proto=%u\n", key->src_ip, bpf_ntohs(key->src_port), key->dst_ip, bpf_ntohs(key->dst_port), key->protocol);
    if(sess){
        bpf_printk("session exists: backend %u\n", sess->backend_id);
        sess->last_seen = bpf_ktime_get_ns();
        if (proto == IPPROTO_TCP){
            struct tcphdr *tcp = l4_hdr;
            if (tcp->rst){
                //bpf_map_delete_elem(&sessions_map, key);//put in comment to check timeout mechanism
            }
            //delete after double fin:
            if(tcp->fin){
                //bpf_printk("XDP: FIN packet\n");
                sess->tcp_state = TCP_FIN_WAIT1;
            }
            if(sess->tcp_state == TCP_FIN_WAIT1 && tcp->ack && !tcp->fin && !tcp->syn){
                bpf_map_delete_elem(&sessions_map, key);
                //bpf_printk("XDP: deleted successfully");
            }
        }
        return sess->backend_id;
    }
    bpf_printk("session miss\n");
    // else if(!sess && (proto == IPPROTO_TCP)){//every packet with no sess and no syn, should be drop
    //     bpf_printk("XDP: here2\n");
    //     struct tcphdr *tcp = l4_hdr;
    //     if(!tcp->syn){
    //         return INVALID_BACKEND;
    //     }
    // }
    __u32 backend_id = pick_backend(key);

    if(should_create_session(proto, l4_hdr)){
        struct session_value new_sess = {
                .backend_id = backend_id,
                .last_seen = bpf_ktime_get_ns(),
                .tcp_state = 0
            };
        bpf_map_update_elem(&sessions_map, key, &new_sess, BPF_ANY);
    }

    return backend_id;
}

static __always_inline int is_service_vip(__u32 address){
    void *m = bpf_map_lookup_elem(&services_map, &address);
    return m != NULL;
}


static __always_inline int send_rst_packet(struct xdp_md* ctx){
    void* data = (void*)(long)ctx->data;
    void* data_end = (void*)(long)ctx->data_end;

    struct ethhdr *eth = data;
    if ((void*)(eth + 1) > data_end) {
        return XDP_PASS;
    }

    if(eth->h_proto != bpf_htons(ETH_P_IP)){
        return XDP_PASS;
    }

    struct iphdr *ip = (struct iphdr *)(eth + 1);
    if ((void*)(ip + 1) > data_end) {
        return XDP_PASS;
    }

    if(ip->protocol != IPPROTO_TCP){
        return XDP_PASS;
    }

    struct tcphdr *tcp = (struct tcphdr *)(ip + 1);
    if ((void*)(tcp + 1) > data_end) {
        return XDP_PASS;
    }

    //mac exchange:
    __u8 tcp_mac[6];
    __builtin_memcpy(tcp_mac, eth->h_dest, 6);
    __builtin_memcpy(eth->h_dest, eth->h_source, 6);
    __builtin_memcpy(eth->h_source, tcp_mac, 6);

    //IP exchange:
    __u32 src_ip = ip->saddr;
    ip->saddr = ip->daddr;
    ip->daddr = src_ip;
    ip->tot_len = bpf_htons(sizeof(struct iphdr) + sizeof(struct tcphdr));
    ip->check = 0;
    ip->check = ip_checksum(ip);

    //TCP ports and flags:
    __u16 src_port = tcp->source;
    tcp->source = tcp->dest;
    tcp->dest = src_port;
    __u8 had_ack = tcp->ack;
    __u32 old_seq = tcp->seq;
    __u32 old_ack = tcp->ack_seq;
    tcp->rst = 1;
    tcp->syn = 0;
    tcp->ack = 0;
    tcp->fin = 0;
    tcp->psh = 0;
    tcp->urg = 0;

    if(had_ack){
        tcp->seq = old_ack;
        tcp->ack = 0;
        tcp->ack_seq = 0;
    } else {
        tcp->ack_seq = bpf_htonl(bpf_ntohl(old_seq) + 1);
        tcp->ack = 1;
        tcp->seq = 0;
    }
    
    tcp->check = 0;
    __u32 csum = 0;
    csum = bpf_csum_diff(0, 0, (__be32 *)&ip->saddr, 4, csum);
    csum = bpf_csum_diff(0, 0, (__be32 *)&ip->daddr, 4, csum);

    __u32 pseudo = bpf_htonl((IPPROTO_TCP << 16) | sizeof(struct tcphdr));
    csum = bpf_csum_diff(0, 0, &pseudo, sizeof(pseudo), csum);

    csum = bpf_csum_diff(0, 0, (__be32 *)tcp, sizeof(struct tcphdr), csum);

    csum = (csum & 0xFFFF) + (csum >> 16);
    csum = (csum & 0xFFFF) + (csum >> 16);

    tcp->check = ~csum;

    return XDP_TX;
}

SEC("xdp")
int xdp_balancer_prog(struct xdp_md *ctx) {
    void* data = (void*)(long)ctx->data;
    void* data_end = (void*)(long)ctx->data_end;
    //bpf_printk("XDP: packet received\n");

    __u64 l4_offset = 0;
    __u8 proto = 0;

    struct session_key key = {};
    if(extract_session_key(ctx, &key, &l4_offset, &proto) != 0){
        return XDP_PASS;
    }
    //bpf_printk("after parsing");
    if(!is_service_vip(key.dst_ip)){
        return XDP_PASS;
    }

    void *l4_hdr = data + l4_offset;
    if (l4_hdr + sizeof(struct tcphdr) > data_end){
        return XDP_PASS;
    }

    //connection tracking code(ensure stateful TCP route):
    //key.dst_port = htons(80);

    __u32 backend_id = get_or_create_backend(&key, l4_hdr, proto);

    if(backend_id == INVALID_BACKEND){
        return XDP_PASS;
    }

    struct backend_info* backend = bpf_map_lookup_elem(&backends_map, &backend_id);
    if(backend){
        bpf_printk("XDP: backend id is %u, status is %u\n", backend_id, backend->is_active);
    }
    //bpf_printk("XDP: backend status is %u\n", backend->is_active);
    if(!backend || !backend->is_active){
        //bpf_printk("XDP: backend is inactive!!\n");

        //bpf_map_delete_elem(&sessions_map, &key);//delete the session if the backend is inactive
        if(proto == IPPROTO_TCP){
            bpf_printk("XDP: backend %u crashed, sending RST packet to backend\n", backend_id);
            return send_rst_packet(ctx);
        }
        
        return XDP_DROP;
    }
    //bpf_printk("XDP: the selected backend is with backend_id %u and ip %d", backend_id, &backend->ip);

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
    outer_ip->saddr = bpf_htonl(0x7F000001);//LB_IP=127.0.0.1
    outer_ip->daddr = backend->ip;
    outer_ip->check = ip_checksum(outer_ip);

    //bpf_printk("encapsulated VIP %pI4 -> backend %pI4\n", &outer_ip->saddr, &outer_ip->daddr);

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
    bpf_printk("XDP: forwarding to backend ID: %d, IP: %pI4, Port: %d\n", backend_id, &backend->ip, backend->port);

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
    struct backend_stats *stats = bpf_map_lookup_elem(&stats_map, &backend_id);
    if(stats){
        //bpf_printk("here2 in backend id= %u", backend_id);
        stats->num_of_packets++;
        stats->num_of_bytes += inner_ip->tot_len;
    }

   
    
    return XDP_TX;
}

char _license[] SEC("license") = "GPL";