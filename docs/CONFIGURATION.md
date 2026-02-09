# Configuration Reference

Complete reference for Century VS pump Modbus registers and ESPHome configuration.

## Protocol Overview

The Century VGreen pump uses a custom Modbus protocol with function codes in the 0x41-0x65 range.

| Function | Description |
|----------|-------------|
| 0x41 | Run pump |
| 0x42 | Stop pump |
| 0x43 | Read status |
| 0x44 | Read sensor (page, address, returns 16-bit scaled value) |
| 0x45 | Set demand (RPM * 4) |
| 0x64 | Read config byte (page, address) |
| 0x65 | Write config byte (page, address, value) + store command |

Full protocol documentation: [Gen3 EPC Modbus Communication Protocol v4.17](Gen3%20EPC%20Modbus%20Communication%20Protocol%20_Rev4.17.pdf)

## ESPHome Number Types

### Demand Control (Default)

Sets pump target speed. No `type` parameter needed.

```yaml
number:
  - platform: centuryvspump
    name: Pump Demand
    id: pump_speed
    unit_of_measurement: RPM
```

### Config Registers

Configuration registers use explicit page/address values. Two types available:

| Type | Description | Size |
|------|-------------|------|
| `config` | Single byte (uint8) | 1 byte |
| `config16` | Two bytes (uint16) | 2 bytes |

**Parameters:**

| Parameter | Required | Default | Description |
|-----------|----------|---------|-------------|
| `page` | Yes | - | Register page number |
| `address` | Yes | - | Register address (hex) |
| `offset` | No | 0 | Value transformation offset |
| `min_value` | No | 0 | Minimum allowed value |
| `max_value` | No | 255/65535 | Maximum allowed value |
| `step` | No | 1 | Value increment step |
| `store_to_flash` | No | true | Persist to pump DataFlash |

**Offset behavior:** Added when reading, subtracted when writing. Example: pump stores 0-18 internally, offset=32 displays 32-50.

## Configuration Parameters

### Page 1 - Serial Settings

| Parameter | Address | Type | Range | Description |
|-----------|---------|------|-------|-------------|
| Serial Timeout | 0x00 | config | 0-250s | Failsafe timer (0=disabled) |

```yaml
- platform: centuryvspump
  name: Serial Timeout
  type: config
  page: 1
  address: 0x00
  min_value: 0
  max_value: 250
  unit_of_measurement: s
```

### Page 10 - Operating Parameters

| Parameter | Address | Type | Range | Notes |
|-----------|---------|------|-------|-------|
| Priming Duration | 0x02 | config | 0-30 min | |
| Priming Speed | 0x03 | config16 | 600-3450 RPM | |
| Freeze Enable | 0x06 | config | 0-1 | On/Off |
| Freeze Temp | 0x07 | config | 32-50 F | Raw +32 offset |
| Freeze Speed | 0x09 | config16 | 600-3450 RPM | |
| Pause Duration | 0x0B | config | 0-255 min | |

```yaml
# Freeze protection with offset
- platform: centuryvspump
  name: Freeze Temp Threshold
  type: config
  page: 10
  address: 0x07
  offset: 32  # Pump stores 0-18, display as 32-50
  min_value: 32
  max_value: 50

# Two-byte config
- platform: centuryvspump
  name: Freeze Protection Speed
  type: config16
  page: 10
  address: 0x09
  min_value: 600
  max_value: 3450
  step: 50
```

## Sensor Registers

### Page 0 - Runtime Data

| Sensor | Address | Scale | Notes |
|--------|---------|-------|-------|
| Demand Readback | 0x03 | 4 | Current demand (RPM = raw / 4) |
| Ambient Temperature | 0x07 | 128 | Air temp (raw / 128 + 32 = F) |
| Motor Status | 0x08 | 1 | Status code |
| Previous Fault | 0x09 | 1 | Last fault code |
| Prime Status | 0x10 | 1 | Priming state |
| IGBT Temperature | 0x12 | 128 | Power electronics temp |

```yaml
sensor:
  # Temperature with scale and offset filter
  - platform: centuryvspump
    name: Ambient Temperature
    address: 7
    page: 0
    scale: 128
    type: custom
    unit_of_measurement: "F"
    filters:
      - offset: 32
```

### Status Codes

**Motor Status (0x08):**

| Code | Status |
|------|--------|
| 0x00 | Stopped |
| 0x09 | Starting/Ramping |
| 0x0B | Running |
| 0x20 | Fault |

**Prime Status (0x10):**

| Code | Status |
|------|--------|
| 0 | Idle |
| 1 | Priming |
| 2 | Complete |

**Fault Codes (0x09):**

| Code | Fault |
|------|-------|
| 0x00 | None |
| 0x21 | Overcurrent |
| 0x22 | DC Overvoltage |
| 0x2E | IGBT Overtemp |
| 0x3E | Comm Loss |
| 0x3F | Generic Fault |
| 0x40 | Coherence Fault |
| 0x41 | UL Fault |

## Hardware Pinouts

### M5Stack ATOM Lite

| Function | Pin |
|----------|-----|
| TX (RS485) | GPIO26 (Atomic RS485 Base) or GPIO19 |
| RX (RS485) | GPIO32 (Atomic RS485 Base) or GPIO22 |
| LED | GPIO27 |
| Button | GPIO39 |

### Pump RS485 Connector

The pump provides 12V power on the RS485 connector when DIP switch #1 is ON (Modbus mode).

**DIP Switch #1:**
- ON = Modbus protocol (required for this component)
- OFF = OEM protocol (not supported)
