#pragma once
#include <unordered_map>
#include <optional>
#include <chrono>
#include <mutex>
#include <vector>
#include <string>
#include "../../../../build/debug/controller/proto/service.pb.h"

namespace lb::session {


enum class SessionState {
    ACTIVE,
    INACTIVE,
    BUSY,
    RECOVERING,
    UNREACHABLE 
};

struct ServiceMetrics{
    uint32_t cpu_usage = 0;
    uint32_t memory_usage = 0;
    uint32_t active_requests = 0;
    std::chrono::steady_clock::time_point last_report;
};

struct SessionInfo {//represents a single instance(server) in service
    int fd;
    std::string service_name;
    std::string token;
    uint32_t ip;
    uint16_t port;
    SessionState state;
    ServiceMetrics metrics;
    std::chrono::steady_clock::time_point last_seen;//for keepalive logs

    SessionInfo(int fd,
                const std::string& name,
                const std::string& t,
                uint32_t ip,
                uint16_t port)
        : fd(fd),
          service_name(name),
          token(t),
          ip(ip),
          port(port),
          state(SessionState::ACTIVE),
          last_seen(std::chrono::steady_clock::now()) {}
};


class SessionManager {
public:
    static SessionManager& instance();

    std::string create_session(int fd, const std::string& service_name, uint32_t ip, uint16_t port);
    SessionInfo* get_session_by_fd(int fd);
    //std::optional<SessionInfo> get_session_by_token(const std::string& token);
    std::vector<int> get_expired_sessions(int timeout_seconds);
    std::vector<int> get_all_session_fds();
    std::vector<int> get_instances_for_service(const std::string& service_name);
    bool has_session(int fd);
    void update_last_seen(int fd);
    void remove_session(int fd);
    void update_metrics(int fd, const lb::ServiceReport& report);
    void print_session_stats();

    void register_service_vip(const std::string& service_name, uint32_t vip);
    uint32_t get_service_vip(const std::string& service_name);
    std::vector<uint32_t> get_all_service_vips();

private:
    SessionManager() = default;
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    std::string generate_token();

    std::unordered_map<int, std::unique_ptr<SessionInfo>> session_mapping;//fd -> session info
    std::unordered_map<std::string, std::vector<int>> service_groups;//service name -> list of fds
    std::unordered_map<std::string, uint32_t> service_vips;//service name -> vip
    std::mutex mtx;
};

}