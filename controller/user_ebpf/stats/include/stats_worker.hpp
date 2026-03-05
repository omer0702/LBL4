#pragma once

#include <atomic>
#include <thread>
#include <vector>
#include <map>

#include "maps_manager.hpp"
#include "session_manager.h"

namespace lb::stats {

struct BackendSnapshot{
    uint64_t last_packets = 0;
    uint64_t last_bytes = 0;
    double pps = 0;//packet per second
    double bps = 0;//bytes per second
};

class StatsWorker{
public:
    StatsWorker(MapsManager& maps_manager, uint32_t interval_ms=2000);
    ~StatsWorker();

    void stop();
    void start();
private:
    void run();
    void collect_metrics();

    MapsManager& maps_manager;
    uint32_t interval_ms;
    std::atomic<bool> running{false};
    std::thread worker;
    std::map<uint32_t, BackendSnapshot> snapshots;
};
}