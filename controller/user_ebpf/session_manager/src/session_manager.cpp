#include "session_manager.h"
#include <random>

namespace lb::session {

SessionManager& SessionManager::instance() {
    static SessionManager sm;
    return sm;
}


std::string SessionManager::generate_token() {
    static constexpr char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);

    std::string token(16, '0');
    for (char& c : token) {
        c = charset[dist(rng)];
    }
    
    return token;
}


std::string SessionManager::create_session(int fd, const std::string& service_name) {
    std::lock_guard<std::mutex> lock(mtx);
    std::string token = generate_token();

    SessionInfo info{
        fd,
        service_name,
        token,
        SessionState::ACTIVE,
        ServiceMetrics{},
        std::chrono::steady_clock::now()
    };

    sessions_by_fd[fd] = info;
    fd_by_token[token] = fd;

    return token;
}


std::optional<SessionInfo> SessionManager::get_session_by_fd(int fd) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = sessions_by_fd.find(fd);
    if (it == sessions_by_fd.end()) return std::nullopt;

    return it->second;
}


std::optional<SessionInfo> SessionManager::get_session_by_token(const std::string& token) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = fd_by_token.find(token);
    if (it == fd_by_token.end()) return std::nullopt;

    return sessions_by_fd[it->second];
}


std::vector<int> SessionManager::get_expired_sessions(int timeout_seconds){
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<int> expired_fds;
    auto now = std::chrono::steady_clock::now();

    for(const auto& [fd, session] : sessions_by_fd){
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - session.last_seen).count();
        if(duration > timeout_seconds){
            expired_fds.push_back(fd);
        }
    }

    return expired_fds;
}

std::vector<int> SessionManager::get_all_session_fds(){
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<int> fds;
    for(const auto& [fd, session] : sessions_by_fd){
        fds.push_back(fd);
    }
    
    return fds;
}

bool SessionManager::has_session(int fd) {
    std::lock_guard<std::mutex> lock(mtx);
    return sessions_by_fd.find(fd) != sessions_by_fd.end();
}

void SessionManager::update_last_seen(int fd) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = sessions_by_fd.find(fd);
    if (it != sessions_by_fd.end()) {
        it->second.last_seen = std::chrono::steady_clock::now();
    }
}


void SessionManager::remove_session(int fd) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = sessions_by_fd.find(fd);
    if (it == sessions_by_fd.end()) return;

    fd_by_token.erase(it->second.token);
    sessions_by_fd.erase(it);
}

void SessionManager::update_metrics(int fd, const lb::ServiceReport& report) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = sessions_by_fd.find(fd);
    if (it != sessions_by_fd.end()) {
        auto& metrics = it->second.metrics;
        metrics.cpu_usage = report.cpu_usage();
        metrics.memory_usage = report.memory_usage();
        metrics.active_requests = report.active_requests();
        metrics.last_report = std::chrono::steady_clock::now();

        it->second.last_seen = metrics.last_report;
    }
}

}