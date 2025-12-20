#include "protocol_encoder.h"
#include "protocol.h"
#include <iostream>
#include <service.pb.h>

namespace lb::protocol::encoder {

static std::vector<uint8_t> serialize_protobuf_to_frame(const google::protobuf::Message& msg, MessageType type) {
    size_t sz = msg.ByteSizeLong();
    std::vector<uint8_t> payload;
    payload.resize(sz);
    if (!msg.SerializeToArray(payload.data(), static_cast<int>(sz))) {
        std::cerr << "[protocol::encoder] protobuf serialize failed\n";
        return {};
    }
    return build_frame_from_vector(type, payload);
}

std::vector<uint8_t> encode_init_request(const ::lb::InitRequest& msg) {
    return serialize_protobuf_to_frame(msg, MessageType::INIT_REQ);
}
std::vector<uint8_t> encode_init_ack(const ::lb::InitResponse& msg) {
    return serialize_protobuf_to_frame(msg, MessageType::INIT_ACK);
}
std::vector<uint8_t> encode_keepalive_req(const ::lb::KeepAlive& msg) {
    return serialize_protobuf_to_frame(msg, MessageType::KEEPALIVE_REQ);
}
std::vector<uint8_t> encode_get_reports_req(const ::lb::GetReport& msg) {
    return serialize_protobuf_to_frame(msg, MessageType::GET_REPORTS_REQ);
}
std::vector<uint8_t> encode_report(const ::lb::ServiceReport& msg) {
    return serialize_protobuf_to_frame(msg, MessageType::REPORT);
}
std::vector<uint8_t> encode_get_reports_resp(const ::lb::GetReportAck& msg) {
    return serialize_protobuf_to_frame(msg, MessageType::GET_REPORTS_RESP);
}
std::vector<uint8_t> encode_close_req(const ::lb::CloseRequest& msg) {
    return serialize_protobuf_to_frame(msg, MessageType::CLOSE_REQ);
}
std::vector<uint8_t> encode_close_ack(const ::lb::CloseAck& msg) {
    return serialize_protobuf_to_frame(msg, MessageType::CLOSE_ACK);
}

}
