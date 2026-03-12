#pragma once

#include "internal_stats.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <string>
#include <memory>

namespace lb::logger{

class Logger {
public:
    static Logger& GetInstance();

    void init(const std::string& server_address);
    void log(lb::stats::Severity severity, const std::string& type, const std::string& service_name, const std::string& message, const std::string& metadata_json = "");

private:
    Logger() = default;
    std::unique_ptr<lb::stats::StatsCollector::Stub> logger_stub;
    bool initialized = false;
};

}