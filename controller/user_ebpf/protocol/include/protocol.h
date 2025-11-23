#pragma once
#include <cstdint>
#include <vector>
#include "protocol_types.h"

namespace lb::protocol {

bool protocol_init();

void protocol_shutdown();

//Build a framed buffer (header + payload bytes) from payload bytes and message type
std::vector<uint8_t> build_frame(MessageType type, const uint8_t* payload, uint32_t payload_len);

//try to parse header, returns DecodeResult and sets header/out_payload/out_len when OK
DecodeResult parse_frame_header(const uint8_t* data, size_t data_len, FrameHeader& out_header);

//append length-prefixed framed message to socket send buffer or return vector
std::vector<uint8_t> build_frame_from_vector(MessageType type, const std::vector<uint8_t>& payload);

}
