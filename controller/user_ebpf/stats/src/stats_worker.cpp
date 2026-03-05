#include "stats_worker.hpp"
#include <iostream>
#include <chrono>

namespace lb::stats{
StatsWorker::StatsWorker(MapsManager& maps_manager, uint32_t interval_ms):
    maps_manager(maps_manager), interval_ms(interval_ms){}

StatsWorker::~StatsWorker(){
    stop();
}

void StatsWorker::start(){
    if(running){
        return;
    }
    running =true;
    worker = std::thread(&StatsWorker::run, this);
}

void StatsWorker::stop(){
    running = false;
    if(worker.joinable()){
        worker.join();
    }
}

void StatsWorker::run(){
    std::cout << "[STATS WORKER] statrted background collection thread\n";

    while(running){
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        collect_metrics();
    }
}

void StatsWorker::collect_metrics(){
    auto& sm = lb::session::SessionManager::instance();
    std::vector<int> all_fds = sm.get_all_session_fds();

    for(int fd : all_fds){
        int id = sm.get_logical_id(fd);
        if(id < 0){
            continue;
        }

        backend_stats curr;
        if(maps_manager.get_backend_stats(static_cast<uint32_t>(id), curr)){
            BackendSnapshot& snap = snapshots[id];

            uint64_t delta_p = curr.num_of_packets - snap.last_packets;
            uint64_t delta_b = curr.num_of_bytes - snap.last_bytes;

            double seconds = interval_ms / 1000.0;
            snap.pps = delta_p / seconds;
            snap.bps = delta_b / seconds;
            snap.last_packets = curr.num_of_packets;
            snap.last_bytes = curr.num_of_bytes;

            lb::session::SessionInfo* info = sm.get_session_by_fd(fd);
            if(info){
                std::cout << "Backend [" << info->service_name << " | ID:" << id
                << "] PPS:" << snap.pps << ", BPS:" << snap.bps
                << ", Total packets: "<< curr.num_of_packets << std::endl;
            }
        }   
    }
}

}