#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <thread>

#include "protocol_decoder.h"
#include "protocol_encoder.h"
#include "service.pb.h"
#include "protocol_types.h"

struct TestConfig {
    bool send_init = true;          
    bool respond_keepalive = true; 
    bool crash_after_init = false; 
    int sleep_before_report = 1;
};

TestConfig config = {true, true, false, 1}; 

void run_close_scenario(int sock, const std::string& token, bool use_correct_token) {
    lb::CloseRequest req;
    req.set_service_id("serviceA");
    req.set_reason("Normal shutdown");
    if(use_correct_token) {
        req.set_session_token(token);
        std::cout << "[CLIENT] Using correct session token in CloseRequest\n";
    } else {
        req.set_session_token("invalid_token");
        std::cout << "[CLIENT] Using INVALID session token in CloseRequest\n";
    }

    auto bytes = lb::protocol::encoder::encode_close_req(req);
    send(sock, bytes.data(), bytes.size(), 0);
    std::cout << "[CLIENT] Sent CloseRequest\n";

    sleep(2);
}

void handle_server_messages(int sock, std::string& token) {
    uint8_t buffer[4096];
    int i = 0;

    while (true) {
        int n = recv(sock, buffer, sizeof(buffer), 0);
        if (n <= 0) {
            std::cout << "[CLIENT] Server closed connection or error.\n";
            break;
        }

        size_t consumed = 0;
        lb::protocol::MessageType type;
        std::vector<uint8_t> payload;

        while (consumed < (size_t)n) {
            auto result = lb::protocol::decoder::try_decode_frame(buffer + consumed, n - consumed, consumed, type, payload);
            if (result != lb::protocol::DecodeResult::OK) break;

            if (type == lb::protocol::MessageType::KEEPALIVE_REQ) {
                std::cout << "[CLIENT] Got KeepAliveRequest\n";
                i++;
                if (config.respond_keepalive) {
                    lb::KeepAliveResp resp;
                    resp.set_session_token(token);
                    auto bytes = lb::protocol::encoder::encode_keepalive_resp(resp);
                    send(sock, bytes.data(), bytes.size(), 0);
                    std::cout << "[CLIENT] Sent KeepAliveResponse\n";
                } else {
                    std::cout << "[CLIENT] Ignoring KeepAlive (Zombie mode)\n";
                }
            } 
            else if (type == lb::protocol::MessageType::GET_REPORTS_REQ) {
                if (config.respond_keepalive) {
                    std::cout << "[CLIENT] Got GetReportRequest\n";
                    lb::ServiceReport report;
                    report.set_session_token(token);
                    report.set_cpu_usage(30.5);
                    report.set_memory_usage(400);
                    report.set_active_requests(12);
                    
                    auto bytes = lb::protocol::encoder::encode_report(report);
                    send(sock, bytes.data(), bytes.size(), 0);
                    std::cout << "[CLIENT] Sent ServiceReport\n";
                }
            }
            else if(type == lb::protocol::MessageType::CLOSE_ACK){
                std::cout << "[CLIENT] Received CloseAck from server. Closing connection.\n";
                return;
            }
            if(i > 2 ){
                run_close_scenario(sock, token, true);
                return;
            }
        }
    }
}

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) return 1;

    std::string token = "no_token";

   
    if (config.send_init) {
        lb::InitRequest req;
        req.set_service_name("serviceA");
        auto bytes = lb::protocol::encoder::encode_init_request(req);
        send(sock, bytes.data(), bytes.size(), 0);
       
        uint8_t res_buf[1024];
        int n = recv(sock, res_buf, sizeof(res_buf), 0);
        size_t cons = 0; lb::protocol::MessageType t; std::vector<uint8_t> p;
        if (lb::protocol::decoder::try_decode_frame(res_buf, n, cons, t, p) == lb::protocol::DecodeResult::OK) {
            lb::InitResponse resp;
            resp.ParseFromArray(p.data(), p.size());
            token = resp.session_token();
            std::cout << "[CLIENT] Connected. Token: " << token << "\n";
        }
    } else {
        lb::ServiceReport report;
        report.set_session_token("fake_token");
        auto bytes = lb::protocol::encoder::encode_report(report);
        send(sock, bytes.data(), bytes.size(), 0);
        std::cout << "[CLIENT] Sent Report WITHOUT Init (Scenario 4)\n";
        sleep(2);
        
        return 0;
    }

    if (config.crash_after_init) {
        std::cout << "[CLIENT] Crashing now (Scenario 3)...\n";
        return 0;
    }

    handle_server_messages(sock, token);

    close(sock);
    return 0;
}