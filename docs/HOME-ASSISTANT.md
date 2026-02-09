# Home Assistant Integration

Guide for integrating the Century VS pump controller with Home Assistant.

## Entities

After adding the ESPHome device to Home Assistant, these entities are created:

### Controls

| Entity | Type | Description |
|--------|------|-------------|
| Pump Run | Switch | Turn pump on/off |
| Pump Demand | Number | Set target RPM (600-3450) |
| Freeze Protection | Switch | Enable/disable freeze protection |
| Freeze Temp Threshold | Number | Temperature trigger (32-50 F) |
| Freeze Protection Speed | Number | RPM when frozen |
| Priming Duration | Number | Startup prime time (minutes) |
| Priming Speed | Number | Prime RPM |
| Pause Duration | Number | Temporary stop duration |
| Serial Timeout | Number | Failsafe timer (seconds) |

### Sensors

| Entity | Type | Description |
|--------|------|-------------|
| Pump RPM | Sensor | Current actual RPM |
| Demand Readback | Sensor | Current demand from pump |
| Ambient Temperature | Sensor | Air temp at pump (F) |
| IGBT Temperature | Sensor | Motor power electronics temp (F) |
| Motor Status | Text | Stopped/Starting/Running/Fault |
| Prime Status | Text | Idle/Priming/Complete |
| Previous Fault | Text | Last fault code |
| WiFi Signal | Sensor | Controller WiFi strength |

### Buttons

| Entity | Description |
|--------|-------------|
| Set 600 RPM | Low speed preset |
| Set 2000 RPM | Medium speed preset |
| Set 2600 RPM | High speed preset |
| Set 3450 RPM | Maximum speed preset |
| Restart | Restart controller |

## Dashboard Example

Example Mushroom card layout for pool pump control:

```yaml
type: vertical-stack
cards:
  # Status row
  - type: custom:mushroom-chips-card
    chips:
      - type: entity
        entity: switch.pump_run
        icon_color: blue
      - type: entity
        entity: sensor.pump_rpm
      - type: entity
        entity: sensor.ambient_temperature
      - type: entity
        entity: text_sensor.motor_status

  # Speed presets
  - type: horizontal-stack
    cards:
      - type: custom:mushroom-template-card
        primary: "1350"
        secondary: Overnight
        icon: mdi:speedometer-slow
        tap_action:
          action: call-service
          service: number.set_value
          data:
            entity_id: number.pump_demand
            value: 1350

      - type: custom:mushroom-template-card
        primary: "2200"
        secondary: Medium
        icon: mdi:speedometer-medium
        tap_action:
          action: call-service
          service: number.set_value
          data:
            entity_id: number.pump_demand
            value: 2200

      - type: custom:mushroom-template-card
        primary: "3100"
        secondary: Daytime
        icon: mdi:speedometer
        tap_action:
          action: call-service
          service: number.set_value
          data:
            entity_id: number.pump_demand
            value: 3100

  # Configuration
  - type: custom:mushroom-title-card
    title: Configuration

  - type: horizontal-stack
    cards:
      - type: custom:mushroom-entity-card
        entity: switch.freeze_protection
        name: Freeze Protection

      - type: custom:mushroom-number-card
        entity: number.freeze_temp_threshold
        name: Freeze Temp
```

## Automations

### Freeze Protection Alert

Alert when temperature drops and pump RPM is too low:

```yaml
automation:
  - id: freeze_risk_alert
    alias: "Pool Freeze Risk Alert"
    trigger:
      - platform: numeric_state
        entity_id: sensor.ambient_temperature
        below: 36
        for:
          minutes: 2
    condition:
      - condition: numeric_state
        entity_id: sensor.pump_rpm
        below: 1300
    action:
      - service: switch.turn_on
        target:
          entity_id: switch.pump_run
      - service: number.set_value
        target:
          entity_id: number.pump_demand
        data:
          value: 1350
      - service: notify.mobile_app
        data:
          title: "Pool Freeze Risk"
          message: "Pump speed increased to prevent freezing"
```

### Controller Offline Alert

Monitor for controller connectivity issues:

```yaml
automation:
  - id: pool_controller_offline
    alias: "Pool Controller Offline Alert"
    trigger:
      - platform: state
        entity_id: binary_sensor.pool_controller_status
        to: "off"
        for:
          minutes: 10
    action:
      - service: notify.mobile_app
        data:
          title: "Pool Controller Offline"
          message: "Pool pump controller has been offline for 10 minutes"
          data:
            priority: high
```

### Schedule Example

Basic time-based scheduling (can also be done in ESPHome):

```yaml
automation:
  - id: pool_daytime_schedule
    alias: "Pool Daytime Speed"
    trigger:
      - platform: time
        at: "09:00:00"
    action:
      - service: number.set_value
        target:
          entity_id: number.pump_demand
        data:
          value: 3100

  - id: pool_evening_schedule
    alias: "Pool Evening Speed"
    trigger:
      - platform: time
        at: "16:00:00"
    action:
      - service: number.set_value
        target:
          entity_id: number.pump_demand
        data:
          value: 1900

  - id: pool_overnight_schedule
    alias: "Pool Overnight Speed"
    trigger:
      - platform: time
        at: "22:00:00"
    action:
      - service: number.set_value
        target:
          entity_id: number.pump_demand
        data:
          value: 1350
```

## Troubleshooting

### Pump Not Responding

1. Check DIP switch #1 is ON (Modbus mode)
2. Verify RS485 wiring (TX/RX may need swapping)
3. Check Modbus address matches pump configuration (default: 21)
4. Enable UART debug in ESPHome config

### Config Writes Not Persisting

The `store_to_flash` parameter should be `true` (default). If changes don't persist across pump power cycles, the pump may not support DataFlash writes for that register.

### Temperature Reading Errors

Raw temperature values use scale=128. Apply the offset filter in ESPHome:

```yaml
filters:
  - offset: 32  # Convert to Fahrenheit
```
