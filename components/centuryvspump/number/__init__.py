from esphome.components import number
import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.const import CONF_ID, CONF_ADDRESS, CONF_TYPE
from esphome.cpp_helpers import logging

from .. import (
    add_century_vs_pump_base_properties,
    century_vs_pump_ns,
    CenturyVSPumpItemSchema,
)
from ..const import (
    CONF_CENTURY_VS_PUMP_ID,
    CONF_PAGE,
)

DEPENDENCIES = ["centuryvspump"]
CODEOWNERS = ["@gazoodle"]

_LOGGER = logging.getLogger(__name__)

CONF_STORE_TO_FLASH = "store_to_flash"

# Number types
NUMBER_TYPE_DEMAND = "demand"
NUMBER_TYPE_SERIAL_TIMEOUT = "serial_timeout"
NUMBER_TYPE_CONFIG = "config"
NUMBER_TYPE_CONFIG16 = "config16"

# Single-byte config presets
NUMBER_TYPE_FREEZE_ENABLE = "freeze_enable"
NUMBER_TYPE_FREEZE_TEMP = "freeze_temp"
NUMBER_TYPE_PRIMING_DURATION = "priming_duration"
NUMBER_TYPE_PAUSE_DURATION = "pause_duration"

# Uint16 config presets
NUMBER_TYPE_FREEZE_SPEED = "freeze_speed"
NUMBER_TYPE_PRIMING_SPEED = "priming_speed"

CenturyVSPumpDemandNumber = century_vs_pump_ns.class_(
    "CenturyVSPumpDemandNumber", cg.Component, number.Number
)

CenturyVSPumpConfigNumber = century_vs_pump_ns.class_(
    "CenturyVSPumpConfigNumber", cg.Component, number.Number
)

CenturyVSPumpConfigNumber16 = century_vs_pump_ns.class_(
    "CenturyVSPumpConfigNumber16", cg.Component, number.Number
)

# Preset definitions: (page, address, min_val, max_val, step, offset)
# Single-byte presets - offset is added to raw value when reading, subtracted when writing
CONFIG_PRESETS = {
    NUMBER_TYPE_SERIAL_TIMEOUT: (1, 0x00, 0, 250, 1, 0),
    NUMBER_TYPE_FREEZE_ENABLE: (10, 0x06, 0, 2, 1, 0),
    NUMBER_TYPE_FREEZE_TEMP: (10, 0x07, 32, 72, 1, 32),  # Pump stores 0-40, display 32-72°F
    NUMBER_TYPE_PRIMING_DURATION: (10, 0x23, 2, 15, 1, 0),  # 2=OFF, 3-15 min
    NUMBER_TYPE_PAUSE_DURATION: (10, 0x0B, 1, 255, 1, 0),
}

# Uint16 presets
CONFIG16_PRESETS = {
    NUMBER_TYPE_FREEZE_SPEED: (10, 0x09, 600, 3450, 25),
    NUMBER_TYPE_PRIMING_SPEED: (10, 0x24, 1500, 3450, 25),  # uint16 at 0x24/0x25
}

DEMAND_SCHEMA = (
    number.number_schema(CenturyVSPumpDemandNumber)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(CenturyVSPumpItemSchema)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(CenturyVSPumpDemandNumber),
        }
    )
)

CONFIG_BASE_SCHEMA = (
    number.number_schema(CenturyVSPumpConfigNumber)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(CenturyVSPumpItemSchema)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(CenturyVSPumpConfigNumber),
            cv.Optional(CONF_PAGE): cv.int_range(min=0, max=255),
            cv.Optional(CONF_ADDRESS): cv.int_range(min=0, max=255),
            cv.Optional(CONF_STORE_TO_FLASH, default=True): cv.boolean,
        }
    )
)

CONFIG16_BASE_SCHEMA = (
    number.number_schema(CenturyVSPumpConfigNumber16)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(CenturyVSPumpItemSchema)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(CenturyVSPumpConfigNumber16),
            cv.Optional(CONF_PAGE): cv.int_range(min=0, max=255),
            cv.Optional(CONF_ADDRESS): cv.int_range(min=0, max=255),
            cv.Optional(CONF_STORE_TO_FLASH, default=True): cv.boolean,
        }
    )
)


def validate_config_number(config):
    if config.get(CONF_TYPE) == NUMBER_TYPE_CONFIG:
        if CONF_PAGE not in config or CONF_ADDRESS not in config:
            raise cv.Invalid("Config type 'config' requires 'page' and 'address'")
    return config


def validate_config16_number(config):
    if config.get(CONF_TYPE) == NUMBER_TYPE_CONFIG16:
        if CONF_PAGE not in config or CONF_ADDRESS not in config:
            raise cv.Invalid("Config type 'config16' requires 'page' and 'address'")
    return config


CONFIG_SCHEMA = cv.typed_schema(
    {
        NUMBER_TYPE_DEMAND: DEMAND_SCHEMA,
        # Single-byte config types
        NUMBER_TYPE_SERIAL_TIMEOUT: CONFIG_BASE_SCHEMA,
        NUMBER_TYPE_FREEZE_ENABLE: CONFIG_BASE_SCHEMA,
        NUMBER_TYPE_FREEZE_TEMP: CONFIG_BASE_SCHEMA,
        NUMBER_TYPE_PRIMING_DURATION: CONFIG_BASE_SCHEMA,
        NUMBER_TYPE_PAUSE_DURATION: CONFIG_BASE_SCHEMA,
        NUMBER_TYPE_CONFIG: cv.All(CONFIG_BASE_SCHEMA, validate_config_number),
        # Uint16 config types
        NUMBER_TYPE_FREEZE_SPEED: CONFIG16_BASE_SCHEMA,
        NUMBER_TYPE_PRIMING_SPEED: CONFIG16_BASE_SCHEMA,
        NUMBER_TYPE_CONFIG16: cv.All(CONFIG16_BASE_SCHEMA, validate_config16_number),
    },
    default_type=NUMBER_TYPE_DEMAND,
)


async def to_code(config):
    num_type = config.get(CONF_TYPE, NUMBER_TYPE_DEMAND)

    if num_type == NUMBER_TYPE_DEMAND:
        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)
        await number.register_number(var, config, min_value=600, max_value=3450, step=50)
    elif num_type in CONFIG16_PRESETS or num_type == NUMBER_TYPE_CONFIG16:
        # Uint16 config types
        if num_type in CONFIG16_PRESETS:
            page, address, min_val, max_val, step = CONFIG16_PRESETS[num_type]
        else:
            page = config[CONF_PAGE]
            address = config[CONF_ADDRESS]
            min_val = 0
            max_val = 65535
            step = 1

        var = cg.new_Pvariable(config[CONF_ID], page, address)
        await cg.register_component(var, config)
        await number.register_number(var, config, min_value=min_val, max_value=max_val, step=step)
        cg.add(var.set_store_to_flash(config[CONF_STORE_TO_FLASH]))
    else:
        # Single-byte config types
        if num_type in CONFIG_PRESETS:
            page, address, min_val, max_val, step, offset = CONFIG_PRESETS[num_type]
        else:
            page = config[CONF_PAGE]
            address = config[CONF_ADDRESS]
            min_val = 0
            max_val = 255
            step = 1
            offset = 0

        var = cg.new_Pvariable(config[CONF_ID], page, address)
        await cg.register_component(var, config)
        await number.register_number(var, config, min_value=min_val, max_value=max_val, step=step)
        cg.add(var.set_store_to_flash(config[CONF_STORE_TO_FLASH]))
        cg.add(var.set_offset(offset))

    paren = await cg.get_variable(config[CONF_CENTURY_VS_PUMP_ID])
    cg.add(var.set_pump(paren))
    cg.add(paren.add_item(var))
    await add_century_vs_pump_base_properties(var, config, type(var))
