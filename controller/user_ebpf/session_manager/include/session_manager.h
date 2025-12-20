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

struct SessionInfo {
    int fd;
    std::string service_name;
    std::string token;
    SessionState state;
    ServiceMetrics metrics;
    std::chrono::steady_clock::time_point last_seen;//for keepalive logs
};


class SessionManager {
public:
    static SessionManager& instance();

    std::string create_session(int fd, const std::string& service_name);
    std::optional<SessionInfo> get_session_by_fd(int fd);
    std::optional<SessionInfo> get_session_by_token(const std::string& token);
    std::vector<int> get_expired_sessions(int timeout_seconds);
    std::vector<int> get_all_session_fds();
    bool has_session(int fd);
    void update_last_seen(int fd);
    void remove_session(int fd);
    void update_metrics(int fd, const lb::ServiceReport& report);

private:
    SessionManager() = default;
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    std::string generate_token();

    std::unordered_map<int, SessionInfo> sessions_by_fd;
    std::unordered_map<std::string, int> fd_by_token;
    std::mutex mtx;
};

}