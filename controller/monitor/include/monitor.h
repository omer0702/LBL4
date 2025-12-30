#pragma once
#include <cstdint>

namespace lb::monitor {
struct  RealMetrics{
    uint32_t memory_usage;
    uint32_t cpu_usage;
};

class Monitor {
public:
    Monitor();
    RealMetrics get_current_metrics();

private:
    long last_process_ticks;
    long last_uptime_ticks;
    void read_cpu_ticks(long &process_ticks, long &uptime_ticks);
};

}