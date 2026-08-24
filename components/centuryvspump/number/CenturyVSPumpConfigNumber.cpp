#include "CenturyVSPumpConfigNumber.h"

namespace esphome {
namespace century_vs_pump {
static const char *const TAG = "century_vs_pump.config";

CenturyPumpCommand CenturyVSPumpConfigNumber::create_command() {
  return CenturyPumpCommand::create_config_read_command(
      pump_, page_, address_, [this](CenturyVSPump *pump, uint8_t value) {
        this->publish_state((float)value + offset_);
      });
}

void CenturyVSPumpConfigNumber::control(float value) {
  // Read-before-write is intentional flash-wear hardening for values
  // that Home Assistant may set again after every controller boot.
  const uint8_t target = (uint8_t)(value - offset_);
  ESP_LOGD(TAG, "Verify config page %d, addr %d before setting to %d", page_,
           address_, target);

  auto readback = [this, target](CenturyVSPump *pump, bool after_store) {
    auto command = CenturyPumpCommand::create_config_read_command(
        pump, page_, address_, [this, target](CenturyVSPump *, uint8_t actual) {
          if (actual != target)
            ESP_LOGW(TAG,
                     "Config RAM readback mismatch: page %d, addr %d expected "
                     "%d, got %d",
                     page_, address_, target, actual);
          this->publish_state((float)actual + offset_);
        });
    if (after_store)
      pump->queue_store_readback_command_(command);
    else
      pump->queue_control_command_(command, true);
  };

  pump_->queue_control_command_(CenturyPumpCommand::create_config_read_command(
      pump_, page_, address_,
      [this, target, readback](CenturyVSPump *pump, uint8_t current) {
        if (current == target) {
          ESP_LOGD(TAG, "Config page %d, addr %d already equals %d", page_,
                   address_, target);
          this->publish_state((float)current + offset_);
          return;
        }

        pump->queue_control_command_(
            CenturyPumpCommand::create_config_write_command(
                pump, page_, address_, target,
                [this, readback](CenturyVSPump *pump) {
                  if (!store_to_flash_) {
                    readback(pump, false);
                    return;
                  }
                  pump->queue_control_command_(
                      CenturyPumpCommand::create_store_config_command(
                          pump,
                          [readback](CenturyVSPump *pump) {
                            ESP_LOGD(TAG, "Config store acknowledged; waiting "
                                          "before RAM readback");
                            readback(pump, true);
                          }),
                      true);
                }),
            true);
      }));
}
} // namespace century_vs_pump
} // namespace esphome
