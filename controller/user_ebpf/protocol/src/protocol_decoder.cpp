#include "protocol_decoder.h"
#include "protocol_types.h"
#include <iostream>
#include <algorithm>
#include <protocol.h>

namespace lb::protocol::decoder {

DecodeResult try_decode_frame(const uint8_t* data, size_t data_len,
                             size_t& out_consumed,
                             MessageType& out_type,
                             std::vector<uint8_t>& out_payload) {
    out_consumed = 0;
    out_type = MessageType::UNKNOWN;
    out_payload.clear();

    FrameHeader hdr;
    DecodeResult r = ::lb::protocol::parse_frame_header(data, data_len, hdr);
    if (r != DecodeResult::OK) return r;

    size_t total_needed = FRAME_HEADER_SIZE + static_cast<size_t>(hdr.payload_length);
    if (data_len < total_needed) return DecodeResult::NEED_MORE_DATA;

    const uint8_t* payload_ptr = data + FRAME_HEADER_SIZE;
    out_payload.assign(payload_ptr, payload_ptr + hdr.payload_length);

    out_type = static_cast<MessageType>(hdr.message_type);
    out_consumed = total_needed;
    return DecodeResult::OK;
}

} 
