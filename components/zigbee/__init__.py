import os
import datetime
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components.esp32 import (
    # add_idf_component,
    add_idf_sdkconfig_option,
    add_extra_build_file,
    CONF_PARTITIONS,
    only_on_variant,
)
from esphome.components.esp32.const import (
    VARIANT_ESP32H2,
    VARIANT_ESP32C6,
)
from esphome.core import CORE
from esphome import automation
import esphome.final_validate as fv


from esphome.const import (
    CONF_ID,
    CONF_AREA,
    CONF_DATE,
    CONF_TRIGGER_ID,
    CONF_NAME,
    CONF_POWER_SUPPLY,
    CONF_TYPE,
    CONF_VALUE,
    CONF_VERSION,
    CONF_ON_VALUE,
    CONF_ATTRIBUTE,
)

from .zigbee_const import (
    DEVICE_ID,
    CLUSTER_ID,
    CLUSTER_ROLE,
    ATTR_TYPE,
)

DEPENDENCIES = ["esp32"]

CONF_ENDPOINTS = "endpoints"
CONF_DEVICE_ID = "device_type"
CONF_ENDPOINT_NUM = "num"
CONF_CLUSTERS = "clusters"
CONF_ON_JOIN = "on_join"
CONF_IDENT = "ident time"
CONF_MANUFACTURER = "manufacturer"
CONF_ATTRIBUTES = "attributes"
CONF_ROLE = "role"
CONF_ENDPOINT = "endpoint"
CONF_CLUSTER = "cluster"
CONF_REPORT = "report"

zigbee_ns = cg.esphome_ns.namespace("zigbee")
ZigBeeComponent = zigbee_ns.class_("ZigBeeComponent", cg.Component)
ZigBeeJoinTrigger = zigbee_ns.class_("ZigBeeJoinTrigger", automation.Trigger)
ZigBeeOnValueTrigger = zigbee_ns.class_(
    "ZigBeeOnValueTrigger", automation.Trigger.template(int), cg.Component
)
ResetZigbeeAction = zigbee_ns.class_(
    "ResetZigbeeAction", automation.Action, cg.Parented.template(ZigBeeComponent)
)
SetAttrAction = zigbee_ns.class_("SetAttrAction", automation.Action)
ReportAction = zigbee_ns.class_(
    "ReportAction", automation.Action, cg.Parented.template(ZigBeeComponent)
)


def final_validate(config):
    esp_conf = fv.full_config.get()["esp32"]
    if CONF_PARTITIONS in esp_conf:
        with open(
            CORE.relative_config_path(esp_conf[CONF_PARTITIONS]), encoding="utf8"
        ) as f:
            partitions = f.read()
            if ("zb_storage" not in partitions) and ("zb_fct" not in partitions):
                raise cv.Invalid(
                    "Add \n'zb_storage, data, fat,   , 16K,'\n'zb_fct, data, fat, , 1K,'\n to your custom partition table."
                )
    else:
        raise cv.Invalid(
            f"Use '{CONF_PARTITIONS}' in esp32 to specify a custom partition table including zigbee partitions"
        )
    return config


FINAL_VALIDATE_SCHEMA = cv.Schema(final_validate)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ZigBeeComponent),
            cv.Optional(CONF_NAME): cv.string,
            cv.Optional(CONF_MANUFACTURER, default="esphome"): cv.string,
            cv.Optional(
                CONF_DATE, default=datetime.datetime.now().strftime("%Y%m%d")
            ): cv.string,
            cv.Optional(CONF_IDENT): cv.string,
            cv.Optional(CONF_POWER_SUPPLY, default=0): cv.int_,  # make enum
            cv.Optional(CONF_VERSION, default=0): cv.int_,
            cv.Optional(CONF_AREA, default=0): cv.int_,  # make enum
            cv.Required(CONF_ENDPOINTS): cv.ensure_list(
                cv.Schema(
                    {
                        cv.Required(CONF_DEVICE_ID): cv.enum(DEVICE_ID, upper=True),
                        cv.Optional(CONF_ENDPOINT_NUM): cv.int_range(1, 240),
                        cv.Optional(CONF_CLUSTERS, default={}): cv.ensure_list(
                            cv.Schema(
                                {
                                    cv.Required(CONF_ID): cv.enum(
                                        CLUSTER_ID, upper=True
                                    ),
                                    cv.Optional(CONF_ROLE, default="Server"): cv.enum(
                                        CLUSTER_ROLE, upper=True
                                    ),
                                    cv.Optional(CONF_ATTRIBUTES): cv.ensure_list(
                                        cv.Schema(
                                            {
                                                cv.Required(CONF_ID): cv.int_,
                                                cv.Optional(CONF_TYPE): cv.enum(
                                                    ATTR_TYPE, upper=True
                                                ),
                                                cv.Optional(CONF_VALUE): cv.valid,
                                                cv.Optional(CONF_REPORT): cv.valid,
                                                cv.Optional(
                                                    CONF_ON_VALUE
                                                ): automation.validate_automation(
                                                    {
                                                        cv.GenerateID(
                                                            CONF_TRIGGER_ID
                                                        ): cv.declare_id(
                                                            ZigBeeOnValueTrigger
                                                        ),
                                                    }
                                                ),
                                            }
                                        )
                                    ),
                                }
                            )
                        ),
                    }
                )
            ),
            cv.Optional(CONF_ON_JOIN): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ZigBeeJoinTrigger),
                }
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.require_framework_version(esp_idf=cv.Version(5, 1, 2)),
    only_on_variant(
        supported=[
            VARIANT_ESP32H2,
            VARIANT_ESP32C6,
        ]
    ),
)


async def to_code(config):
    add_extra_build_file(
        "src/idf_component.yml",
        os.path.join(os.path.dirname(__file__), "idf_component.yml"),
    )
    # change this to idf component (not working at the moment)
    #    add_idf_component(
    #            name="esp-zboss-lib",
    #            repo="https://github.com/espressif/esp-zboss-lib.git",
    #            ref="838714a0aa580ebcbd2e59652666eceb9b7e6649",
    #        )
    #    add_idf_component(
    #            name="esp-zigbee-lib",
    #            repo="https://github.com/espressif/esp-zigbee-sdk.git",
    #            path="components/esp-zigbee-lib",
    #            ref="4d68ccf0443770b40610892a1e598963d7fb154f",
    #        )
    add_idf_sdkconfig_option("CONFIG_ZB_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_ZB_ZED", True)
    add_idf_sdkconfig_option("CONFIG_ZB_RADIO_NATIVE", True)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if CONF_NAME not in config:
        config[CONF_NAME] = CORE.name or ""
    cg.add(
        var.create_basic_cluster(
            config[CONF_NAME],
            config[CONF_MANUFACTURER],
            config[CONF_DATE],
            config[CONF_POWER_SUPPLY],
            config[CONF_VERSION],
            0,
            0,
            CORE.area or "",
            config[CONF_AREA],
        )
    )
    if CONF_IDENT in config:
        cg.add(var.create_ident_cluster(config[CONF_IDENT]))
    for ep in config[CONF_ENDPOINTS]:
        cg.add(
            var.create_default_cluster(
                ep[CONF_ENDPOINT_NUM], DEVICE_ID[ep[CONF_DEVICE_ID]]
            )
        )
        cg.add(
            var.add_cluster(
                ep[CONF_ENDPOINT_NUM], CLUSTER_ID["BASIC"], CLUSTER_ROLE["SERVER"]
            )
        )
        if CONF_IDENT in config:
            cg.add(
                var.add_cluster(
                    ep[CONF_ENDPOINT_NUM],
                    CLUSTER_ID["IDENTIFY"],
                    CLUSTER_ROLE["SERVER"],
                )
            )
        for cl in ep[CONF_CLUSTERS]:
            cg.add(
                var.add_cluster(
                    ep[CONF_ENDPOINT_NUM],
                    CLUSTER_ID[cl[CONF_ID]],
                    CLUSTER_ROLE[cl[CONF_ROLE]],
                )
            )
            for attr in cl[CONF_ATTRIBUTES]:
                if CONF_VALUE in attr:
                    cg.add(
                        var.add_attr(
                            ep[CONF_ENDPOINT_NUM],
                            CLUSTER_ID[cl[CONF_ID]],
                            CLUSTER_ROLE[cl[CONF_ROLE]],
                            attr[CONF_ID],
                            attr[CONF_VALUE],
                        )
                    )
                if CONF_REPORT in attr and attr[CONF_REPORT] is True:
                    cg.add(
                        var.set_report(
                            ep[CONF_ENDPOINT_NUM],
                            CLUSTER_ID[cl[CONF_ID]],
                            CLUSTER_ROLE[cl[CONF_ROLE]],
                            attr[CONF_ID],
                        )
                    )

                for conf in attr.get(CONF_ON_VALUE, []):
                    trigger = cg.new_Pvariable(
                        conf[CONF_TRIGGER_ID], cg.TemplateArguments(int), var
                    )
                    await cg.register_component(trigger, conf)
                    cg.add(
                        trigger.set_attr(
                            ep[CONF_ENDPOINT_NUM],
                            CLUSTER_ID[cl[CONF_ID]],
                            attr[CONF_ID],
                            attr[CONF_TYPE],
                        )
                    )
                    await automation.build_automation(trigger, [(int, "x")], conf)
    for conf in config.get(CONF_ON_JOIN, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)


ZIGBEE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(ZigBeeComponent),
    }
)

ZIGBEE_SET_ATTR_SCHEMA = cv.All(
    automation.maybe_simple_id(
        ZIGBEE_ACTION_SCHEMA.extend(
            cv.Schema(
                {
                    cv.Required(CONF_ENDPOINT): cv.int_range(1, 240),
                    cv.Required(CONF_CLUSTER): cv.enum(CLUSTER_ID, upper=True),
                    cv.Optional(CONF_ROLE, default="Server"): cv.enum(
                        CLUSTER_ROLE, upper=True
                    ),
                    cv.Required(CONF_ATTRIBUTE): cv.int_,
                    cv.Required(CONF_VALUE): cv.templatable(cv.valid),
                }
            )
        )
    ),
)


@automation.register_action(
    "zigbee.reset", ResetZigbeeAction, automation.maybe_simple_id(ZIGBEE_ACTION_SCHEMA)
)
async def reset_zigbee_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "zigbee.report", ReportAction, automation.maybe_simple_id(ZIGBEE_ACTION_SCHEMA)
)
async def report_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action("zigbee.setAttr", SetAttrAction, ZIGBEE_SET_ATTR_SCHEMA)
async def zigbee_set_attr_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    cg.add(
        var.set_target(
            config[CONF_ENDPOINT],
            config[CONF_CLUSTER],
            config[CONF_ROLE],
            config[CONF_ATTRIBUTE],
        )
    )
    template_ = await cg.templatable(config[CONF_VALUE], args, cg.int64)
    cg.add(var.set_value(template_))

    return var
