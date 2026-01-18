# HSB-Pool Changelog

Century VS Pool Pump Controller - ESPHome component for controlling Century/Regal Beloit VGreen variable speed pool pumps via RS485 Modbus.

## Project Info

- **Fork**: https://github.com/Ixian/ESPHome_VSPump
- **Upstream**: https://github.com/gazoodle/CenturyVSPump
- **Branch**: `fix/bugs-and-failsafe`

## Hardware

- **Controller**: M5Stack ATOM Lite (ESP32-PICO-D4)
- **Interface**: M5Stack RS485 Base
- **Pump**: Century VGreen 165 (Regal Beloit EPC Gen3)
- **Modbus Address**: 21
- **Pins**: TX=GPIO26, RX=GPIO32, LED=GPIO27, Button=GPIO39

## 2026-01-18 - Initial Setup and Bug Fixes

### Bug Fixes

1. **CenturyVSPumpDemandNumber.cpp** - Fixed incorrect variable in `control()` callback
   - Changed `this->publish_state(state)` to `this->publish_state(value)`
   - The `state` variable doesn't exist in that scope; `value` is the function parameter

2. **CenturyVSPump.cpp** - Added missing pump status codes
   - Original code only handled status 0x00 (stopped) and 0x0B (running)
   - Added 0x09 (starting/ramping) and 0x20 (unknown, treating as running)
   - Refactored to switch statement for clarity

3. **C++20 Lambda Capture Warnings** - Fixed deprecated implicit `this` capture
   - Changed `[=]` to `[this]` or `[this, value]` in all lambda callbacks
   - Files: CenturyVSPumpConfigNumber.cpp, CenturyVSPumpDemandNumber.cpp, CenturyVSPumpSensor.cpp, CenturyVSPumpRunSwitch.cpp

### New Features

1. **Serial Timeout Failsafe Configuration** - Added ability to read/write pump config registers
   - New Modbus commands: config read (0x64), config write (0x65), store to flash
   - New component: `CenturyVSPumpConfigNumber` for config register access
   - Preset for `serial_timeout` (page 1, address 0) - controls how long pump runs without serial communication before stopping
   - `store_to_flash` option to persist config changes to pump's DataFlash

### Configuration Updates

1. **example_century_vs_pump.yaml**
   - Added explicit `framework: type: arduino` under esp32
   - Removed deprecated `rmt_channel` from LED config (deprecated in ESPHome 2025.2.0+)

## Current State

- Firmware compiled cleanly (no warnings)
- Installed on M5Stack ATOM Lite
- Device online and accessible
- **Not yet connected to actual pool pump**

## Next Steps

1. **Physical Connection** - Connect RS485 to pool pump's EPC connector
2. **Communication Test** - Verify Modbus communication with pump
3. **Serial Timeout Test** - Configure and test the failsafe timeout feature
4. **Home Assistant Integration** - Add to HA and create automations
5. **Upstream PR** - Consider submitting bug fixes to gazoodle/CenturyVSPump

## Protocol Reference

Regal Beloit EPC Gen3 Modbus Protocol (Century VGreen):
- Function 0x41: Read status
- Function 0x42: Run pump
- Function 0x43: Stop pump
- Function 0x44: Read sensor (page, address, returns 16-bit scaled value)
- Function 0x45: Set demand (RPM * 4)
- Function 0x64: Read config byte (page, address)
- Function 0x65: Write config byte (page, address, value) + store command

Status codes:
- 0x00: Stopped
- 0x09: Starting/ramping
- 0x0B: Running
- 0x20: Unknown (treated as running)

Config registers:
- Page 1, Address 0: Serial timeout (seconds, 0-250, 0=disabled)
