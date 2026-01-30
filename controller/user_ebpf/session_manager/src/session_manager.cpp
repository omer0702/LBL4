#include "session_manager.h"
#include <random>
#include <iomanip>
// #include "../../../model/routing_logic/include/scorer.h"
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


std::string SessionManager::create_session(int fd, const std::string& service_name, uint32_t ip, uint16_t port) {
    std::lock_guard<std::mutex> lock(mtx);
    std::string token = generate_token();

    auto info = std::make_unique<SessionInfo>(fd, service_name, token, ip, port);

    session_mapping[fd] = std::move(info);
    service_groups[service_name].push_back(fd);

    return token;
}


SessionInfo* SessionManager::get_session_by_fd(int fd) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = session_mapping.find(fd);
    if (it == session_mapping.end()) return nullptr;

    return it->second.get();
}


// std::optional<SessionInfo> SessionManager::get_session_by_token(const std::string& token) {
//     std::lock_guard<std::mutex> lock(mtx);
//     auto it = fd_by_token.find(token);
//     if (it == fd_by_token.end()) return std::nullopt;

//     return sessions_by_fd[it->second];
// }


std::vector<int> SessionManager::get_expired_sessions(int timeout_seconds){
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<int> expired_fds;
    auto now = std::chrono::steady_clock::now();

    for(const auto& [fd, session] : session_mapping){
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - session->last_seen).count();
        //std::cout << duration << " seconds since last seen for fd=" << fd << "\n";
        if(duration > timeout_seconds){
            expired_fds.push_back(fd);
        }
    }

    return expired_fds;
}

std::vector<int> SessionManager::get_all_session_fds(){
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<int> fds;
    for(const auto& [fd, session] : session_mapping){
        fds.push_back(fd);
    }
    
    return fds;
}

std::vector<int> SessionManager::get_instances_for_service(const std::string& service_name){
    std::lock_guard<std::mutex> lock(mtx);
    auto it = service_groups.find(service_name);
    if(it == service_groups.end()){
        return {};
    }

    return it->second;
}
bool SessionManager::has_session(int fd) {
    std::lock_guard<std::mutex> lock(mtx);
    return session_mapping.find(fd) != session_mapping.end();
}

void SessionManager::update_last_seen(int fd) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = session_mapping.find(fd);
    if (it != session_mapping.end()) {
        it->second->last_seen = std::chrono::steady_clock::now();
    }
}


void SessionManager::remove_session(int fd) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = session_mapping.find(fd);
    if (it == session_mapping.end()) return;

    std::string name = it->second->service_name;
    auto& group = service_groups[name];
    group.erase(std::remove(group.begin(), group.end(), fd), group.end());

    if(group.empty()){
        service_groups.erase(name);
    }

    session_mapping.erase(it);
}

void SessionManager::update_metrics(int fd, const lb::ServiceReport& report) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = session_mapping.find(fd);
    if (it != session_mapping.end()) {//use has_session instead
        auto& metrics = it->second->metrics;
        metrics.cpu_usage = report.cpu_usage();
        metrics.memory_usage = report.memory_usage();
        metrics.active_requests = report.active_requests();
        metrics.last_report = std::chrono::steady_clock::now();

        it->second->last_seen = metrics.last_report;
    }
}

void SessionManager::print_session_stats(){
    std::lock_guard<std::mutex> lock(mtx);
    std::cout << "----- Session Statistics -----\n";//prints even if there is no sessions
    for(const auto& [fd, session] : session_mapping){
        const auto& metrics = session->metrics;
        std::cout << "FD: " << fd
                  << ", Service: " << session->service_name
                  << ", CPU: " << metrics.cpu_usage << "%"
                  << ", Memory: " << metrics.memory_usage << "MB"
                  << (session->state == SessionState::ACTIVE ? ", State: ACTIVE" : ", State: SUSPECTED")
                  <<"\n";
    }

    // std::vector<BackendScore> scores = lb::routing::Scorer::score_service_instances("serviceA");
    // std::cout << "----- Backend Scores for serviceA -----\n";
    // for(const auto& score : scores){
    //     std::cout << "FD: " << score.backend_id << ", Score: " << score.score << "\n";
    // }
}


void SessionManager::register_service_vip(const std::string& service_name, uint32_t vip){
    std::lock_guard<std::mutex> lock(mtx);
    service_vips[service_name] = vip;
}

uint32_t SessionManager::get_service_vip(const std::string& service_name){
    std::lock_guard<std::mutex> lock(mtx);
    auto it = service_vips.find(service_name);
    if(it != service_vips.end()){
        return it->second;
    }

    return 0;
}

std::unordered_map<std::string, uint32_t> SessionManager::get_all_service_vips(){
    std::lock_guard<std::mutex> lock(mtx);
    return service_vips;
}

uint32_t SessionManager::allocate_service_vip(){
    if(next_vip_suffix >= start_suffix + max_services){
        return 0;
    }

    return next_vip_suffix.fetch_add(1);
}
}