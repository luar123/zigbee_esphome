import esphome.codegen as cg
from esphome.components import time as time_
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_generator import get_variable

from ..const import CONF_ZIGBEE_ID

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
    cg.add_define("USE_ZIGBEE_TIME")
    zb = await get_variable(config[CONF_ZIGBEE_ID])
    var = cg.new_Pvariable(config[CONF_ID], zb)
    await cg.register_component(var, config)
    await time_.register_time(var, config)
