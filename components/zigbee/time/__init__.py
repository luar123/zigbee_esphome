import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import time as time_
from esphome.cpp_generator import get_variable
from esphome.const import (CONF_ID)
from .. import (CONF_ZIGBEE_ID)

DEPENDENCIES = ["zigbee"]

zigbee_ns = cg.esphome_ns.namespace("zigbee")
ZigbeeTime = zigbee_ns.class_("ZigbeeTime", time_.RealTimeClock)
ZigBeeComponent = zigbee_ns.class_("ZigBeeComponent", cg.Component)
CONFIG_SCHEMA = time_.TIME_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(ZigbeeTime),
        cv.GenerateID(CONF_ZIGBEE_ID): cv.use_id(ZigBeeComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    zb = await get_variable(config[CONF_ZIGBEE_ID])
    var = cg.new_Pvariable(config[CONF_ID], zb)
    await cg.register_component(var, config)
    await time_.register_time(var, config)
