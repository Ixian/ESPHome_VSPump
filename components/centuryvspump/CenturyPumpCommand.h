#pragma once

#include "CenturyVSPumpProtocol.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace esphome::century_vs_pump {

class CenturyVSPump;

enum class CenturyPumpCommandKind : uint8_t {
  POLL,
  CONTROL,
};

class CenturyPumpCommand {
public:
  using ResponseCallback =
      std::function<void(CenturyVSPump *pump,
                         const std::vector<uint8_t> &data)>;

  static constexpr uint8_t MAX_TRANSMISSIONS = 5;
  static constexpr uint8_t MAX_BUSY_RETRIES = 3;

  CenturyVSPump *pump_{};
  uint8_t function_{};
  std::vector<uint8_t> payload_{};
  std::vector<ResponseCallback> on_data_funcs_;
  const void *poll_consumer_{};
  std::vector<const void *> poll_consumers_;
  CenturyPumpCommandKind kind_{CenturyPumpCommandKind::POLL};
  uint8_t transmission_count_{0};
  uint8_t busy_retry_count_{0};
  uint8_t queue_refusal_count_{0};
  uint32_t not_before_{0};

  std::vector<uint8_t> pdu() const {
    return protocol::make_request_pdu(this->function_, this->payload_);
  }
  bool matches_pdu(std::span<const uint8_t> request_pdu) const {
    return protocol::request_matches(this->function_, this->payload_,
                                     request_pdu);
  }
  void add_poll_consumer(const CenturyPumpCommand &command) {
    if (command.poll_consumer_ == nullptr ||
        std::find(this->poll_consumers_.begin(), this->poll_consumers_.end(),
                  command.poll_consumer_) != this->poll_consumers_.end())
      return;
    this->poll_consumers_.push_back(command.poll_consumer_);
    this->on_data_funcs_.insert(this->on_data_funcs_.end(),
                                command.on_data_funcs_.begin(),
                                command.on_data_funcs_.end());
  }

  static CenturyPumpCommand create_status_command(
      CenturyVSPump *pump,
      std::function<void(CenturyVSPump *pump, bool running)> on_status_func);
  static CenturyPumpCommand create_read_sensor_command(
      CenturyVSPump *pump, uint8_t page, uint8_t address, uint16_t scale,
      std::function<void(CenturyVSPump *pump, uint16_t value)> on_value_func);
  static CenturyPumpCommand create_run_command(
      CenturyVSPump *pump,
      std::function<void(CenturyVSPump *pump)> on_confirmation_func);
  static CenturyPumpCommand create_stop_command(
      CenturyVSPump *pump,
      std::function<void(CenturyVSPump *pump)> on_confirmation_func);
  static CenturyPumpCommand create_set_demand_command(
      CenturyVSPump *pump, uint16_t demand,
      std::function<void(CenturyVSPump *pump)> on_confirmation_func);
  static CenturyPumpCommand create_config_read_command(
      CenturyVSPump *pump, uint8_t page, uint8_t address,
      std::function<void(CenturyVSPump *pump, uint8_t value)> on_value_func);
  static CenturyPumpCommand create_config_write_command(
      CenturyVSPump *pump, uint8_t page, uint8_t address, uint8_t value,
      std::function<void(CenturyVSPump *pump)> on_confirmation_func);
  static CenturyPumpCommand create_config_read_uint16_command(
      CenturyVSPump *pump, uint8_t page, uint8_t address,
      std::function<void(CenturyVSPump *pump, uint16_t value)> on_value_func);
  static CenturyPumpCommand create_config_write_uint16_command(
      CenturyVSPump *pump, uint8_t page, uint8_t address, uint16_t value,
      std::function<void(CenturyVSPump *pump)> on_confirmation_func);
  static CenturyPumpCommand create_store_config_command(
      CenturyVSPump *pump,
      std::function<void(CenturyVSPump *pump)> on_confirmation_func);
};

} // namespace esphome::century_vs_pump
