import datetime
import re

from esphome import automation
import esphome.codegen as cg
from esphome.components.esp32 import (
    CONF_PARTITIONS,
    # add_extra_build_file,
    add_idf_component,
    add_idf_sdkconfig_option,
    only_on_variant,
)
from esphome.components.esp32.const import VARIANT_ESP32C6, VARIANT_ESP32H2
import esphome.config_validation as cv
from esphome.const import (
    CONF_AP,
    CONF_AREA,
    CONF_DATE,
    CONF_DEVICE,
    CONF_ID,
    CONF_LAMBDA,
    CONF_NAME,
    CONF_ON_VALUE,
    CONF_POWER_SUPPLY,
    CONF_TRIGGER_ID,
    CONF_TYPE,
    CONF_VALUE,
    CONF_VERSION,
    CONF_WIFI,
)
from esphome.core import CORE, EsphomeError
import esphome.final_validate as fv

from .zigbee_const import ATTR_ACCESS, ATTR_TYPE, CLUSTER_ID, CLUSTER_ROLE, DEVICE_ID

DEPENDENCIES = ["esp32"]

CONF_ENDPOINTS = "endpoints"
CONF_DEVICE_TYPE = "device_type"
CONF_NUM = "num"
CONF_CLUSTERS = "clusters"
CONF_ON_JOIN = "on_join"
CONF_IDENT_TIME = "ident_time"
CONF_MANUFACTURER = "manufacturer"
CONF_ATTRIBUTES = "attributes"
CONF_ROLE = "role"
CONF_ENDPOINT = "endpoint"
CONF_CLUSTER = "cluster"
CONF_REPORT = "report"
CONF_ACCESS = "access"
CONF_SCALE = "scale"
CONF_ATTRIBUTE_ID = "attribute_id"
CONF_ZIGBEE_ID = "zigbee_id"

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
ReportAction = zigbee_ns.class_(
    "ReportAction", automation.Action, cg.Parented.template(ZigBeeComponent)
)


def get_c_size(bits, options):
    return str([n for n in options if n >= int(bits)][0])


def get_c_type(attr_type):
    if attr_type == "BOOL":
        return cg.bool_
    if attr_type == "SINGLE":
        return cg.float_
    if attr_type == "DOUBLE":
        return cg.double
    if "STRING" in attr_type:
        return cg.std_string
    test = re.match(r"(^U?)(\d{1,2})(BITMAP$|BIT$|BIT_ENUM$|$)", attr_type)
    if test and test.group(2):
        return getattr(cg, "uint" + get_c_size(test.group(2), [8, 16, 32, 64]))
    test = re.match(r"^S(\d{1,2})$", attr_type)
    if test and test.group(1):
        return getattr(cg, "int" + get_c_size(test.group(1), [16, 32, 64]))
    raise EsphomeError(f"Zigbee: type {attr_type} not supported or implemented")


def get_cv_by_type(attr_type):
    if attr_type == "BOOL":
        return cv.boolean
    if attr_type in ["SEMI", "SINGLE", "DOUBLE"]:
        return cv.float_
    if "STRING" in attr_type:
        return cv.string
    test = re.match(r"(^U?)(\d{1,2})(BITMAP$|BIT$|BIT_ENUM$|$)", attr_type)
    if test and test.group(2):
        return cv.positive_int
    test = re.match(r"^S(\d{1,2})$", attr_type)
    if test and test.group(1):
        return cv.int_
    raise cv.Invalid(f"Zigbee: type {attr_type} not supported or implemented")


def get_default_by_type(attr_type):
    return 0


def validate_clusters(config):
    for attr in config.get(CONF_ATTRIBUTES):
        if isinstance(config.get(CONF_ID), int) and config.get(CONF_ID) >= 0xFC00:
            if not {CONF_TYPE, CONF_ACCESS, CONF_VALUE} <= attr.keys():
                raise cv.Invalid(
                    f"Parameters {CONF_TYPE}, {CONF_VALUE} and {CONF_ACCESS} are need for custom cluster."
                )
    return config


def validate_attributes(config):
    if CONF_VALUE in config:
        config[CONF_VALUE] = get_cv_by_type(config[CONF_TYPE])(config[CONF_VALUE])
    else:
        config[CONF_VALUE] = get_default_by_type(config[CONF_TYPE])
    config[CONF_ACCESS] = (
        ATTR_ACCESS[config[CONF_ACCESS]] + config[CONF_REPORT] * 4
        if CONF_ACCESS in config
        else 0
    )

    return config


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
    if CONF_WIFI in fv.full_config.get():
        if CONF_AP in fv.full_config.get()[CONF_WIFI]:
            raise cv.Invalid("Zigbee can't be used together with an Wifi Access Point.")

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
            cv.Optional(CONF_IDENT_TIME): cv.string,
            cv.Optional(CONF_POWER_SUPPLY, default=0): cv.int_,  # make enum
            cv.Optional(CONF_VERSION, default=0): cv.int_,
            cv.Optional(CONF_AREA, default=0): cv.int_,  # make enum
            cv.Required(CONF_ENDPOINTS): cv.ensure_list(
                cv.Schema(
                    {
                        cv.Required(CONF_DEVICE_TYPE): cv.enum(DEVICE_ID, upper=True),
                        cv.Optional(CONF_NUM): cv.int_range(1, 240),
                        cv.Optional(CONF_CLUSTERS, default={}): cv.ensure_list(
                            cv.Schema(
                                {
                                    cv.Required(CONF_ID): cv.Any(
                                        cv.enum(CLUSTER_ID, upper=True),
                                        cv.int_range(0xFC00, 0xFFFF),
                                    ),
                                    cv.Optional(CONF_ROLE, default="Server"): cv.enum(
                                        CLUSTER_ROLE, upper=True
                                    ),
                                    cv.Optional(CONF_ATTRIBUTES): cv.ensure_list(
                                        cv.Schema(
                                            {
                                                cv.GenerateID(): cv.declare_id(
                                                    ZigBeeAttribute
                                                ),
                                                cv.Required(CONF_ATTRIBUTE_ID): cv.int_,
                                                cv.Required(CONF_TYPE): cv.enum(
                                                    ATTR_TYPE, upper=True
                                                ),
                                                cv.Optional(CONF_ACCESS): cv.enum(
                                                    ATTR_ACCESS, upper=True
                                                ),
                                                cv.Optional(CONF_VALUE): cv.valid,
                                                cv.Optional(
                                                    CONF_REPORT, default=False
                                                ): cv.boolean,
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
                                                cv.Optional(CONF_DEVICE): cv.use_id(
                                                    cg.EntityBase
                                                ),
                                                cv.Optional(
                                                    CONF_SCALE, default=1.0
                                                ): cv.float_,
                                                cv.Optional(
                                                    CONF_LAMBDA
                                                ): cv.returning_lambda,
                                            }
                                        ),
                                        validate_attributes,
                                    ),
                                },
                            ),
                            validate_clusters,
                        ),
                    }
                ),
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


def find_attr(conf, id):
    for ep in conf[CONF_ENDPOINTS]:
        for cl in ep[CONF_CLUSTERS]:
            for attr in cl[CONF_ATTRIBUTES]:
                if attr[CONF_ID] == id:
                    return attr
    raise EsphomeError(f"Zigbee: Cannot find attribute {id}.")


async def to_code(config):
    # use component manager:
    # add_extra_build_file(
    #    "src/idf_component.yml",
    #    os.path.join(os.path.dirname(__file__), "idf_component.yml"),
    # )
    # use add_idf_component: v1.5.1
    add_idf_component(
        name="espressif__esp-zboss-lib",
        repo="https://github.com/espressif/esp-zboss-lib.git",
        ref="354f742fab22cdb12b4ae61a103a7b438c7c975f",
    )
    add_idf_component(
        name="esp-zigbee-lib",
        repo="https://github.com/espressif/esp-zigbee-sdk.git",
        path="components/esp-zigbee-lib",
        ref="df56883084e89ebabf8a4985f92a023bd816a1b5",
    )
    add_idf_sdkconfig_option("CONFIG_ZB_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_ZB_ZED", True)
    add_idf_sdkconfig_option("CONFIG_ZB_RADIO_NATIVE", True)
    if CONF_WIFI in CORE.config:
        add_idf_sdkconfig_option("CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE", 4096)
        cg.add_define("CONFIG_WIFI_COEX")
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if CONF_NAME not in config:
        config[CONF_NAME] = CORE.name or ""
    cg.add(
        var.set_basic_cluster(
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
    if CONF_IDENT_TIME in config:
        cg.add(var.set_ident_time(config[CONF_IDENT_TIME]))
    for ep in config[CONF_ENDPOINTS]:
        cg.add(
            var.create_default_cluster(ep[CONF_NUM], DEVICE_ID[ep[CONF_DEVICE_TYPE]])
        )
        cg.add(
            var.add_cluster(ep[CONF_NUM], CLUSTER_ID["BASIC"], CLUSTER_ROLE["SERVER"])
        )
        if CONF_IDENT_TIME in config:
            cg.add(
                var.add_cluster(
                    ep[CONF_NUM],
                    CLUSTER_ID["IDENTIFY"],
                    CLUSTER_ROLE["SERVER"],
                )
            )
        for cl in ep[CONF_CLUSTERS]:
            cg.add(
                var.add_cluster(
                    ep[CONF_NUM],
                    cl[CONF_ID],
                    cl[CONF_ROLE],
                )
            )
            for attr in cl[CONF_ATTRIBUTES]:
                attr_var = cg.new_Pvariable(
                    attr[CONF_ID],
                    var,
                    ep[CONF_NUM],
                    cl[CONF_ID],
                    cl[CONF_ROLE],
                    attr[CONF_ATTRIBUTE_ID],
                    attr[CONF_TYPE],
                    attr[CONF_SCALE],
                )
                await cg.register_component(attr_var, attr)

                cg.add(
                    attr_var.add_attr(
                        attr[CONF_ACCESS],
                        attr[CONF_VALUE],
                    )
                )
                if attr[CONF_REPORT]:
                    cg.add(attr_var.set_report())

                if CONF_LAMBDA in attr:
                    lambda_ = await cg.process_lambda(
                        attr[CONF_LAMBDA],
                        [(cg.float_, "x")],
                        return_type=get_c_type(attr[CONF_TYPE]),
                    )

                if CONF_DEVICE in attr:
                    device = await cg.get_variable(attr[CONF_DEVICE])
                    template_arg = cg.TemplateArguments(get_c_type(attr[CONF_TYPE]))
                    if CONF_LAMBDA in attr:
                        lambda_ = await cg.process_lambda(
                            attr[CONF_LAMBDA],
                            [(cg.float_, "x")],
                            return_type=get_c_type(attr[CONF_TYPE]),
                        )
                        cg.add(attr_var.connect(template_arg, device, lambda_))
                    else:
                        cg.add(attr_var.connect(template_arg, device))

                for conf in attr.get(CONF_ON_VALUE, []):
                    trigger = cg.new_Pvariable(
                        conf[CONF_TRIGGER_ID],
                        cg.TemplateArguments(get_c_type(attr[CONF_TYPE])),
                        attr_var,
                    )
                    await cg.register_component(trigger, conf)
                    await automation.build_automation(
                        trigger, [(get_c_type(attr[CONF_TYPE]), "x")], conf
                    )
    for conf in config.get(CONF_ON_JOIN, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)


ZIGBEE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(ZigBeeComponent),
    }
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


ZIGBEE_ATTRIBUTE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(ZigBeeAttribute),
    }
)

ZIGBEE_SET_ATTR_SCHEMA = cv.All(
    automation.maybe_simple_id(
        ZIGBEE_ATTRIBUTE_ACTION_SCHEMA.extend(
            cv.Schema(
                {
                    cv.Required(CONF_VALUE): cv.templatable(cv.valid),
                }
            )
        )
    ),
)


@automation.register_action("zigbee.setAttr", SetAttrAction, ZIGBEE_SET_ATTR_SCHEMA)
async def zigbee_set_attr_to_code(config, action_id, template_arg, args):
    attr = find_attr(
        CORE.config["zigbee"],
        config[CONF_ID],
    )
    template_arg = cg.TemplateArguments(get_c_type(attr[CONF_TYPE]), template_arg.args)
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    template_ = await cg.templatable(
        config[CONF_VALUE], args, get_c_type(attr[CONF_TYPE])
    )
    cg.add(var.set_value(template_))

    return var
