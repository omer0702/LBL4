#pragma once
#include <vector>
#include <cstdint>
#include "protocol_types.h"

namespace lb::protocol::decoder {


DecodeResult try_decode_frame(const uint8_t* data, size_t data_len,
                             size_t& out_consumed,
                             MessageType& out_type,
                             std::vector<uint8_t>& out_payload);

}