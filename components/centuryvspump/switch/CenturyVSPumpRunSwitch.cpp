#include "CenturyVSPumpRunSwitch.h"

namespace esphome
{
    namespace century_vs_pump
    {
        static const char *const TAG = "century_vs_pump.switch";

        /////////////////////////////////////////////////////////////////////////////////////////////
        CenturyPumpCommand CenturyVSPumpRunSwitch::create_command()
        {
            return CenturyPumpCommand::create_status_command(pump_, [this](CenturyVSPump *pump, bool running)
                                                             { this->publish_state(running); });
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPumpRunSwitch::write_state(bool state)
        {
            // State published only on pump confirmation, not optimistically.
            // Immediate update() polls status to detect failures.
            if (state)
            {
                pump_->queue_command_(CenturyPumpCommand::create_run_command(pump_, [this](CenturyVSPump *pump)
                                                                             { this->publish_state(true); }));
            }
            else
            {
                pump_->queue_command_(CenturyPumpCommand::create_stop_command(pump_, [this](CenturyVSPump *pump)
                                                                              { this->publish_state(false); }));
            }

            pump_->update();
        }
    }
}
