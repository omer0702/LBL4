#pragma once
#include <cstdint>
#include <cstring>
#include <array>

namespace lb::protocol {


constexpr uint32_t PROTO_MAGIC = 0xAAAAAAAA;//has been changed
constexpr size_t FRAME_HEADER_SIZE = 12;

enum class MessageType : uint16_t {
    UNKNOWN = 0,
    INIT_REQ = 1,
    INIT_ACK = 2,
    KEEPALIVE_REQ = 3,
    KEEPALIVE_RESP = 4,
    GET_REPORTS_REQ = 5,
    REPORT = 6,
    CLOSE_REQ = 7,
    CLOSE_ACK = 8,
    ERROR = 0xFFFF
};

enum class DecodeResult {
    OK,
    NEED_MORE_DATA,
    INVALID_MAGIC,
    INVALID_HEADER,
    PAYLOAD_TOO_LARGE,
    PROTOBUF_PARSE_ERROR,
    UNKNOWN_TYPE
};

struct FrameHeader {
    uint32_t magic;
    uint16_t message_type;
    uint32_t payload_length;
    uint16_t reserved;

    FrameHeader() : magic(PROTO_MAGIC), message_type(0), payload_length(0), reserved(0) {}
};

// Convert 16/32 bit to network byte order
inline uint16_t to_be16(uint16_t v) {
    return (uint16_t)(((v & 0xff) << 8) | ((v & 0xff00) >> 8));
}
inline uint32_t to_be32(uint32_t v) {
    return ((v & 0x000000ff) << 24) |
           ((v & 0x0000ff00) << 8)  |
           ((v & 0x00ff0000) >> 8)  |
           ((v & 0xff000000) >> 24);
}
inline uint16_t from_be16(uint16_t v) { return to_be16(v); }
inline uint32_t from_be32(uint32_t v) { return to_be32(v); }

}
