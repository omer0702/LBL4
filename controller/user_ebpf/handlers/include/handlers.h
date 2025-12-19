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
}