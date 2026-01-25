#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <thread>
#include <dlfcn.h>
#include <string>

#include "protocol_decoder.h"
#include "protocol_encoder.h"
#include "service.pb.h"
#include "protocol_types.h"

void start_udp_receiver(uint16_t port, const std::string& ip_addr) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return;

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip_addr.c_str(), &addr.sin_addr);

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[UDP] Failed to bind to " << ip_addr << ":" << port << " (Check if IP exists)\n";
        return;
    }

    std::cout << "[UDP] Receiver ready on " << ip_addr << ":" << port << "\n";

    char buffer[4096];
    while (true) {
        sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int n = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&client_addr, &addr_len);
        if (n > 0) {
            std::cout << "\n[UDP] >>> SUCCESS! Packet received at Backend (" << ip_addr << ") <<<\n";
            std::cout << "[UDP] Content: " << std::string(buffer, n) << "\n";
        }
    }
}

void handle_server_messages(int sock, std::string& token) {
    uint8_t buffer[4096];
    while (true) {
        int n = recv(sock, buffer, sizeof(buffer), 0);
        if (n <= 0) break;

        size_t consumed = 0;
        lb::protocol::MessageType type;
        std::vector<uint8_t> payload;

        while (consumed < (size_t)n) {
            auto result = lb::protocol::decoder::try_decode_frame(buffer + consumed, n - consumed, consumed, type, payload);
            if (result != lb::protocol::DecodeResult::OK) break;

            if (type == lb::protocol::MessageType::KEEPALIVE_REQ) {
                lb::KeepAliveResp resp;
                resp.set_session_token(token);
                auto bytes = lb::protocol::encoder::encode_keepalive_resp(resp);
                send(sock, bytes.data(), bytes.size(), 0);
            } 
            else if (type == lb::protocol::MessageType::GET_REPORTS_REQ) {
                lb::ServiceReport report;
                report.set_session_token(token);
                report.set_cpu_usage(20);
                report.set_memory_usage(10);
                auto bytes = lb::protocol::encoder::encode_report(report);
                send(sock, bytes.data(), bytes.size(), 0);
            }
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./protocol_client <local_ip_to_bind>\n";
        std::cerr << "Example: ./protocol_client 127.0.0.2\n";
        return 1;
    }
    std::string my_ip = argv[1];

    std::thread udp_thread(start_udp_receiver, 9000, my_ip);
    udp_thread.detach();

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in local_addr = {};
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = 0;
    if(inet_pton(AF_INET, my_ip.c_str(), &local_addr.sin_addr) <= 0){
        std::cerr << "Invlaid local ip address\n";
        return 1;
    }

    if(bind(sock, (sockaddr*)&local_addr, sizeof(local_addr)) < 0){
        perror("bind TCP sokcet failed");
        close(sock);
        return 1;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Could not connect to Controller\n";
        close(sock);
        return 1;
    }

    lb::InitRequest req;
    req.set_service_name("serviceA");
    auto bytes = lb::protocol::encoder::encode_init_request(req);
    send(sock, bytes.data(), bytes.size(), 0);

    uint8_t res_buf[1024];
    int n = recv(sock, res_buf, sizeof(res_buf), 0);
    size_t cons = 0; lb::protocol::MessageType t; std::vector<uint8_t> p;
    std::string token = "none";
    
    if (lb::protocol::decoder::try_decode_frame(res_buf, n, cons, t, p) == lb::protocol::DecodeResult::OK) {
        lb::InitResponse resp;
        resp.ParseFromArray(p.data(), p.size());
        token = resp.session_token();
        std::cout << "[CLIENT] Registered as " << my_ip << ". Token: " << token << "\n";
    }

    handle_server_messages(sock, token);
    close(sock);
    return 0;
}