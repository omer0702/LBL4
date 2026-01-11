#pragma once

#include<vector>
#include<string>
#include "maglev_builder.h"
#include "../../../controller/user_ebpf/session_manager/include/session_manager.h"

namespace lb::routing {
class Scorer {
public:
    static std::vector<BackendScore> score_service_instances(const std::string& service_name);
private:
    static double calculate_score(const lb::session::ServiceMetrics& metrics, lb::session::SessionState& state);
};

}