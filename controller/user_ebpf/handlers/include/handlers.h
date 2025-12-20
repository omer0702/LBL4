#pragma once

#include <vector>
#include <cstdint>

#include "protocol_types.h"
#include "session_manager.h"

enum class HandlerResult {
    OK,
    CLOSE_CONNECTION,
    ERROR
};

namespace lb::handlers {
HandlerResult handle_init_req(int fd, const std::vector<uint8_t>& payload);
HandlerResult handle_close_req(int fd, const std::vector<uint8_t>& payload);
HandlerResult handle_keepalive_resp(int fd, const std::vector<uint8_t>& payload);
HandlerResult handle_get_report_resp(int fd, const std::vector<uint8_t>& payload);

ssize_t send_all(int fd, const uint8_t* data, size_t len);
ssize_t send_all(int fd, const std::vector<uint8_t>& bytes);
}