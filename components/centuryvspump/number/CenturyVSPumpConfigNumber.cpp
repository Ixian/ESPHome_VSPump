#include "CenturyVSPumpConfigNumber.h"

namespace esphome
{
    namespace century_vs_pump
    {
        static const char *const TAG = "century_vs_pump.config";

        CenturyPumpCommand CenturyVSPumpConfigNumber::create_command()
        {
            return CenturyPumpCommand::create_config_read_command(pump_, page_, address_, [this](CenturyVSPump *pump, uint8_t value)
                                                                  { this->publish_state((float)value); });
        }

        void CenturyVSPumpConfigNumber::control(float value)
        {
            uint8_t byte_value = (uint8_t)value;
            ESP_LOGD(TAG, "Set config page %d, addr %d to %d", page_, address_, byte_value);

            pump_->queue_command_(CenturyPumpCommand::create_config_write_command(pump_, page_, address_, byte_value, [this, value](CenturyVSPump *pump)
                                                                                   {
                this->publish_state(value);
                if (store_to_flash_)
                {
                    ESP_LOGD(TAG, "Storing config to DataFlash");
                    pump_->queue_command_(CenturyPumpCommand::create_store_config_command(pump_, [](CenturyVSPump *pump)
                                                                                          { ESP_LOGD(TAG, "Config stored successfully"); }));
                } }));
            this->publish_state(value);
            pump_->update();
        }
    }
}
