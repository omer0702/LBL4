#include "scorer.h"
#include "logger.hpp"


namespace lb::routing {
std::vector<BackendScore> Scorer::score_service_instances(const std::string& service_name) {
    auto& sm = lb::session::SessionManager::instance();
    std::vector<BackendScore> backend_scores;
    std::vector<int> fds = sm.get_instances_for_service(service_name);

    for(int fd : fds){
        lb::session::SessionInfo* session = sm.get_session_by_fd(fd);
        if(session){
            double score = calculate_score(session->metrics, session->state, service_name);
            backend_scores.push_back(BackendScore{sm.get_logical_id(session->fd), score});
        }
    }

    return backend_scores;
}

double Scorer::calculate_score(const lb::session::ServiceMetrics& metrics, lb::session::SessionState& state, const std::string& service_name) {
    if(state != lb::session::SessionState::ACTIVE){
        return 0.0;
    }

    double usage = (metrics.cpu_usage + metrics.memory_usage) / 2.0;
    double score = 100.0 - usage;
    if(score < 0.0){
        score = 0.0;
    }

    // if(score > 90.0){
    //     // lb::logger::Logger::GetInstance().log(lb::stats::CRITICAL, "HIGH_LOAD_ALERT", service_name,
    //     // "Service instance  is under high load: CPU {}%, Memory {}%", 
    //     //  metrics.cpu_usage, metrics.memory_usage);
    // }
    
    return score;
}

}