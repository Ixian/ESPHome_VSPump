#pragma once

#include "esphome/components/centuryvspump/CenturyVSPump.h"
#include "esphome/components/number/number.h"
#include "esphome/core/component.h"

namespace esphome
{
    using namespace number;

    namespace century_vs_pump
    {
        class CenturyVSPumpConfigNumber : public CenturyPumpItemBase, public Component, public Number
        {
        public:
            CenturyVSPumpConfigNumber(uint8_t page, uint8_t address)
                : CenturyPumpItemBase(), page_(page), address_(address) {}

            void set_store_to_flash(bool store) { store_to_flash_ = store; }

            CenturyPumpCommand create_command() override;
            void control(float value) override;

        private:
            uint8_t page_;
            uint8_t address_;
            bool store_to_flash_{true};
        };
    }
}
