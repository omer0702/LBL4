#include "maps_manager.hpp"
#include <iostream>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include "common_structs.h"
#include <unistd.h>
#include <arpa/inet.h>

 

MapsManager::MapsManager(struct balancer_bpf* skel): skel(skel) {
}

bool MapsManager::update_service_map(uint32_t service_ip, const std::vector<uint32_t>& table) {
    std::cout<<"here0"<<std::endl;
    int inner_map_fd = create_inner_map();
    if(inner_map_fd < 0){
        std::cout<<"here1"<<std::endl;
        return false;
    }
    std::cout<<"here2"<<std::endl;

    for(uint32_t i = 0; i<table.size(); ++i){
        uint32_t val = table[i];
        bpf_map_update_elem(inner_map_fd, &i, &val, BPF_ANY);
    }

    int outer_map_fd = bpf_map__fd(skel->maps.services_map);
    std::cout<<"[DEBUG] attempting update outer fd="<<outer_map_fd<<", key="<<service_ip<< " 0x("<<std::hex<<service_ip<<std::dec<<")"<<", inner_fd= "<<inner_map_fd<<std::endl;

    int res = bpf_map_update_elem(outer_map_fd, &service_ip, &inner_map_fd, BPF_ANY);
    if(res!=0){
        perror("actual error from bpf_map_update_elem");
    }
    else{
        int val=-1;
        int l_res=bpf_map_lookup_elem(outer_map_fd, &service_ip, &val);
        if(l_res==0){
            std::cout<<"[DEBUG] success, inner map id in map: "<<val<<std::endl;
        }
        else{
            std::cout<<"[DEBUG] failure"<<std::endl;
        }
    }

    //close(inner_map_fd);


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
    int fd= bpf_map_create(BPF_MAP_TYPE_ARRAY, "maglev",
                              sizeof(uint32_t),
                              sizeof(uint32_t),
                              65537,
                              0);

    if(fd<0){
        int err=errno;
        std::cerr<<"[DEBUGER] failed"<<strerror(err)<<" (code"<<err<<")"<<std::endl;
        if(err==EINVAL){
            std::cerr<<"[DEBUGER] hint"<<std::endl;
        }
    }
    
    return fd;
}
