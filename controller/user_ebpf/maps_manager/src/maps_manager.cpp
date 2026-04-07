#include "maps_manager.hpp"
#include <iostream>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include "common_structs.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <map>
#include "../../session_manager/include/session_manager.h"
// #include <cstdio>
#include <fstream>
#include <sstream>

MapsManager::MapsManager(struct balancer_bpf* skel): skel(skel) {
}

bool MapsManager::update_service_map(uint32_t service_ip, const std::vector<uint32_t>& table) {
    int outer_map_fd = bpf_map__fd(skel->maps.services_map);
    int inner_map_fd;

    int lookup_res = bpf_map_lookup_elem(outer_map_fd, &service_ip, &inner_map_fd);
    if(lookup_res != 0){
        std::cout<<"[MAPS] creating new inner map for service: "<<service_ip<<std::endl;
        inner_map_fd=create_inner_map();

        // unsigned char *bytes = (unsigned char*)&service_ip;
        // printf("[MAPS] cinserting VIP: %u.%u.%u.%u. (HEX: %02x %02x %02x %02x)\n",
        //      bytes[0], bytes[1], bytes[2], bytes[3],
        //      bytes[0], bytes[1], bytes[2], bytes[3]);
        bpf_map_update_elem(outer_map_fd, &service_ip, &inner_map_fd, BPF_ANY);
    }

    // std::map<uint32_t, int> counts;
    // for(auto x : table)counts[x]++;
    // std::cout << "[DEBUG], size: "<<table.size()<<std::endl;
    // for(auto const& [id, count]: counts){
    //     std::cout<<"ID="<<id<<"count="<<count<<std::endl;
    // }

    uint32_t counts2[6]={0,0,0,0,0,0};
    int c=0;
    for(uint32_t i = 0; i< table.size(); ++i){
        uint32_t val = table[i];
        bpf_map_update_elem(inner_map_fd, &i, &val, BPF_ANY);


        if(val>=0 && val<=5){
            counts2[val]++;
            if(val==5){
                c++;
            }
        }
        else{
            std::cout<<"warning!!\n";
        }
    }

    std::cout<<"distribution in map after update :"<<c<<std::endl;
    for(int i=0; i<6; ++i){
        std::cout<<"server ID="<<i<<"count="<<counts2[i]<<std::endl;
    }

    std::cout<<"[MAPS] in place update complete for VIP= "<<service_ip<<std::endl;

    return true;
}
bool MapsManager::update_service_map2(uint32_t service_ip, const std::vector<uint32_t>& table) {
    int inner_map_fd = create_inner_map();
    if(inner_map_fd < 0){
        return false;
    }

    std::map<uint32_t, int> counts;
    for(auto x : table)counts[x]++;
    std::cout << "[DEBUG]\n";
    for(auto const& [id, count]: counts){
        std::cout<<"ID="<<id<<"count="<<count<<std::endl;
    }
    
    uint32_t test_val = 0x12345678;
    uint32_t test_key = 0;
    bpf_map_update_elem(inner_map_fd, &test_key, &test_val, BPF_ANY);

    for(uint32_t i = 1; i<table.size(); ++i){
        uint32_t val = table[i];
        //std::cout<<"DEBUG, val= "<<val<<std::endl;
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

bool MapsManager::add_backend(int socket_fd, uint32_t ip, uint16_t port, uint8_t* mac2) {
    int logical_id = lb::session::SessionManager::instance().get_logical_id(socket_fd);
    if(logical_id==-1){
        std::cerr<<"[MAPS MANAGER] no logical id found for fd:"<<socket_fd<<std::endl;
        return false;
    }

    backend_info info{};
    info.ip = ip;
    info.port = port;
    info.is_active = 1;
    //read suitable ifindex and mac address to backend from tmp/backend_ifindex_map/ that run_backends.sh created:
    // int ifindex = -1;
    // uint8_t mac[6] = {0};

    // std::ifstream ifs("/tmp/backend_ifindex_map");
    // std::string line;

    // while (std::getline(ifs, line)) {
    //     std::istringstream iss(line);
    //     int id;
    //     int idx;
    //     std::string mac_str;

    //     if (iss >> id >> idx >> mac_str) {
    //         if (id == logical_id) {
    //             ifindex = idx;

    //             //  parsing MAC string -> byte array
    //             int values[6];
    //             if (sscanf(mac_str.c_str(), "%x:%x:%x:%x:%x:%x",
    //                     &values[0], &values[1], &values[2],
    //                     &values[3], &values[4], &values[5]) == 6) {

    //                 for (int i = 0; i < 6; i++) {
    //                     mac[i] = (uint8_t)values[i];
    //                 }
    //             } else {
    //                 std::cerr << "[MAPS MANAGER] failed to parse MAC\n";
    //                 return false;
    //             }

    //             break;
    //         }
    //     }
    // }

    // ifs.close();

    // if (ifindex == -1) {
    //     std::cerr << "[MAPS MANAGER] no ifindex found for fd:" << socket_fd << std::endl;
    //     return false;
    // }

    // info.ifindex = ifindex;
    // memcpy(info.mac, mac, 6);
    // std::cout << "[MAPS_MANAGER] backend " << logical_id << " full mac=" << mac << std::endl;

    int fd = bpf_map__fd(skel->maps.backends_map);
    int error = bpf_map_update_elem(fd, &logical_id, &info, BPF_ANY);
    if(error){
        std::cerr << "[MAPS_MANAGER]Failed to update backend: " << logical_id << std::endl;
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

bool MapsManager::get_backend_stats(uint32_t logical_id, backend_stats &stats){
    int num_of_cpus = libbpf_num_possible_cpus();
    if(num_of_cpus < 0){
        return false;
    }

    std::vector<backend_stats> cpu_stats(num_of_cpus);
    int map_fd = bpf_map__fd(skel->maps.stats_map);

    if(bpf_map_lookup_elem(map_fd, &logical_id, cpu_stats.data()) != 0){
        return false;
    }

    stats.num_of_packets = 0;
    stats.num_of_bytes = 0;

    for(int i = 0; i< num_of_cpus; i++){
        stats.num_of_packets += cpu_stats[i].num_of_packets;
        stats.num_of_bytes += cpu_stats[i].num_of_bytes;
    }

    return true;
}

// std::map<uint32_t, backend_stats> get_all_stats(const std::vector<uint32_t>& active_ids){

// }

void MapsManager::trigger_rebuild(){
    needs_rebuild = true;
    cv.notify_one();
}

void MapsManager::wait_for_update(int seconds){
    std::unique_lock<std::mutex> lock(update_mutex);

    cv.wait_for(lock, std::chrono::seconds(seconds), [&]{
        return needs_rebuild || shutdown;
    });

    needs_rebuild = false;
}

void MapsManager::change_shutdown(){
    shutdown = true;
}