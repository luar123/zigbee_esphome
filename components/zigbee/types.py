from esphome import automation
import esphome.codegen as cg

zigbee_ns = cg.esphome_ns.namespace("zigbee")
ZigBeeComponent = zigbee_ns.class_("ZigBeeComponent", cg.Component)
ZigBeeAttribute = zigbee_ns.class_("ZigBeeAttribute", cg.Component)
ZigBeeJoinTrigger = zigbee_ns.class_("ZigBeeJoinTrigger", automation.Trigger)
ZigBeeOnValueTrigger = zigbee_ns.class_(
    "ZigBeeOnValueTrigger", automation.Trigger.template(int), cg.Component
)
ResetZigbeeAction = zigbee_ns.class_(
    "ResetZigbeeAction", automation.Action, cg.Parented.template(ZigBeeComponent)
)
SetAttrAction = zigbee_ns.class_("SetAttrAction", automation.Action)
ReportAttrAction = zigbee_ns.class_(
    "ReportAttrAction", automation.Action, cg.Parented.template(ZigBeeAttribute)
)
ReportAction = zigbee_ns.class_(
    "ReportAction", automation.Action, cg.Parented.template(ZigBeeComponent)
)
