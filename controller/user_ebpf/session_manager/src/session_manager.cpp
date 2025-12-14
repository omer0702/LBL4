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

}