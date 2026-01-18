#include "io_epoll.h"
#include "protocol_encoder.h"
#include "service.pb.h"
#include <iostream>
#include <chrono>
#include <thread>


#include "ebpf_loader.hpp"
#include "maps_manager.hpp"
#include "scorer.h"
#include "maglev_builder.h"
#include "session_manager.h"

#define PORT 8080
using namespace lb;

int main() {  
    EbpfLoader ebpf_loader;
    if(!ebpf_loader.loadProgram()){
        std::cerr << "[MAIN] failed to load eBPF program\n";
        return 1;
    }

    if(!ebpf_loader.attachProgram("lo")){
        std::cerr << "[MAIN] failed to attach eBPF program\n";
        return 1;
    }

    MapsManager maps_manager(ebpf_loader.get_skel());
    auto& sm = lb::session::SessionManager::instance();

    std::thread maglev_thread([&](){
        std::cout << "[MAIN] Maglev update thread started\n";
        while(true){
            std::this_thread::sleep_for(std::chrono::seconds(5));
            std::unordered_map<std::string, uint32_t> services = sm.get_all_service_vips();

            for(const auto& [name, vip] : services){
                std::vector<BackendScore> backend_scores = lb::routing::Scorer::score_service_instances(name);
                if(backend_scores.empty()){
                    std::cout << "[MAIN] no backends for service: " << name << "\n";
                    continue;
                }

                MaglevBuilder builder(backend_scores);
                std::vector<uint32_t> table = builder.build_table();
                maps_manager.update_service_map(vip, table);
                
                std::cout << "[MAIN] updated maglev table for service: " << name << "\n";
            }
        }
    });

    maglev_thread.detach();


    int listen_fd = lb::io_epoll::start_listen(PORT);
    if (listen_fd < 0) return 1;
    std::cout << "[MAIN] Listening on port " << PORT << "\n";

    lb::io_epoll::run_loop(listen_fd, maps_manager);
    return 0;
}
