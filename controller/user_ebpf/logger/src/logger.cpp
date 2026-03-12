#include "logger.hpp"

namespace lb::logger{

Logger& Logger::GetInstance(){
    static Logger instance;
    return instance;
}

void Logger::init(const std::string& server_address){
    if(initialized){
        return;
    }
    auto channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
    logger_stub = lb::stats::StatsCollector::NewStub(channel);
    initialized = true;
}

void Logger::log(lb::stats::Severity severity, const std::string& type, const std::string& service_name, const std::string& message, const std::string& metadata_json){
    if(!initialized){
        return;
    }

    lb::stats::EventRequest request;
    request.set_timestamp(time(nullptr));
    request.set_severity(severity);
    request.set_event_type(type);
    request.set_service_name(service_name);
    request.set_message(message);
    if(!metadata_json.empty()){
        request.set_metadata_json(metadata_json);
    }

    lb::stats::LogResponse response;
    grpc::ClientContext context;
    grpc::Status status = logger_stub->LogEvent(&context, request, &response);

    if (!status.ok()) {
        std::cerr << "Failed to log event: " << status.error_message() << std::endl;
    }
}

}