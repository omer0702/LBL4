#include "handlers.h"
#include "protocol_encoder.h"
#include <iostream>
#include "service.pb.h"
#include "io_epoll.h"

using namespace lb::protocol;

namespace lb::handlers {

HandlerResult handle_init_req(int fd, const std::vector<uint8_t>& payload) {
    lb::InitRequest req;
    if (!req.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
        std::cerr << "[HANDLER] Failed to parse InitRequest\n";
        return HandlerResult::ERROR;
    }

    std::cout << "[HANDLER] InitRequest from service: " << req.service_name() << "\n";

    auto& sm = lb::session::SessionManager::instance();
    if(sm.has_session(fd)) {
        std::cerr << "[HANDLER] Session already exists: " << req.service_name() << "\n";
        return HandlerResult::ERROR;
    }

    auto session = sm.create_session(fd, req.service_name());

    lb::InitResponse resp;
    resp.set_accepted(true);
    resp.set_session_token(session);
    resp.set_reason("OK");

    auto bytes = encoder::encode_init_ack(resp);
    lb::io_epoll::send_all(fd, bytes);

    return HandlerResult::OK;
}

HandlerResult handle_close_req(int fd, const std::vector<uint8_t>& payload) {
    std::cout<<"[HANDLER] CloseRequest received, closing connection: fd=" << fd << "\n";
    auto& sm = lb::session::SessionManager::instance();
    
    if(!sm.has_session(fd)) {
        std::cerr << "[HANDLER] no session for fd: " << fd << "\n";
        return HandlerResult::CLOSE_CONNECTION;
    }

    lb::CloseAck resp;
    resp.set_ok(true);
    auto bytes = encoder::encode_close_ack(resp);
    lb::io_epoll::send_all(fd, bytes);

    sm.remove_session(fd);

    return HandlerResult::CLOSE_CONNECTION;
}

HandlerResult handle_keepalive_resp(int fd, const std::vector<uint8_t>& payload) {
    auto& sm = lb::session::SessionManager::instance();
    if(!sm.has_session(fd)) {
        std::cerr << "[HANDLER] no session for fd: " << fd << "\n";
        return HandlerResult::ERROR;
    }

    sm.update_last_seen(fd);
    std::cout << "[HANDLER] KeepaliveResponse received, updated last seen: fd=" << fd << "\n";

    return HandlerResult::OK;
}


HandlerResult handle_get_report_resp(int fd, const std::vector<uint8_t>& payload) {
    lb::GetReport report;
    if (!report.ParseFromArray(payload.data(), payload.size())) {
        std::cerr << "[HANDLER] Failed to parse GetReport\n";
        return HandlerResult::ERROR;
    }

    auto& sm = lb::session::SessionManager::instance();
    if(!sm.has_session(fd)) {
        std::cerr << "[HANDLER] no session for fd: " << fd << "\n";
        return HandlerResult::ERROR;
    }

    // sm.update_metrics(fd, report);
    // lb::ReportAck resp;
    // resp.set_received(true);
    // auto bytes = encoder::encode_report_ack(resp);
    // lb::io_epoll::send_all(fd, bytes);

    std::cout<<"[HANDLER] metrics updated fd=" << fd << "\n";

    return HandlerResult::OK;
}

}