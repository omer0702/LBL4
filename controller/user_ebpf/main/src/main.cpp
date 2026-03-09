#include "io_epoll.h"
#include "protocol_encoder.h"
#include "service.pb.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>
#include <atomic>


#include "ebpf_loader.hpp"
#include "maps_manager.hpp"
#include "scorer.h"
#include "maglev_builder.h"
#include "session_manager.h"
#include "common_structs.h"
#include "stats_worker.hpp"

#define PORT 8080
using namespace lb;

std::atomic<bool> keep_running(true);

void signal_handler(int signum) {
    keep_running = false;
    
    std::cout << "[MAIN] Caught signal " << signum << ", shutting down..." << std::endl;
}

int main() {  
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
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

    lb::stats::StatsWorker stats_worker(maps_manager, 5000);//add print
    stats_worker.start();

    std::thread maglev_thread([&](){
        std::cout << "[MAIN] Maglev update thread started\n";
        while(keep_running){
            maps_manager.wait_for_update(5);
            if(!keep_running) break; //if we woke up because of shutdown

            std::unordered_map<std::string, uint32_t> services = sm.get_all_service_vips();

            for(const auto& [name, vip] : services){
                std::vector<BackendScore> backend_scores = lb::routing::Scorer::score_service_instances(name);
                if(backend_scores.empty()){
                    std::cout << "[MAIN] no backends for service: " << name << "\n";
                    continue;
                }

                MaglevBuilder builder(backend_scores);
                std::vector<uint32_t> table = builder.build_table();
                builder.test_maglev_builder(table, backend_scores);
                maps_manager.update_service_map(vip, table);
                    
                std::cout << "[MAIN] updated maglev table for service: " << name << "\n";

                // std::cout << "[MAIN] check statistics: \n";
                // backend_stats test;
                // if(maps_manager.get_backend_stats(5, test)){
                //     std::cout << "[MAIN] backend 5 stats: packets= "<< test.num_of_packets<< ", bytes= "<<test.num_of_bytes<<std::endl;
                // }
            }
        }
    });

    maglev_thread.detach();//maybe replace with join


    int tcp_listen_fd = lb::io_epoll::start_listen_tcp(PORT);
    int unix_listen_fd = lb::io_epoll::start_listen_unix("./lb.sock");
    if (tcp_listen_fd < 0 || unix_listen_fd < 0) return 1;
    std::cout << "[MAIN] Listening on port " << PORT << "\n";

    lb::io_epoll::run_loop(tcp_listen_fd, unix_listen_fd, maps_manager, keep_running);
    unlink("/tmp/lb.sock");
    
    return 0;
}
