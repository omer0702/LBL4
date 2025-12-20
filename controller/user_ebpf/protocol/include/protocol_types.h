#pragma once
#include <cstdint>
#include <cstring>
#include <array>
#include <endian.h>

namespace lb::protocol {


constexpr uint32_t FRAME_SIGNATURE = 0xAAAAAAAA;
constexpr size_t FRAME_HEADER_SIZE = 12;

enum class MessageType : uint16_t {
    UNKNOWN = 0,
    INIT_REQ = 1,
    INIT_ACK = 2,
    KEEPALIVE_REQ = 3,
    KEEPALIVE_RESP = 4,
    GET_REPORTS_REQ = 5,
    REPORT = 6,
    GET_REPORTS_RESP = 7,
    CLOSE_REQ = 8,
    CLOSE_ACK = 9,
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
    uint32_t frame_signature;
    uint16_t message_type;//change uint16_t to MessageType
    uint32_t payload_length;
    uint16_t reserved;

    FrameHeader() : frame_signature(FRAME_SIGNATURE), message_type(0), payload_length(0), reserved(0) {}
};

//Convert 16/32 bit to network byte order
// inline uint16_t to_be16(uint16_t v) {
//     return (uint16_t)(((v & 0xff) << 8) | ((v & 0xff00) >> 8));
// }
// inline uint32_t to_be32(uint32_t v) {
//     return ((v & 0x000000ff) << 24) |
//            ((v & 0x0000ff00) << 8)  |
//            ((v & 0x00ff0000) >> 8)  |
//            ((v & 0xff000000) >> 24);
// }

inline uint16_t to_be16(uint16_t v) {return htobe16(v);}
inline uint16_t from_be16(uint16_t v) { return be16toh(v); }

inline uint32_t to_be32(uint32_t v) { return htobe32(v); }
inline uint32_t from_be32(uint32_t v) { return be32toh(v); }

}
