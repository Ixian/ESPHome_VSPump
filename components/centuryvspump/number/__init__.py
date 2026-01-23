from esphome.components import number
import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.const import CONF_ID, CONF_ADDRESS, CONF_TYPE
from esphome.cpp_helpers import logging

from .. import (
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
CONF_OFFSET = "offset"

# Number types - users specify page/address/offset in YAML
NUMBER_TYPE_DEMAND = "demand"
NUMBER_TYPE_CONFIG = "config"
NUMBER_TYPE_CONFIG16 = "config16"

CenturyVSPumpDemandNumber = century_vs_pump_ns.class_(
    "CenturyVSPumpDemandNumber", cg.Component, number.Number
)

CenturyVSPumpConfigNumber = century_vs_pump_ns.class_(
    "CenturyVSPumpConfigNumber", cg.Component, number.Number
)

CenturyVSPumpConfigNumber16 = century_vs_pump_ns.class_(
    "CenturyVSPumpConfigNumber16", cg.Component, number.Number
)

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

CONFIG_SCHEMA_BASE = (
    number.number_schema(CenturyVSPumpConfigNumber)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(CenturyVSPumpItemSchema)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(CenturyVSPumpConfigNumber),
            cv.Required(CONF_PAGE): cv.int_range(min=0, max=255),
            cv.Required(CONF_ADDRESS): cv.int_range(min=0, max=255),
            cv.Optional(CONF_STORE_TO_FLASH, default=True): cv.boolean,
            cv.Optional(CONF_OFFSET, default=0): cv.int_,
        }
    )
)

CONFIG16_SCHEMA_BASE = (
    number.number_schema(CenturyVSPumpConfigNumber16)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(CenturyVSPumpItemSchema)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(CenturyVSPumpConfigNumber16),
            cv.Required(CONF_PAGE): cv.int_range(min=0, max=255),
            cv.Required(CONF_ADDRESS): cv.int_range(min=0, max=255),
            cv.Optional(CONF_STORE_TO_FLASH, default=True): cv.boolean,
        }
    )
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        NUMBER_TYPE_DEMAND: DEMAND_SCHEMA,
        NUMBER_TYPE_CONFIG: CONFIG_SCHEMA_BASE,
        NUMBER_TYPE_CONFIG16: CONFIG16_SCHEMA_BASE,
    },
    default_type=NUMBER_TYPE_DEMAND,
)


async def to_code(config):
    num_type = config.get(CONF_TYPE, NUMBER_TYPE_DEMAND)

    if num_type == NUMBER_TYPE_DEMAND:
        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)
        await number.register_number(var, config, min_value=600, max_value=3450, step=50)

    elif num_type == NUMBER_TYPE_CONFIG16:
        page = config[CONF_PAGE]
        address = config[CONF_ADDRESS]

        var = cg.new_Pvariable(config[CONF_ID], page, address)
        await cg.register_component(var, config)
        await number.register_number(var, config)
        cg.add(var.set_store_to_flash(config[CONF_STORE_TO_FLASH]))

    else:  # NUMBER_TYPE_CONFIG
        page = config[CONF_PAGE]
        address = config[CONF_ADDRESS]
        offset = config.get(CONF_OFFSET, 0)

        var = cg.new_Pvariable(config[CONF_ID], page, address)
        await cg.register_component(var, config)
        await number.register_number(var, config)
        cg.add(var.set_store_to_flash(config[CONF_STORE_TO_FLASH]))
        cg.add(var.set_offset(offset))

    paren = await cg.get_variable(config[CONF_CENTURY_VS_PUMP_ID])
    cg.add(var.set_pump(paren))
    cg.add(paren.add_item(var))
