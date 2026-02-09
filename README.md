# CenturyVSPump

ESPHome custom component to drive Century (and other) variable speed pump motors via RS485 Modbus.

## Documentation

- [Configuration Reference](docs/CONFIGURATION.md) - Register maps, protocol details, ESPHome config options
- [Home Assistant Integration](docs/HOME-ASSISTANT.md) - Entities, dashboards, automations
- [Safety Features](docs/SAFETY.md) - Freeze protection, failsafes, serial timeout
- [Protocol Specification](docs/Gen3%20EPC%20Modbus%20Communication%20Protocol%20_Rev4.17.pdf) - Official Regal Modbus protocol

## Disclaimer

Use this information at your own risk, if your pump isn't the same as mine then it may work, or it may not. YMMV. I don't think you'll break your pump because when I did something wrong during my development, the pump reported errors back to me, but it's possible that you might issue a valid, yet destructive command to your pump and that might be the end of it. I doubt it, but I can't be 100% sure, so ... you've been warned, tread carefully.

## Details

There are a slew of pool pumps that have a super variable speed motor driving them, and look like they can be controlled by a modbus controller as there is a connector marked RS485.

I have a Pentair Sta-Rite S5P2R-VS pool pump and looking closely at the pump labels, I found that this is powered by a Century VS motor which in turn was manufactured by Regal as a whitelabeled version of their VGreen variable speed motor.

After much hunting around I found this very informative post https://www.troublefreepool.com/threads/century-regal-vgreen-motor-automation.238733/

While that post is really about how to make the VGreen motor talk the Jandy protocol so that the pump can be controlled from an existing system, my interest was piqued because during that process, the pump is placed into a Modbus protocol mode which was something that I was very interested in.

You can identify these pumps as they have two automation compatible interfaces, the RS485 one and a 5-wire connector that allows remote control of the three preset speeds or the override speed.

There is also a 5-way DIP switch that controls how the pump firmware behaves, and this is key to driving the pump.

![Picture of interfaces & DIP switches](images/Automation-Interfaces.PNG)

The critical switch is #1 which determines if the pump talks the Modbus commands as documented in [Regal Modbus v4.17 protocol](https://github.com/gazoodle/CenturyVSPump/blob/main/docs/Gen3%20EPC%20Modbus%20Communication%20Protocol%20_Rev4.17.pdf) (in the ON position), or if it talks the OEM protocol (in the OFF position).

The RS485 interface also has a modest 12V power line which I was also very pleased about as I could use it to power the fantastic [M5Stack ATOM ESP32 device](https://shop.m5stack.com/products/atom-rs485-kit?_pos=2&_sid=36efb1489&_ss=r) without the need for any other external power source. Indeed, a good way to determine if you're selected on the Modbus protocol is that the 12V line is only live when DIP switch #1 is on and so this little device wont be powered otherwise.

![Picture of ATOM RS485 module](images/Atom-RS485.png)

# Protocol

For some reason, the pump doesn't talk basic Modbus (coils and registers) which would have worked just fine. Instead they have a custom protocol which uses functions sensibly in the custom function range. Without the protocol documentation that was outed on www.troublefreepool.com this wouldn't have been possible, so thanks to whoever found that.

# Software

I won't go into how to flash these devices, there are tons of resources available, you can start looking at https://esphome.io/

Since this pump code is based off of data gleaned from an unsupported document, I won't be pushing this pump code to the ESPHome main branch, but you can get it from my GIT repo using this YAML.

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/Ixian/ESPHome_VSPump
      ref: main
```

Once flashed and added to Home Assistant, you will be able to turn the pump on or off and set the RPM speed.

## ESPHome Modbus Patch Required

This component requires a patched ESPHome modbus component that exposes the raw payload for custom function codes. The standard ESPHome modbus component only supports standard Modbus functions (read/write coils and registers), but the Century pump uses custom function codes (0x41-0x65).

See the ESPHome issue/PR for details on the required modification to `esphome/components/modbus/modbus.cpp`.

# YAML Configuration

See [example_century_vs_pump.yaml](example_century_vs_pump.yaml) for a complete configuration example including:
- Temperature sensors (ambient, IGBT)
- Motor status text sensors
- Freeze protection settings
- Configuration register access

**Basic setup:**

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/Ixian/ESPHome_VSPump
      ref: main

uart:
  baud_rate: 9600
  rx_pin: GPIO22
  tx_pin: GPIO19

modbus:

centuryvspump:
  address: 21  # Pump Modbus address

switch:
  - platform: centuryvspump
    name: Pump Run

number:
  - platform: centuryvspump
    name: Pump Demand
    unit_of_measurement: RPM

sensor:
  - platform: centuryvspump
    name: Pump RPM
    type: rpm
    unit_of_measurement: RPM
```

**Configuration registers** use `type: config` (uint8) or `type: config16` (uint16):

```yaml
number:
  - platform: centuryvspump
    name: Serial Timeout
    type: config
    page: 1
    address: 0x00
    min_value: 0
    max_value: 250

  - platform: centuryvspump
    name: Freeze Protection Speed
    type: config16
    page: 10
    address: 0x09
    min_value: 600
    max_value: 3450
    step: 50
```

See [Configuration Reference](docs/CONFIGURATION.md) for all available registers.

# Keywords

Shameless list of keywords to help other folk find this repo.

- Pentair SuperFlo VSP
- Jandy VS pump
- Hayward VS pump
- Century VS motor
- Regal VGreen motor

# Information sources

A list of sources that I discovered and thought might be of use when driving my pump.

- https://www.troublefreepool.com/threads/century-regal-vgreen-motor-automation.238733/
- https://community.home-assistant.io/t/intercept-echo-filter-modbus-communications-from-1-uart-hub-to-another-vs-pool-pump-solar-speed-override-project/340383
- https://github.com/Here-Be-Dragons/Pool-Controller/issues/4
- https://www.pentairpooleurope.com/sites/default/files/documents/manuals/2137__SB-CU-IMP-040F_-_Notice_SPEED_FC_2019_04_09_EN_light.pdf
- https://hayward-pool-assets.com/assets/documents/pools/pdf/manuals/2014-TriStar-VS-TSG.pdf?fromCDN=true
- http://www.desert-home.com/2019/03/i-finally-gave-up-on-my-hayward.html
- https://github.com/tagyoureit/nodejs-poolController/issues/393
- https://www.modbus.org/docs/Modbus_Application_Protocol_V1_1b.pdf
