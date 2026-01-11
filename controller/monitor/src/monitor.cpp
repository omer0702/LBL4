#include "monitor.h"
#include <fstream>
#include <unistd.h>
#include <string>
#include <thread>
#include <chrono>
#include <iostream>

namespace lb::monitor {
Monitor::Monitor(){
    read_cpu_ticks(last_process_ticks, last_uptime_ticks);
    
}

void Monitor::read_cpu_ticks(long &process_ticks, long &uptime_ticks){
    std::ifstream stat_file("/proc/self/stat");
    std::string line;
    for(int i = 0; i < 13; ++i) stat_file >> line; 
    long utime, stime;
    stat_file >> utime >> stime;
    process_ticks = utime + stime;

    std::ifstream uptime_file("/proc/uptime");
    double uptime_sec;
    uptime_file >> uptime_sec;

    uptime_ticks = static_cast<long>(uptime_sec * sysconf(_SC_CLK_TCK));
}

RealMetrics Monitor::get_current_metrics(){
    long current_process, current_uptime;
    read_cpu_ticks(current_process, current_uptime);

    long process_diff = current_process - last_process_ticks;
    long uptime_diff = current_uptime - last_uptime_ticks;

    uint32_t cpu_usage = 0;
    if(uptime_diff > 0){
        cpu_usage = static_cast<uint32_t>((process_diff/uptime_diff) * 100.0);
    }

    last_process_ticks = current_process;
    last_uptime_ticks = current_uptime;

    std::ifstream statm("/proc/self/statm");
    long size, rss;
    statm >> size >> rss;
    uint32_t memory_usage = (rss * sysconf(_SC_PAGESIZE)) / (1024 * 1024);//in MB

    last_process_ticks = current_process;
    last_uptime_ticks = current_uptime;

    return {memory_usage, cpu_usage};
}

}

extern "C" {
    lb::monitor::Monitor* create_monitor() {
        return new lb::monitor::Monitor();
    }

    void get_metrics(lb::monitor::Monitor* monitor, uint32_t* cpu, uint32_t* memory) {
        auto metrics = monitor->get_current_metrics();
        *cpu = metrics.cpu_usage;
        *memory = metrics.memory_usage;
    }
}

// void background_monitor_task() {
//     lb::monitor::Monitor monitor;

//     while (true) {
//         auto metrics = monitor.get_current_metrics();
//         std::cout << "[MONITOR] CPU Usage: " << metrics.cpu_usage << "%, Memory Usage: " << metrics.memory_usage << " MB\n";
//         std::this_thread::sleep_for(std::chrono::seconds(5));
//     }
// }

// __attribute__((constructor))
// void init_monitoring() {
//     std::thread (background_monitor_task).detach();
// }