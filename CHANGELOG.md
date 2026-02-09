# ESPHome_VSPump Changelog

Century VS Pool Pump Controller - ESPHome component for controlling Century/Regal Beloit VGreen variable speed pool pumps via RS485 Modbus.

## Project Info

- **Fork**: https://github.com/Ixian/ESPHome_VSPump
- **Upstream**: https://github.com/gazoodle/CenturyVSPump
- **Branch**: `additional_features` (current), `main` (stable)

## Hardware

- **Controller**: M5Stack ATOM Lite (ESP32-PICO-D4)
- **Interface**: M5Stack RS485 Base
- **Pump**: Century VGreen 165 (Regal Beloit EPC Gen3)
- **Modbus Address**: 21
- **Pins**: TX=GPIO26, RX=GPIO32, LED=GPIO27, Button=GPIO39

## 2026-02-09 - Documentation Consolidation

### New Documentation

Added comprehensive documentation to make the repo self-contained:

- `docs/CONFIGURATION.md` - Register maps, protocol details, ESPHome config options
- `docs/HOME-ASSISTANT.md` - Entities, dashboards, example automations
- `docs/SAFETY.md` - Freeze protection, failsafes, serial timeout behavior

### Example YAML Updates

Updated `example_century_vs_pump.yaml` with:
- Temperature sensors (Ambient, IGBT)
- Status text_sensors (Motor Status, Prime Status, Previous Fault)
- Freeze protection template switch pattern
- Generic config types (removed deprecated presets)

### README Updates

- Added documentation links section
- Simplified YAML examples, link to full example file
- Updated external_components to use `main` branch

## 2026-01-18 - Additional Configuration Features

### Generic Config Types

Configuration registers are accessed using generic types with explicit page/address values.
No hardcoded presets - all pump-specific values are defined in YAML.

| Type | Description | Required Params |
|------|-------------|-----------------|
| `config` | Single byte (uint8) | `page`, `address` |
| `config16` | Two bytes (uint16) | `page`, `address` |

Optional parameters for both types:
- `store_to_flash` (default: true) - Persist changes to pump DataFlash
- `offset` (config only, default: 0) - Value transformation offset

### Example Usage

```yaml
number:
  - platform: centuryvspump
    name: Freeze Temp
    type: config
    page: 10
    address: 0x07
    offset: 32      # Pump stores 0-18, display as 32-50°F
    min_value: 32
    max_value: 50

  - platform: centuryvspump
    name: Freeze Speed
    type: config16
    page: 10
    address: 0x0C
    min_value: 600
    max_value: 3450
    step: 50
```

### Implementation Details

1. **CenturyVSPumpConfigNumber** - Single-byte config values with optional offset
2. **CenturyVSPumpConfigNumber16** - Two-byte (uint16) config values
3. **New Modbus commands**: config read/write (0x64), store to flash (0x65)

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
- Installed on M5Stack ATOM Lite at 192.168.68.70
- Connected to Century VGreen pump via RS485
- Integrated with Home Assistant
- Autonomous scheduling running 24/7
- Serial timeout failsafe configured (120s)

## Future Work

1. **Upstream PRs** - Submit bug fixes to gazoodle/CenturyVSPump (5 PRs planned)
2. **Booster pump interlock** - Hardware relay for pressure-side cleaner safety

## Protocol Reference

Regal Beloit EPC Gen3 Modbus Protocol (Century VGreen):
- Function 0x41: Run pump
- Function 0x42: Stop pump
- Function 0x43: Read status
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
