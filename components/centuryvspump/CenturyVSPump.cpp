#include "CenturyVSPump.h"
#include "CenturyVSPumpProtocol.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace century_vs_pump {

static const char *const TAG = "century_vs_pump";

static bool validate_response_exact(const std::vector<uint8_t> &data,
                                    size_t expected_size,
                                    const char *cmd_name) {
  if (data.size() != expected_size) {
    ESP_LOGW(TAG, "%s response has %zu data bytes; expected %zu", cmd_name,
             data.size(), expected_size);
    return false;
  }
  return true;
}

void CenturyVSPump::setup() {
#ifdef MODBUS_ENABLE_SWITCH
  enabled_switch_ = new CenturyPumpEnabledSwitch();
  enabled_switch_->set_name(name_ + " MODBUS enabled");
  App.register_switch(enabled_switch_);
#endif
}

void CenturyVSPump::loop() { this->send_next_command_(); }

void CenturyVSPump::update() {
#ifdef MODBUS_ENABLE_SWITCH
  if (enabled_switch_ == nullptr || !enabled_switch_->state)
    return;
#endif
  ESP_LOGV(TAG, "Updating pump component");
  for (auto *item : items_) {
    auto command = item->create_command();
    command.poll_consumer_ = item;
    this->queue_command_(command);
  }
}

void CenturyVSPump::dump_config() {
  ESP_LOGCONFIG(TAG, "CenturyVSPump:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
  ESP_LOGCONFIG(TAG, "  Maximum transmissions per request: %u",
                CenturyPumpCommand::MAX_TRANSMISSIONS);
  ESP_LOGCONFIG(TAG, "  Maximum temporary-busy retries: %u",
                CenturyPumpCommand::MAX_BUSY_RETRIES);
}

bool CenturyVSPump::queue_command_(const CenturyPumpCommand &command) {
#ifdef MODBUS_ENABLE_SWITCH
  if (enabled_switch_ == nullptr || !enabled_switch_->state)
    return false;
#endif
  return this->commands_.enqueue_poll(command, this->in_flight_.get());
}

bool CenturyVSPump::queue_control_command_(const CenturyPumpCommand &command,
                                           bool front) {
  const auto result = this->commands_.enqueue_control(command, front);
  if (result == ControlQueueResult::REFUSED) {
    ESP_LOGE(TAG, "Control queue full; refusing function 0x%02X",
             command.function_);
    return false;
  }
  return true;
}

bool CenturyVSPump::queue_delayed_control_command_(
    const CenturyPumpCommand &command, uint32_t delay_ms, bool front) {
  CenturyPumpCommand delayed = command;
  delayed.not_before_ = millis() + delay_ms;
  return this->queue_control_command_(delayed, front);
}

bool CenturyVSPump::queue_store_readback_command_(
    const CenturyPumpCommand &command) {
  return this->queue_delayed_control_command_(command, STORE_QUIET_PERIOD_MS,
                                              true);
}

bool CenturyVSPump::queue_stop_command_(const CenturyPumpCommand &command) {
  if (this->in_flight_ != nullptr && this->in_flight_->function_ == 0x42)
    return false;
  this->stop_requested_ = true;
  return this->commands_.enqueue_stop(command) == ControlQueueResult::QUEUED;
}

bool CenturyVSPump::send_next_command_() {
  if (this->in_flight_ != nullptr)
    return false;

  CenturyPumpCommand *candidate = this->commands_.next();
  if (candidate == nullptr)
    return false;

  if (!protocol::deadline_reached(millis(), candidate->not_before_))
    return false;

  const std::vector<uint8_t> request_pdu = candidate->pdu();
  if (!this->queue_pdu(request_pdu)) {
    candidate->queue_refusal_count_++;
    candidate->not_before_ = millis() + QUEUE_REFUSAL_DELAY_MS;
    if (candidate->queue_refusal_count_ == 1)
      ESP_LOGW(TAG,
               "ESPHome Modbus queue refused pump function 0x%02X; retrying",
               candidate->function_);
    if (candidate->queue_refusal_count_ >= MAX_QUEUE_REFUSALS &&
        candidate->function_ != 0x42) {
      ESP_LOGE(TAG,
               "Dropping pump function 0x%02X after %u Modbus queue refusals",
               candidate->function_, candidate->queue_refusal_count_);
      this->commands_.take_next();
    }
    return false;
  }

  this->in_flight_ = this->commands_.take_next();
  return true;
}

void CenturyVSPump::finish_in_flight_() {
  if (this->in_flight_ != nullptr)
    this->stop_requested_ = protocol::stop_pending_after_terminal(
        this->in_flight_->function_, this->stop_requested_);
  this->in_flight_.reset();
}

void CenturyVSPump::retry_busy_command_() {
  auto command = std::move(this->in_flight_);
  command->busy_retry_count_++;
  command->not_before_ = millis() + BUSY_RETRY_DELAY_MS;
  this->commands_.restore_front(std::move(command));
}

void CenturyVSPump::on_custom_response(std::span<const uint8_t> request_pdu,
                                       std::span<const uint8_t> response_pdu,
                                       modbus::ResponseStatus status) {
  if (this->in_flight_ == nullptr ||
      !this->in_flight_->matches_pdu(request_pdu)) {
    ESP_LOGE(
        TAG,
        "Received a response that does not match the in-flight pump request");
    this->finish_in_flight_();
    return;
  }

  if (status.has_value()) {
    const uint8_t exception = static_cast<uint8_t>(status.value());
    if (protocol::superseded_by_stop(this->in_flight_->function_,
                                     this->stop_requested_)) {
      ESP_LOGW(TAG,
               "Dropping superseded function 0x%02X because STOP is pending",
               this->in_flight_->function_);
      this->finish_in_flight_();
      return;
    }
    if (exception == protocol::TEMPORARILY_BUSY &&
        this->in_flight_->busy_retry_count_ <
            CenturyPumpCommand::MAX_BUSY_RETRIES &&
        this->in_flight_->transmission_count_ <
            CenturyPumpCommand::MAX_TRANSMISSIONS) {
      ESP_LOGW(
          TAG, "Pump temporarily busy for function 0x%02X; retrying (%u/%u)",
          this->in_flight_->function_, this->in_flight_->busy_retry_count_ + 1,
          CenturyPumpCommand::MAX_BUSY_RETRIES);
      this->retry_busy_command_();
      return;
    }
    ESP_LOGW(TAG, "Pump exception for function 0x%02X: 0x%02X",
             this->in_flight_->function_, exception);
    this->finish_in_flight_();
    return;
  }

  const auto validation =
      protocol::validate_response(this->in_flight_->function_, response_pdu);
  if (validation != protocol::ResponseValidation::OK) {
    if (validation == protocol::ResponseValidation::TOO_SHORT)
      ESP_LOGW(TAG,
               "Pump response for function 0x%02X is too short (%zu bytes)",
               this->in_flight_->function_, response_pdu.size());
    else if (validation == protocol::ResponseValidation::FUNCTION_MISMATCH)
      ESP_LOGW(TAG,
               "Pump response function mismatch: got 0x%02X, expected 0x%02X",
               response_pdu[0], this->in_flight_->function_);
    else
      ESP_LOGW(TAG, "Pump NACK for function 0x%02X: 0x%02X",
               this->in_flight_->function_, response_pdu[1]);
    this->finish_in_flight_();
    return;
  }

  auto completed = std::move(this->in_flight_);
  this->stop_requested_ = protocol::stop_pending_after_terminal(
      completed->function_, this->stop_requested_);
  std::vector<uint8_t> data(response_pdu.begin() + 2, response_pdu.end());
  for (const auto &on_data_func : completed->on_data_funcs_)
    on_data_func(this, data);
}

void CenturyVSPump::on_not_sent(std::span<const uint8_t> request_pdu) {
  if (this->in_flight_ == nullptr ||
      !this->in_flight_->matches_pdu(request_pdu)) {
    ESP_LOGE(TAG, "Dropped request does not match the in-flight pump request");
    this->finish_in_flight_();
    return;
  }
  ESP_LOGW(TAG, "Pump function 0x%02X was dropped before transmission",
           this->in_flight_->function_);
  this->finish_in_flight_();
}

void CenturyVSPump::on_sent(std::span<const uint8_t> request_pdu) {
  if (this->in_flight_ != nullptr && this->in_flight_->matches_pdu(request_pdu))
    this->in_flight_->transmission_count_++;
}

bool CenturyVSPump::on_no_response(std::span<const uint8_t> request_pdu) {
  if (this->in_flight_ == nullptr ||
      !this->in_flight_->matches_pdu(request_pdu)) {
    ESP_LOGE(TAG, "Timeout does not match the in-flight pump request");
    this->finish_in_flight_();
    return false;
  }
  if (protocol::superseded_by_stop(this->in_flight_->function_,
                                   this->stop_requested_)) {
    ESP_LOGW(TAG,
             "Not retrying superseded function 0x%02X because STOP is pending",
             this->in_flight_->function_);
    this->finish_in_flight_();
    return false;
  }
  if (this->in_flight_->transmission_count_ <
      CenturyPumpCommand::MAX_TRANSMISSIONS) {
    ESP_LOGW(
        TAG,
        "No response for pump function 0x%02X; retrying transmission %u/%u",
        this->in_flight_->function_, this->in_flight_->transmission_count_ + 1,
        CenturyPumpCommand::MAX_TRANSMISSIONS);
    return true;
  }

  ESP_LOGE(TAG, "No response for pump function 0x%02X after %u transmissions",
           this->in_flight_->function_, this->in_flight_->transmission_count_);
  this->finish_in_flight_();
  return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////
CenturyPumpCommand CenturyPumpCommand::create_status_command(
    CenturyVSPump *pump,
    std::function<void(CenturyVSPump *pump, bool running)> on_status_func) {
  CenturyPumpCommand cmd = {};
  cmd.pump_ = pump;
  cmd.function_ = 0x43; // Pump status
  cmd.on_data_funcs_.push_back([=](CenturyVSPump *pump,
                                   const std::vector<uint8_t> data) {
    if (!validate_response_exact(data, 1, "Status"))
      return;

    ESP_LOGD(TAG, "Got status command reply %02X", data[0]);

    switch (data[0]) {
    case 0x00: // Stopped
      on_status_func(pump, false);
      break;
    case 0x09: // Boot/Initializing
      ESP_LOGD(TAG, "Pump is booting/initializing");
      on_status_func(pump, false);
      break;
    case 0x0B: // Running
      on_status_func(pump, true);
      break;
    case 0x20: // Fault
      ESP_LOGW(TAG, "Pump reports FAULT condition");
      on_status_func(pump, false);
      break;
    default:
      ESP_LOGW(TAG, "Unknown pump status: 0x%02X", data[0]);
      break;
    }
  });
  return cmd;
}

/////////////////////////////////////////////////////////////////////////////////////////////
CenturyPumpCommand CenturyPumpCommand::create_read_sensor_command(
    CenturyVSPump *pump, uint8_t page, uint8_t address, uint16_t scale,
    std::function<void(CenturyVSPump *pump, uint16_t value)> on_value_func) {
  CenturyPumpCommand cmd = {};
  cmd.pump_ = pump;
  cmd.function_ = 0x45; // Read sensor
  cmd.payload_.push_back(page);
  cmd.payload_.push_back(address);
  cmd.on_data_funcs_.push_back([=](CenturyVSPump *pump,
                                   const std::vector<uint8_t> data) {
    // Response format: page, address, value_lo, value_hi
    if (!validate_response_exact(data, 4, "Sensor read"))
      return;
    if (data[0] != page || data[1] != address) {
      ESP_LOGW(TAG,
               "Sensor response mismatch: expected page %d addr %d, got page "
               "%d addr %d",
               page, address, data[0], data[1]);
      return;
    }

    uint16_t value = (uint16_t)data[2] | ((uint16_t)data[3] << 8);
    // Scale the value
    value /= scale;
    ESP_LOGD(TAG, "Read value %d from page %d, addr %d", value, page, address);
    on_value_func(pump, value);
  });
  return cmd;
}

/////////////////////////////////////////////////////////////////////////////////////////////
CenturyPumpCommand CenturyPumpCommand::create_run_command(
    CenturyVSPump *pump,
    std::function<void(CenturyVSPump *pump)> on_confirmation_func) {
  CenturyPumpCommand cmd = {};
  cmd.pump_ = pump;
  cmd.function_ = 0x41; // Go
  cmd.on_data_funcs_.push_back([=](CenturyVSPump *pump,
                                   const std::vector<uint8_t> data) {
    if (!validate_response_exact(data, 0, "Run"))
      return;
    ESP_LOGD(TAG, "Confirmed pump running");
    on_confirmation_func(pump);
  });
  return cmd;
}

/////////////////////////////////////////////////////////////////////////////////////////////
CenturyPumpCommand CenturyPumpCommand::create_stop_command(
    CenturyVSPump *pump,
    std::function<void(CenturyVSPump *pump)> on_confirmation_func) {
  CenturyPumpCommand cmd = {};
  cmd.pump_ = pump;
  cmd.function_ = 0x42; // Stop
  cmd.on_data_funcs_.push_back([=](CenturyVSPump *pump,
                                   const std::vector<uint8_t> data) {
    if (!validate_response_exact(data, 0, "Stop"))
      return;
    ESP_LOGD(TAG, "Confirmed pump stopped");
    on_confirmation_func(pump);
  });
  return cmd;
}

/////////////////////////////////////////////////////////////////////////////////////////////
CenturyPumpCommand CenturyPumpCommand::create_set_demand_command(
    CenturyVSPump *pump, uint16_t demand,
    std::function<void(CenturyVSPump *pump)> on_confirmation_func) {
  CenturyPumpCommand cmd = {};
  cmd.pump_ = pump;
  cmd.function_ = 0x44;      // Set demand
  cmd.payload_.push_back(0); // Mode (0=Speed, 1=Torque, 2=Reserved, 3=Reserved)
  demand *= 4;               // Scaling
  cmd.payload_.push_back(demand & 0xff);
  cmd.payload_.push_back(demand >> 8);
  cmd.on_data_funcs_.push_back([=](CenturyVSPump *pump,
                                   const std::vector<uint8_t> data) {
    if (!validate_response_exact(data, 3, "Set demand"))
      return;
    if (data[0] != 0 || data[1] != (demand & 0xff) ||
        data[2] != (demand >> 8)) {
      ESP_LOGW(TAG, "Set demand response did not echo the request");
      return;
    }
    ESP_LOGD(TAG, "Set demand comfirmed");
    on_confirmation_func(pump);
  });
  return cmd;
}

/////////////////////////////////////////////////////////////////////////////////////////////
CenturyPumpCommand CenturyPumpCommand::create_config_read_command(
    CenturyVSPump *pump, uint8_t page, uint8_t address,
    std::function<void(CenturyVSPump *pump, uint8_t value)> on_value_func) {
  CenturyPumpCommand cmd = {};
  cmd.pump_ = pump;
  cmd.function_ = 0x64;         // Config Read/Write
  cmd.payload_.push_back(page); // Page (MSBit=0 for read)
  cmd.payload_.push_back(address);
  cmd.payload_.push_back(0); // Length 0 = 1 byte
  cmd.on_data_funcs_.push_back([=](CenturyVSPump *pump,
                                   const std::vector<uint8_t> data) {
    // Response format: page, address, length, data
    if (!validate_response_exact(data, 4, "Config read"))
      return;

    // Validate response matches request (guards against line noise/corruption)
    if (data[0] != page || data[1] != address || data[2] != 0) {
      ESP_LOGW(TAG,
               "Config read response mismatch: expected page %d addr %d, got "
               "page %d addr %d",
               page, address, data[0], data[1]);
      return;
    }

    uint8_t value = data[3];
    ESP_LOGD(TAG, "Config read page %d, addr %d = %d", page, address, value);
    on_value_func(pump, value);
  });
  return cmd;
}

/////////////////////////////////////////////////////////////////////////////////////////////
CenturyPumpCommand CenturyPumpCommand::create_config_write_command(
    CenturyVSPump *pump, uint8_t page, uint8_t address, uint8_t value,
    std::function<void(CenturyVSPump *pump)> on_confirmation_func) {
  CenturyPumpCommand cmd = {};
  cmd.pump_ = pump;
  cmd.function_ = 0x64;                // Config Read/Write
  cmd.payload_.push_back(page | 0x80); // Page with MSBit=1 for write
  cmd.payload_.push_back(address);
  cmd.payload_.push_back(0); // Length 0 = 1 byte
  cmd.payload_.push_back(value);
  cmd.on_data_funcs_.push_back([=](CenturyVSPump *pump,
                                   const std::vector<uint8_t> data) {
    if (!validate_response_exact(data, 4, "Config write"))
      return;
    if (data[0] != (page | 0x80) || data[1] != address || data[2] != 0 ||
        data[3] != value) {
      ESP_LOGW(TAG, "Config write response did not echo page/address/value");
      return;
    }
    ESP_LOGD(TAG, "Config write confirmed: page %d, addr %d = %d", page,
             address, value);
    on_confirmation_func(pump);
  });
  return cmd;
}

/////////////////////////////////////////////////////////////////////////////////////////////
CenturyPumpCommand CenturyPumpCommand::create_store_config_command(
    CenturyVSPump *pump,
    std::function<void(CenturyVSPump *pump)> on_confirmation_func) {
  CenturyPumpCommand cmd = {};
  cmd.pump_ = pump;
  cmd.function_ = 0x65; // Store config to DataFlash
  cmd.on_data_funcs_.push_back([=](CenturyVSPump *pump,
                                   const std::vector<uint8_t> data) {
    if (!validate_response_exact(data, 0, "Config store"))
      return;
    ESP_LOGD(TAG, "Pump acknowledged configuration store request");
    on_confirmation_func(pump);
  });
  return cmd;
}

/////////////////////////////////////////////////////////////////////////////////////////////
CenturyPumpCommand CenturyPumpCommand::create_config_read_uint16_command(
    CenturyVSPump *pump, uint8_t page, uint8_t address,
    std::function<void(CenturyVSPump *pump, uint16_t value)> on_value_func) {
  CenturyPumpCommand cmd = {};
  cmd.pump_ = pump;
  cmd.function_ = 0x64;         // Config Read/Write
  cmd.payload_.push_back(page); // Page (MSBit=0 for read)
  cmd.payload_.push_back(address);
  cmd.payload_.push_back(1); // Length 1 = 2 bytes
  cmd.on_data_funcs_.push_back([=](CenturyVSPump *pump,
                                   const std::vector<uint8_t> data) {
    // Response format: page, address, length, data_lo, data_hi
    if (!validate_response_exact(data, 5, "Config read uint16"))
      return;

    // Validate response matches request (guards against line noise/corruption)
    if (data[0] != page || data[1] != address || data[2] != 1) {
      ESP_LOGW(TAG,
               "Config read uint16 response mismatch: expected page %d addr "
               "%d, got page %d addr %d",
               page, address, data[0], data[1]);
      return;
    }

    uint16_t value = (uint16_t)data[3] | ((uint16_t)data[4] << 8);
    ESP_LOGD(TAG, "Config read uint16 page %d, addr %d = %d", page, address,
             value);
    on_value_func(pump, value);
  });
  return cmd;
}

/////////////////////////////////////////////////////////////////////////////////////////////
CenturyPumpCommand CenturyPumpCommand::create_config_write_uint16_command(
    CenturyVSPump *pump, uint8_t page, uint8_t address, uint16_t value,
    std::function<void(CenturyVSPump *pump)> on_confirmation_func) {
  CenturyPumpCommand cmd = {};
  cmd.pump_ = pump;
  cmd.function_ = 0x64;                // Config Read/Write
  cmd.payload_.push_back(page | 0x80); // Page with MSBit=1 for write
  cmd.payload_.push_back(address);
  cmd.payload_.push_back(1);                   // Length 1 = 2 bytes
  cmd.payload_.push_back(value & 0xff);        // Low byte
  cmd.payload_.push_back((value >> 8) & 0xff); // High byte
  cmd.on_data_funcs_.push_back([=](CenturyVSPump *pump,
                                   const std::vector<uint8_t> data) {
    if (!validate_response_exact(data, 5, "Config write uint16"))
      return;
    if (data[0] != (page | 0x80) || data[1] != address || data[2] != 1 ||
        data[3] != (value & 0xff) || data[4] != ((value >> 8) & 0xff)) {
      ESP_LOGW(TAG,
               "Config write uint16 response did not echo page/address/value");
      return;
    }
    ESP_LOGD(TAG, "Config write uint16 confirmed: page %d, addr %d = %d", page,
             address, value);
    on_confirmation_func(pump);
  });
  return cmd;
}
} // namespace century_vs_pump
} // namespace esphome
