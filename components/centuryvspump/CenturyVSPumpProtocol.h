#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace esphome::century_vs_pump::protocol {

static constexpr uint8_t REQUEST = 0x20;
static constexpr uint8_t ACK = 0x10;
static constexpr uint8_t TEMPORARILY_BUSY = 0x06;

enum class ResponseValidation : uint8_t {
  OK,
  TOO_SHORT,
  FUNCTION_MISMATCH,
  NACK,
};

inline std::vector<uint8_t> make_request_pdu(uint8_t function,
                                             std::span<const uint8_t> payload) {
  std::vector<uint8_t> request{function, REQUEST};
  request.insert(request.end(), payload.begin(), payload.end());
  return request;
}

inline bool request_matches(uint8_t function, std::span<const uint8_t> payload,
                            std::span<const uint8_t> request_pdu) {
  const auto expected = make_request_pdu(function, payload);
  return request_pdu.size() == expected.size() &&
         std::equal(request_pdu.begin(), request_pdu.end(), expected.begin());
}

inline ResponseValidation
validate_response(uint8_t expected_function,
                  std::span<const uint8_t> response_pdu) {
  if (response_pdu.size() < 2)
    return ResponseValidation::TOO_SHORT;
  if (response_pdu[0] != expected_function)
    return ResponseValidation::FUNCTION_MISMATCH;
  if (response_pdu[1] != ACK)
    return ResponseValidation::NACK;
  return ResponseValidation::OK;
}

inline bool deadline_reached(uint32_t now, uint32_t deadline) {
  return deadline == 0 || static_cast<int32_t>(now - deadline) >= 0;
}

inline bool superseded_by_stop(uint8_t function, bool stop_requested) {
  return stop_requested && (function == 0x41 || function == 0x44);
}

inline bool stop_pending_after_terminal(uint8_t function,
                                        bool stop_requested) {
  return function == 0x42 ? false : stop_requested;
}

} // namespace esphome::century_vs_pump::protocol
