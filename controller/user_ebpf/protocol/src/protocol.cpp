#include "protocol.h"
#include "protocol_types.h"
#include <iostream>

namespace lb::protocol {

bool protocol_init() {
    std::cout << "[protocol] init\n";
    return true;
}

void protocol_shutdown() {
    std::cout << "[protocol] shutdown\n";
}

std::vector<uint8_t> build_frame(MessageType type, const uint8_t* payload, uint32_t payload_len) {
    std::vector<uint8_t> buf;
    buf.reserve(FRAME_HEADER_SIZE + payload_len);

    FrameHeader header;
    header.magic = PROTO_MAGIC;
    header.message_type = static_cast<uint16_t>(type);
    header.payload_length = payload_len;
    header.reserved = 0;

    // serialize header to network byte order (big endian)
    uint32_t magic_be = to_be32(header.magic);
    uint16_t type_be = to_be16(header.message_type);
    uint32_t len_be = to_be32(header.payload_length);
    uint16_t res_be = to_be16(header.reserved);

    buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&magic_be), reinterpret_cast<uint8_t*>(&magic_be) + sizeof(magic_be));
    buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&type_be), reinterpret_cast<uint8_t*>(&type_be) + sizeof(type_be));
    buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&len_be), reinterpret_cast<uint8_t*>(&len_be) + sizeof(len_be));
    buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&res_be), reinterpret_cast<uint8_t*>(&res_be) + sizeof(res_be));

    if (payload && payload_len) {
        buf.insert(buf.end(), payload, payload + payload_len);
    }

    return buf;
}

DecodeResult parse_frame_header(const uint8_t* data, size_t data_len, FrameHeader& out_header) {
    if (data_len < FRAME_HEADER_SIZE) return DecodeResult::NEED_MORE_DATA;

    // read values (remember network byte order)
    uint32_t magic_be;
    uint16_t type_be;
    uint32_t len_be;
    uint16_t res_be;

    std::memcpy(&magic_be, data, sizeof(magic_be));
    std::memcpy(&type_be, data + 4, sizeof(type_be));
    std::memcpy(&len_be, data + 6, sizeof(len_be));
    std::memcpy(&res_be, data + 10, sizeof(res_be));

    out_header.magic = from_be32(magic_be);
    out_header.message_type = from_be16(type_be);
    out_header.payload_length = from_be32(len_be);
    out_header.reserved = from_be16(res_be);

    if (out_header.magic != PROTO_MAGIC) return DecodeResult::INVALID_MAGIC;
    if (out_header.payload_length > (1u<<24)) return DecodeResult::PAYLOAD_TOO_LARGE;
    return DecodeResult::OK;
}

std::vector<uint8_t> build_frame_from_vector(MessageType type, const std::vector<uint8_t>& payload) {
    return build_frame(type, payload.empty() ? nullptr : payload.data(), static_cast<uint32_t>(payload.size()));
}

}
