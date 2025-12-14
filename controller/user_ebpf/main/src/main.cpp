#include "io_epoll.h"
#include "protocol_encoder.h"
#include "service.pb.h"
#include <iostream>

using namespace lb;

int main() {
    uint16_t port = 8080;
    int listen_fd = lb::io_epoll::start_listen(port);
    if (listen_fd < 0) return 1;

    lb::io_epoll::MessageHandler handler = [](int fd, lb::protocol::MessageType msg_type, const std::vector<uint8_t>& payload){
        if (msg_type == lb::protocol::MessageType::INIT_REQ) {
            lb::InitRequest req;
            if (!req.ParseFromArray(payload.data(), payload.size())) {
                std::cerr << "bad init parse\n";
                return;
            }
            std::cout << "[HANDLER] init from " << req.service_name() << "\n";

            lb::InitResponse resp;
            if (req.service_name() == "serviceA") {
                resp.set_accepted(true);
                resp.set_session_token("TOKEN_ABC123");
                resp.set_reason("OK");
            } else {
                resp.set_accepted(false);
                resp.set_session_token("");
                resp.set_reason("forbidden");
            }
            auto out = lb::protocol::encoder::encode_init_ack(resp);
            lb::io_epoll::send_all(fd, out);
        }
        else if (msg_type == lb::protocol::MessageType::REPORT) {
            lb::ServiceReport r;
            if (r.ParseFromArray(payload.data(), payload.size())) {
                std::cout << "[HANDLER] report cpu=" << r.cpu_usage() << "\n";
                //update maps, DB and more
            }
        }
    };

    lb::io_epoll::run_loop(listen_fd, handler);
    return 0;
}
