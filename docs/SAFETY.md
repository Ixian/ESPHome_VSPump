# Safety Features

Critical safety information for pool pump automation.

## Serial Timeout Failsafe

The pump has a built-in serial timeout that reverts to the panel timer schedule if the controller stops communicating.

| Setting | Recommended Value | Description |
|---------|-------------------|-------------|
| Serial Timeout | 120 seconds | Time before pump reverts to panel schedule |

**How it works:**
1. Controller sends commands regularly (via `update_interval`)
2. If no commands received for timeout period, pump reverts to panel mode
3. Panel timer schedule takes over until serial communication resumes

**Configuration:**

```yaml
number:
  - platform: centuryvspump
    name: Serial Timeout
    type: config
    page: 1
    address: 0x00
    min_value: 0
    max_value: 250
    store_to_flash: true
```

Set to 0 to disable (not recommended).

## Freeze Protection

**Important:** The pump's built-in freeze protection is disabled when under RS-485/Modbus automation control. The pump expects the external controller to handle freeze logic.

### Protection Layers

| Layer | Description |
|-------|-------------|
| 1. Continuous operation | Run pump 24/7 at minimum RPM |
| 2. Serial timeout | Pump reverts to panel schedule if controller fails |
| 3. HA automation | Detect freeze risk and auto-correct |

### Recommended Minimum RPM

| Climate | Minimum RPM | Notes |
|---------|-------------|-------|
| Central Texas | 1350 | Sufficient for typical freezes |
| Hard freeze zones | 1800+ | Consult local pool professional |

### Pump Freeze Settings

These settings are in the pump firmware but are bypassed during RS-485 control:

| Parameter | Address | Description |
|-----------|---------|-------------|
| Freeze Enable | Page 10, 0x06 | 0=Off, 1=On |
| Freeze Temp | Page 10, 0x07 | Trigger temp (raw +32 = F) |
| Freeze Speed | Page 10, 0x09 | Speed when frozen (RPM) |

**Unverified assumption:** Whether the pump re-enables internal freeze protection when serial control disconnects has not been tested. The failsafe design assumes the panel schedule provides adequate protection.

### Home Assistant Freeze Automation

Self-healing automation that detects and fixes freeze risk:

```yaml
automation:
  - id: critical_alert_pool_freeze_risk
    alias: "Pool Freeze Risk - Self Healing"
    trigger:
      - platform: numeric_state
        entity_id: weather.local
        attribute: temperature
        below: 36
        for:
          minutes: 2
    condition:
      - condition: numeric_state
        entity_id: sensor.pump_rpm
        below: 1300
    action:
      # Auto-fix
      - service: switch.turn_on
        target:
          entity_id: switch.pump_run
      - service: number.set_value
        target:
          entity_id: number.pump_demand
        data:
          value: 1350
      # Verify fix
      - delay:
          seconds: 60
      - choose:
          - conditions:
              - condition: numeric_state
                entity_id: sensor.pump_rpm
                above: 1200
            sequence:
              - service: notify.discord
                data:
                  message: "Freeze protection activated - pump now at {{ states('sensor.pump_rpm') }} RPM"
        default:
          - service: notify.discord
            data:
              message: "ALERT: Freeze protection auto-fix failed! Manual intervention required."
```

## Controller Offline Monitoring

Alert if the ESP controller goes offline:

```yaml
automation:
  - id: critical_alert_pool_controller_offline
    alias: "Pool Controller Offline"
    trigger:
      - platform: state
        entity_id: binary_sensor.pool_controller_status
        to: unavailable
        for:
          minutes: 10
    action:
      - service: notify.discord
        data:
          message: "Pool pump controller offline for 10+ minutes. Serial timeout failsafe should engage."
```

## Power Loss Scenarios

| Scenario | Result |
|----------|--------|
| Controller loses power | Pump reverts to panel schedule after timeout |
| Controller loses WiFi | Pump continues at last command until timeout |
| Pump loses power | Pump restarts in last mode when power returns |
| Both lose power | Pump starts on panel schedule, controller reconnects |

## Future Considerations

### Booster Pump Interlock

The booster pump (for pressure-side cleaner) must not run unless the filter pump is at adequate speed.

| Requirement | Value |
|-------------|-------|
| Minimum filter pump RPM | 1900+ |
| Interlock type | External relay recommended |

**Warning:** Software-only interlocks in Home Assistant are not failsafe. A hardware interlock (relay/contactor) is recommended for booster pump protection.

### Additional Monitoring

Consider adding:
- Water level sensor (low water = pump damage)
- Flow sensor (verify water is actually moving)
- Pressure sensor (filter condition monitoring)
