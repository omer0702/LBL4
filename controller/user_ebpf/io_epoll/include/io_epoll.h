#pragma once
#include <functional>
#include <vector>
#include <cstdint>
#include <unordered_map>

#include "protocol.h"
#include "maps_manager.hpp"

namespace lb::io_epoll {

using MessageType = lb::protocol::MessageType;
using DecodeResult = lb::protocol::DecodeResult;

struct ConnectionState{
    std::vector<uint8_t> buffer;
    uint32_t ip;
    uint16_t port;
    uint8_t* mac;
};


int start_listen(uint16_t port);
void run_loop(int listen_fd, MapsManager& maps_manager);
void stop_loop();
void choose_handler(int fd, MessageType type, const std::vector<uint8_t>& payload, int epfd,
                    std::unordered_map<int, ConnectionState>& conn_map,
                    int& active_connections, MapsManager& maps_manager);
void close_client(int fd, int epfd, std::unordered_map<int, ConnectionState>& conn_map, int& active_connections, MapsManager& maps_manager);
void setup_timer(int epfd);

// ssize_t send_all(int fd, const uint8_t* data, size_t len);
// ssize_t send_all(int fd, const std::vector<uint8_t>& bytes);

}
