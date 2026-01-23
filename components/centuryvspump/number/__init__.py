from esphome.components import number
import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.const import (
    CONF_ID,
    CONF_ADDRESS,
    CONF_TYPE,
    CONF_MIN_VALUE,
    CONF_MAX_VALUE,
    CONF_STEP,
)
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

# Common number options for min/max/step
NUMBER_OPTIONS = {
    cv.Optional(CONF_MIN_VALUE): cv.float_,
    cv.Optional(CONF_MAX_VALUE): cv.float_,
    cv.Optional(CONF_STEP): cv.positive_float,
}

DEMAND_SCHEMA = (
    number.number_schema(CenturyVSPumpDemandNumber)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(CenturyVSPumpItemSchema)
    .extend(NUMBER_OPTIONS)
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
    .extend(NUMBER_OPTIONS)
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
    .extend(NUMBER_OPTIONS)
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


def get_number_kwargs(config):
    """Extract min_value/max_value/step from config for register_number."""
    kwargs = {}
    if CONF_MIN_VALUE in config:
        kwargs["min_value"] = config[CONF_MIN_VALUE]
    if CONF_MAX_VALUE in config:
        kwargs["max_value"] = config[CONF_MAX_VALUE]
    if CONF_STEP in config:
        kwargs["step"] = config[CONF_STEP]
    return kwargs


async def to_code(config):
    num_type = config.get(CONF_TYPE, NUMBER_TYPE_DEMAND)
    kwargs = get_number_kwargs(config)

    if num_type == NUMBER_TYPE_DEMAND:
        # Default values for demand if not specified
        if "min_value" not in kwargs:
            kwargs["min_value"] = 600
        if "max_value" not in kwargs:
            kwargs["max_value"] = 3450
        if "step" not in kwargs:
            kwargs["step"] = 50

        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)
        await number.register_number(var, config, **kwargs)

    elif num_type == NUMBER_TYPE_CONFIG16:
        # Defaults for config16: uint16 range, step=1
        if "min_value" not in kwargs:
            kwargs["min_value"] = 0
        if "max_value" not in kwargs:
            kwargs["max_value"] = 65535
        if "step" not in kwargs:
            kwargs["step"] = 1

        page = config[CONF_PAGE]
        address = config[CONF_ADDRESS]

        var = cg.new_Pvariable(config[CONF_ID], page, address)
        await cg.register_component(var, config)
        await number.register_number(var, config, **kwargs)
        cg.add(var.set_store_to_flash(config[CONF_STORE_TO_FLASH]))

    else:  # NUMBER_TYPE_CONFIG
        # Defaults for config: uint8 range, step=1
        if "min_value" not in kwargs:
            kwargs["min_value"] = 0
        if "max_value" not in kwargs:
            kwargs["max_value"] = 255
        if "step" not in kwargs:
            kwargs["step"] = 1

        page = config[CONF_PAGE]
        address = config[CONF_ADDRESS]
        offset = config.get(CONF_OFFSET, 0)

        var = cg.new_Pvariable(config[CONF_ID], page, address)
        await cg.register_component(var, config)
        await number.register_number(var, config, **kwargs)
        cg.add(var.set_store_to_flash(config[CONF_STORE_TO_FLASH]))
        cg.add(var.set_offset(offset))

    paren = await cg.get_variable(config[CONF_CENTURY_VS_PUMP_ID])
    cg.add(var.set_pump(paren))
    cg.add(paren.add_item(var))
