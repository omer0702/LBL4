#include <iostream>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>

#include "protocol_decoder.h"
#include "protocol_encoder.h"
#include "service.pb.h"
#include "protocol_types.h"
#include <protocol.h>

int main(){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if(connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Connection failed\n";
        close(sock);
        return 1;
    }

    lb::InitRequest req;
    req.set_service_name("serviceA");
    req.set_instance_id("abc123");
    req.set_session_token("");

    auto bytes = lb::protocol::encoder::encode_init_request(req);
    
    if(send(sock, bytes.data(), bytes.size(), 0) != (ssize_t)bytes.size()) {
        std::cerr << "Send failed\n";
        close(sock);
        return 1;
    }
    std::cout << "send success\n";


    uint8_t buffer[1024];
    int n = recv(sock, buffer, sizeof(buffer), 0);
    if (n <= 0) {
        std::cerr << "Receive failed\n";
        return 1;
    }

    lb::protocol::MessageType type;
    std::vector<uint8_t> payload;
    size_t consumed = 0;

    auto result = lb::protocol::decoder::try_decode_frame(buffer, n, consumed, type, payload);
    if(result != lb::protocol::DecodeResult::OK) {
        std::cerr << "Failed to decode frame\n";
        return 1;
    }
    
    lb::InitResponse resp;
    if(!resp.ParseFromArray(payload.data(), payload.size())) {
        std::cerr << "Failed to parse InitResponse\n";
        return 1;
    }

    std::cout << "InitResponse: accepted=" << resp.accepted() << ", token=" << resp.session_token() << "\n";
    //maybe add print of 'reason' if accepted = 0

    close(sock);
    return 0;
}

