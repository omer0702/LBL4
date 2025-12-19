#pragma once
#include <unordered_map>
#include <optional>
#include <chrono>
#include <mutex>
#include <vector>
#include <string>
namespace lb::session {


enum class SessionState {
    ACTIVE,
    INACTIVE,
    BUSY,
    RECOVERING,
    UNREACHABLE 
};


struct SessionInfo {
    int fd;
    std::string service_name;
    std::string token;
    SessionState state;
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