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

NUMBER_TYPE_DEMAND = "demand"
NUMBER_TYPE_SERIAL_TIMEOUT = "serial_timeout"
NUMBER_TYPE_CONFIG = "config"

CenturyVSPumpDemandNumber = century_vs_pump_ns.class_(
    "CenturyVSPumpDemandNumber", cg.Component, number.Number
)

CenturyVSPumpConfigNumber = century_vs_pump_ns.class_(
    "CenturyVSPumpConfigNumber", cg.Component, number.Number
)

CONFIG_PRESETS = {
    NUMBER_TYPE_SERIAL_TIMEOUT: (1, 0, 0, 250, 1),
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


def validate_config_number(config):
    if config.get(CONF_TYPE) == NUMBER_TYPE_CONFIG:
        if CONF_PAGE not in config or CONF_ADDRESS not in config:
            raise cv.Invalid("Config type 'config' requires 'page' and 'address'")
    return config


CONFIG_SCHEMA = cv.typed_schema(
    {
        NUMBER_TYPE_DEMAND: DEMAND_SCHEMA,
        NUMBER_TYPE_SERIAL_TIMEOUT: CONFIG_BASE_SCHEMA,
        NUMBER_TYPE_CONFIG: cv.All(CONFIG_BASE_SCHEMA, validate_config_number),
    },
    default_type=NUMBER_TYPE_DEMAND,
)


async def to_code(config):
    num_type = config.get(CONF_TYPE, NUMBER_TYPE_DEMAND)

    if num_type == NUMBER_TYPE_DEMAND:
        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)
        await number.register_number(var, config, min_value=600, max_value=3450, step=50)
    else:
        if num_type in CONFIG_PRESETS:
            page, address, min_val, max_val, step = CONFIG_PRESETS[num_type]
        else:
            page = config[CONF_PAGE]
            address = config[CONF_ADDRESS]
            min_val = 0
            max_val = 255
            step = 1

        var = cg.new_Pvariable(config[CONF_ID], page, address)
        await cg.register_component(var, config)
        await number.register_number(var, config, min_value=min_val, max_value=max_val, step=step)
        cg.add(var.set_store_to_flash(config[CONF_STORE_TO_FLASH]))

    paren = await cg.get_variable(config[CONF_CENTURY_VS_PUMP_ID])
    cg.add(var.set_pump(paren))
    cg.add(paren.add_item(var))
    await add_century_vs_pump_base_properties(var, config, type(var))
