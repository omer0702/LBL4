#pragma once
#include <functional>
#include <vector>
#include <cstdint>
#include <unordered_map>

#include "protocol.h"

namespace lb::io_epoll {

using MessageType = lb::protocol::MessageType;
using DecodeResult = lb::protocol::DecodeResult;

using MessageHandler = std::function<void(int fd, MessageType msg_type, const std::vector<uint8_t>& payload)>;

int start_listen(uint16_t port); // returns listen_fd or -1
void run_loop(int listen_fd, MessageHandler handler); // blocking loop
void stop_loop(); // polite stop (sets internal flag)
ssize_t send_all(int fd, const uint8_t* data, size_t len); // helper to send all bytes
ssize_t send_all(int fd, const std::vector<uint8_t>& bytes);

}
