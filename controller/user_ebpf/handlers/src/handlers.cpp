#include "handlers.h"
#include "protocol_encoder.h"
#include <iostream>
#include "service.pb.h"
#include <sys/socket.h>
#include "../../../model/routing_logic/include/scorer.h"
#include "../../../model/routing_logic/include/maglev_builder.h"


using namespace lb::protocol;

namespace lb::handlers {
ssize_t send_all(int fd, const uint8_t* data, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = send(fd, data + total, len - total, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000);
                continue;
            }
            return -1;
        }
        total += (size_t)n;
    }
    return (ssize_t)total;
}

ssize_t send_all(int fd, const std::vector<uint8_t>& bytes) {
    return send_all(fd, bytes.data(), bytes.size());
}

HandlerResult handle_init_req(int fd, const std::vector<uint8_t>& payload, uint32_t ip, uint16_t port) {
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

    auto token = sm.create_session(fd, req.service_name(), ip, port);

    lb::InitResponse resp;
    resp.set_accepted(true);
    resp.set_session_token(token);
    resp.set_reason("OK");

    auto bytes = encoder::encode_init_ack(resp);
    send_all(fd, bytes);

    return HandlerResult::OK;
}

HandlerResult handle_close_req(int fd, const std::vector<uint8_t>& payload) {
    auto& sm = lb::session::SessionManager::instance();
    
    lb::CloseRequest req;
    if (!req.ParseFromArray(payload.data(), payload.size())) {
        std::cerr << "[HANDLER] Failed to parse CloseRequest\n";
        return HandlerResult::CLOSE_CONNECTION;
    }
    std::cout<< "[HANDLER] CloseRequest received from fd: " << fd << "\n";//maybe add token(add token to service.proto at CloseRequest)
    auto* session = sm.get_session_by_fd(fd);

    if(!sm.has_session(fd)) {
        std::cerr << "[HANDLER] no session for fd: " << fd << "\n";
        return HandlerResult::CLOSE_CONNECTION;
    }

    if(session->token != req.session_token()) {//so that one client doesnt close another's session
        std::cerr << "[HANDLER] invalid session token for fd: " << fd << "expected: " << session->token << " got: " << req.session_token() << "\n";
        lb::CloseAck resp;
        resp.set_ok(false);
        auto bytes = encoder::encode_close_ack(resp);
        send_all(fd, bytes);

        return HandlerResult::CLOSE_CONNECTION;//maybe change to OK
    }

    lb::CloseAck resp;
    resp.set_ok(true);
    auto bytes = encoder::encode_close_ack(resp);
    send_all(fd, bytes);

    sm.remove_session(fd);
    std::cout<< "[HANDLER] Session for fd " << fd << " closed successfully" << "\n";
    
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
    lb::ServiceReport report;
    if (!report.ParseFromArray(payload.data(), payload.size())) {
        std::cerr << "[HANDLER] Failed to parse GetReport\n";
        return HandlerResult::ERROR;
    }

    auto& sm = lb::session::SessionManager::instance();
    if(!sm.has_session(fd)) {
        std::cerr << "[HANDLER] no session for fd: " << fd << "\n";
        return HandlerResult::ERROR;
    }

    sm.update_metrics(fd, report);
    std::vector<BackendScore> scores = lb::routing::Scorer::score_service_instances("serviceA");
    std::cout << "----- Backend Scores for serviceA -----\n";
    for(const auto& score : scores){
        std::cout << "FD: " << score.backend_id << ", Score: " << score.score << "\n";
    }
    MaglevBuilder builder(scores);
    std::vector<uint32_t> table = builder.build_table();
    builder.test_maglev_builder(table, scores);

    //sm.print_session_stats();
    lb::GetReportAck resp;
    resp.set_ok(true);
    auto bytes = encoder::encode_get_reports_resp(resp);
    send_all(fd, bytes);

    std::cout<<"[HANDLER] metrics updated fd=" << fd << "\n";

    return HandlerResult::OK;
}

}