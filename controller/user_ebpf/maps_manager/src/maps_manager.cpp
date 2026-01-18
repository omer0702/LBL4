#include "maps_manager.hpp"
#include <iostream>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include "common_structs.h"
#include <unistd.h>

 

MapsManager::MapsManager(struct balancer_bpf* skel): skel(skel) {
}

bool MapsManager::update_service_map(uint32_t service_ip, const std::vector<uint32_t>& table) {
    int inner_map_fd = create_inner_map();
    if(inner_map_fd < 0){
        return false;
    }
    
    for(uint32_t i = 0; i<table.size(); ++i){
        uint32_t val = table[i];
        bpf_map_update_elem(inner_map_fd, &i, &val, BPF_ANY);
    }

    int outer_map_fd = bpf_map__fd(skel->maps.services_map);
    int error = bpf_map_update_elem(outer_map_fd, &service_ip, &inner_map_fd, BPF_ANY);

    close(inner_map_fd);

    if(error){
        std::cerr << "[MAPS_MANAGER]Failed to link VIP to service table: " << strerror(-error) << std::endl;
        return false;
    }

    return true;
}

bool MapsManager::add_backend(int backend_id, uint32_t ip, uint16_t port, uint8_t* mac) {
    backend_info info{};
    info.ip = ip;
    info.port = port;
    info.is_active = 1;
    memcpy(info.mac, mac, 6);

    int fd = bpf_map__fd(skel->maps.backends_map);
    int error = bpf_map_update_elem(fd, &backend_id, &info, BPF_ANY);
    if(error){
        std::cerr << "[MAPS_MANAGER]Failed to update backend: " << backend_id << std::endl;
        return false;
    }
    
    return true;
}

bool MapsManager::update_backend_status(uint32_t backend_id, bool is_active) {
    int fd = bpf_map__fd(skel->maps.backends_map);
    backend_info info{};
    int error = bpf_map_lookup_elem(fd, &backend_id, &info);
    if(error){
        std::cerr << "[MAPS_MANAGER]Failed to find backend: " << backend_id << std::endl;
        return false;
    }

    info.is_active = is_active ? 1 : 0;

    error = bpf_map_update_elem(fd, &backend_id, &info, BPF_ANY);
    if(error){
        std::cerr << "[MAPS_MANAGER]Failed to update backend status: " << backend_id << std::endl;
        return false;
    }

    return true;
}

int MapsManager::create_inner_map(){
    return bpf_create_map(BPF_MAP_TYPE_ARRAY, sizeof(uint32_t), sizeof(uint32_t), MaglevBuilder::M, 0);
}
