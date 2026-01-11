#include "scorer.h"

namespace lb::routing {
std::vector<BackendScore> Scorer::score_service_instances(const std::string& service_name) {
    auto& sm = lb::session::SessionManager::instance();
    std::vector<BackendScore> backend_scores;
    std::vector<int> fds = sm.get_instances_for_service(service_name);

    for(int fd : fds){
        lb::session::SessionInfo* session = sm.get_session_by_fd(fd);
        if(session){
            double score = calculate_score(session->metrics, session->state);
            backend_scores.push_back(BackendScore{session->fd, score});
        }
    }

    return backend_scores;
}

double Scorer::calculate_score(const lb::session::ServiceMetrics& metrics, lb::session::SessionState& state) {
    if(state != lb::session::SessionState::ACTIVE){
        return 0.0;
    }

    double usage = (metrics.cpu_usage + metrics.memory_usage) / 2.0;
    double score = 100.0 - usage;
    if(score < 0.0){
        score = 0.0;
    }
    
    return score;
}

}