#pragma once

#include "CenturyPumpCommand.h"
#include "CenturyPumpCommandQueue.h"
#include "esphome/components/modbus/modbus.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

#include <memory>
#include <span>
#include <string>
#include <vector>

// #define MODBUS_ENABLE_SWITCH

namespace esphome {
namespace century_vs_pump {

class CenturyVSPump;
class CenturyVSPumpSensor;

class CenturyPumpItemBase {
public:
  CenturyPumpItemBase() : pump_(nullptr) {}
  explicit CenturyPumpItemBase(CenturyVSPump *pump) : pump_(pump) {}
  virtual CenturyPumpCommand create_command() = 0;

  void set_pump(CenturyVSPump *pump) { pump_ = pump; }

protected:
  CenturyVSPump *pump_;
};

#ifdef MODBUS_ENABLE_SWITCH
class CenturyPumpEnabledSwitch : public esphome::switch_::Switch {
public:
  void write_state(bool state) override {
    enabled_ = state;
    this->publish_state(enabled_);
  }

private:
  bool enabled_{false};
};
#endif

class CenturyVSPump : public PollingComponent,
                      public modbus::ModbusClientDevice {
public:
  CenturyVSPump() = default;

  void loop() override;
  void setup() override;
  void update() override;
  void dump_config() override;

  void on_custom_response(std::span<const uint8_t> request_pdu,
                          std::span<const uint8_t> response_pdu,
                          modbus::ResponseStatus status) override;
  void on_not_sent(std::span<const uint8_t> request_pdu) override;
  void on_sent(std::span<const uint8_t> request_pdu) override;
  bool on_no_response(std::span<const uint8_t> request_pdu) override;

  void add_item(CenturyPumpItemBase *item) { items_.push_back(item); }

  // ESPHome classifies every vendor function (0x41-0x65) as a custom read, so
  // it cannot infer pump control priority. Poll commands are coalesced locally;
  // controls retain FIFO order and are always selected before polls.
  bool queue_command_(const CenturyPumpCommand &command);
  bool queue_control_command_(const CenturyPumpCommand &command,
                              bool front = false);
  bool queue_delayed_control_command_(const CenturyPumpCommand &command,
                                      uint32_t delay_ms, bool front = true);
  bool queue_store_readback_command_(const CenturyPumpCommand &command);
  bool queue_stop_command_(const CenturyPumpCommand &command);

protected:
  static constexpr uint32_t STORE_QUIET_PERIOD_MS = 1000;
  static constexpr uint32_t BUSY_RETRY_DELAY_MS = 250;
  static constexpr uint32_t QUEUE_REFUSAL_DELAY_MS = 100;
  static constexpr uint8_t MAX_QUEUE_REFUSALS = 5;

  bool send_next_command_();
  void finish_in_flight_();
  void retry_busy_command_();

  CenturyPumpCommandQueue commands_;
  std::unique_ptr<CenturyPumpCommand> in_flight_;
  bool stop_requested_{false};

public:
  std::string name_;
  std::vector<CenturyPumpItemBase *> items_;
#ifdef MODBUS_ENABLE_SWITCH
  CenturyPumpEnabledSwitch *enabled_switch_{nullptr};
#endif
};

} // namespace century_vs_pump
} // namespace esphome
