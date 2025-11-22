#pragma once
#include <vector>
#include "protocol_types.h"

#include "service.pb.h" 

namespace lb::protocol::encoder {

std::vector<uint8_t> encode_init_request(const ::lb::InitRequest& msg);
std::vector<uint8_t> encode_init_ack(const ::lb::InitResponse& msg);
std::vector<uint8_t> encode_keepalive_req(const ::lb::KeepAlive& msg);
std::vector<uint8_t> encode_get_reports_req(const ::lb::GetReport& msg);
std::vector<uint8_t> encode_report(const ::lb::ServiceReport& msg);
std::vector<uint8_t> encode_close_req(const ::lb::CloseRequest& msg);
std::vector<uint8_t> encode_close_ack(const ::lb::CloseAck& msg);

}
