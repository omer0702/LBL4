#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <stdint.h>
#include "balancer.skel.h"
#include "maglev_builder.h"
#include <map>
#include "common_structs.h"

class MapsManager{
public:
    MapsManager(struct balancer_bpf* skel);

    bool update_service_map(uint32_t service_ip, const std::vector<uint32_t>& table);
    bool update_service_map2(uint32_t service_ip, const std::vector<uint32_t>& table);
    bool add_backend(int backend_id, uint32_t ip, uint16_t port, uint8_t* mac);
    bool update_backend_status(uint32_t backend_id, bool is_active);//maybe without 'is_active' var

    bool get_backend_stats(uint32_t logical_id, backend_stats& stats);
    std::map<uint32_t, backend_stats> get_all_stats(const std::vector<uint32_t>& active_ids);
    void cleanup_sessions(uint64_t tcp_timeout, uint64_t udp_timeout);
    void print_sessions_count();

    void trigger_rebuild();
    void wait_for_update(int seconds);
    void change_shutdown();
    
private:
    struct balancer_bpf* skel;

    int create_inner_map();

    std::mutex update_mutex;
    std::atomic<bool> needs_rebuild{false};
    std::condition_variable cv;
    bool shutdown = false;
};