#include "CenturyVSPumpConfigNumber16.h"

namespace esphome {
namespace century_vs_pump {
static const char *const TAG = "century_vs_pump.config16";

CenturyPumpCommand CenturyVSPumpConfigNumber16::create_command() {
  return CenturyPumpCommand::create_config_read_uint16_command(
      pump_, page_, address_, [this](CenturyVSPump *pump, uint16_t value) {
        this->publish_state((float)value);
      });
}

void CenturyVSPumpConfigNumber16::control(float value) {
  // Read-before-write is intentional flash-wear hardening for values
  // that Home Assistant may set again after every controller boot.
  const uint16_t target = (uint16_t)value;
  ESP_LOGD(TAG, "Verify config16 page %d, addr %d before setting to %d", page_,
           address_, target);

  auto readback = [this, target](CenturyVSPump *pump, bool after_store) {
    auto command = CenturyPumpCommand::create_config_read_uint16_command(
        pump, page_, address_,
        [this, target](CenturyVSPump *, uint16_t actual) {
          if (actual != target)
            ESP_LOGW(TAG,
                     "Config16 RAM readback mismatch: page %d, addr %d "
                     "expected %d, got %d",
                     page_, address_, target, actual);
          this->publish_state((float)actual);
        });
    if (after_store)
      pump->queue_store_readback_command_(command);
    else
      pump->queue_control_command_(command, true);
  };

  pump_->queue_control_command_(
      CenturyPumpCommand::create_config_read_uint16_command(
          pump_, page_, address_,
          [this, target, readback](CenturyVSPump *pump, uint16_t current) {
            if (current == target) {
              ESP_LOGD(TAG, "Config16 page %d, addr %d already equals %d",
                       page_, address_, target);
              this->publish_state((float)current);
              return;
            }

            pump->queue_control_command_(
                CenturyPumpCommand::create_config_write_uint16_command(
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
                                ESP_LOGD(TAG, "Config16 store acknowledged; "
                                              "waiting before RAM readback");
                                readback(pump, true);
                              }),
                          true);
                    }),
                true);
          }));
}
} // namespace century_vs_pump
} // namespace esphome
