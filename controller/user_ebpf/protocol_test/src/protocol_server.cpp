#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <vector>
#include <string>

#include "protocol.h"
#include "protocol_decoder.h"
#include "protocol_encoder.h"
#include "service.pb.h"

using namespace lb::protocol;

static const int PORT = 8080;

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { 
        perror("socket"); return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    listen(server_fd, 10);
    std::cout << "[SERVER] Listening on port " << PORT << std::endl;

    int client_fd = accept(server_fd, nullptr, nullptr);
    std::cout << "[SERVER] Client connected" << std::endl;

    uint8_t buffer[2048];

    while (true) {
        int n = recv(client_fd, buffer, sizeof(buffer), 0);
        if (n <= 0) break;

        lb::protocol::FrameHeader hdr;
        size_t consumed = 0;
        lb::protocol::MessageType type;
        std::vector<uint8_t> payload;

        lb::protocol::DecodeResult result = decoder::try_decode_frame(buffer, n, consumed, type, payload);

        if (result != lb::protocol::DecodeResult::OK) {
            std::cout << "[SERVER] decode failed\n";
            continue;
        }
        std::cout << "[SERVER] Received message type: " << (int)type << std::endl;


        lb::InitRequest init_req;
        if (!init_req.ParseFromArray(payload.data(), payload.size())) {
            std::cout << "[SERVER] protobuf parse failed\n";
            continue;
        }
        std::cout << "[SERVER] Init from service: " << init_req.service_name() << std::endl;

        lb::InitResponse resp;

        if (init_req.service_name() == "serviceA") {
            resp.set_accepted(true);
            resp.set_session_token("TOKEN_ABC123");
            resp.set_reason("OK");
        } else {
            resp.set_accepted(false);
            resp.set_session_token("");
            resp.set_reason("Service not allowed");
        }

        auto out_bytes = encoder::encode_init_ack(resp);

        send(client_fd, out_bytes.data(), out_bytes.size(), 0);
        std::cout << "[SERVER] sent InitResponse" << std::endl;
    }

    close(client_fd);
    close(server_fd);
    return 0;
}
