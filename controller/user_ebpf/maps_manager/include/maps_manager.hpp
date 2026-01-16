#pragma once

#include <vector>
#include <string>
#include <stdint.h>
#include "balancer.skel.h"
#include "maglev_builder.h"

class MapsManager{
public:
    explicit MapsManager(struct balancer_bpf* skel);

    bool update_service_map(uint32_t service_ip, const std::vector<uint32_t>& table);
    bool add_backend(uint32_t backend_id, uint32_t ip, uint16_t port);
    bool update_backend_status(uint32_t backend_id, bool is_active);//maybe without 'is_active' var
    
private:
    struct balancer_bpf* skel;

    int create_inner_map();
};