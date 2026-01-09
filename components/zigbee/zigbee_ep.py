import copy

from esphome.components import light, output, switch
import esphome.config_validation as cv
from esphome.const import (
    CONF_COMPONENTS,
    CONF_DEVICE,
    CONF_DEVICE_CLASS,
    CONF_ID,
    CONF_MAX_LENGTH,
    CONF_TYPE,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_VALUE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLUME_FLOW_RATE,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_HECTOPASCAL,
    UNIT_HERTZ,
    UNIT_KILOWATT,
    UNIT_KILOWATT_HOURS,
    UNIT_LUX,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_OHM,
    UNIT_PARTS_PER_MILLION,
    UNIT_PASCAL,
    UNIT_PERCENT,
    UNIT_SECOND,
    UNIT_VOLT,
    UNIT_WATT,
)
from esphome.core import CORE, ID

from .const import (
    CONF_ACCESS,
    CONF_AS_GENERIC,
    CONF_ATTRIBUTE_ID,
    CONF_ATTRIBUTES,
    CONF_CLUSTERS,
    CONF_DEVICE_TYPE,
    CONF_ENDPOINTS,
    CONF_NUM,
    CONF_REPORT,
    CONF_ROLE,
    CONF_SCALE,
    AnalogInputType,
    BacnetUnit,
    BinarySensor,
    Sensor,
)
from .types import ZigBeeAttribute
from .zigbee_const import CLUSTER_ROLE

# endpoint configs:
ep_configs = {
    "binary_input": {
        CONF_DEVICE_TYPE: "CUSTOM_ATTR",
        CONF_CLUSTERS: [
            {
                CONF_ID: "BINARY_INPUT",
                CONF_ROLE: CLUSTER_ROLE["SERVER"],
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x55,
                        CONF_VALUE: False,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "BOOL",
                        CONF_REPORT: True,
                        CONF_SCALE: 1,
                        CONF_DEVICE: None,
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x51,
                        CONF_VALUE: False,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "BOOL",
                        CONF_REPORT: False,
                        CONF_SCALE: 1,
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x6F,
                        CONF_VALUE: 0,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "8BITMAP",
                        CONF_REPORT: False,
                        CONF_SCALE: 1,
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x1C,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "CHAR_STRING",
                        CONF_REPORT: False,
                        CONF_SCALE: 1,
                    },
                ],
            },
        ],
    },
    "analog_input": {
        CONF_DEVICE_TYPE: "CUSTOM_ATTR",
        CONF_CLUSTERS: [
            {
                CONF_ID: "ANALOG_INPUT",
                CONF_ROLE: CLUSTER_ROLE["SERVER"],
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x55,
                        CONF_VALUE: False,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "SINGLE",
                        CONF_REPORT: True,
                        CONF_SCALE: 1,
                        CONF_DEVICE: None,
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x51,
                        CONF_VALUE: False,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "BOOL",
                        CONF_REPORT: False,
                        CONF_SCALE: 1,
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x6F,
                        CONF_VALUE: 0,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "8BITMAP",
                        CONF_REPORT: False,
                        CONF_SCALE: 1,
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x1C,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "CHAR_STRING",
                        CONF_REPORT: False,
                        CONF_SCALE: 1,
                    },
                ],
            },
        ],
    },
    "binary_output": {
        CONF_DEVICE_TYPE: "CUSTOM_ATTR",
        CONF_CLUSTERS: [
            {
                CONF_ID: "BINARY_OUTPUT",
                CONF_ROLE: CLUSTER_ROLE["SERVER"],
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x55,
                        CONF_VALUE: False,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "BOOL",
                        CONF_REPORT: True,
                        CONF_SCALE: 1,
                        CONF_DEVICE: None,
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x51,
                        CONF_VALUE: False,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "BOOL",
                        CONF_REPORT: False,
                        CONF_SCALE: 1,
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x6F,
                        CONF_VALUE: 0,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "8BITMAP",
                        CONF_REPORT: False,
                        CONF_SCALE: 1,
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x1C,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "CHAR_STRING",
                        CONF_REPORT: False,
                        CONF_SCALE: 1,
                    },
                ],
            },
        ],
    },
    "temperature": {
        CONF_DEVICE_TYPE: "TEMPERATURE_SENSOR",
        CONF_CLUSTERS: [
            {
                CONF_ID: "TEMP_MEASUREMENT",
                CONF_ROLE: CLUSTER_ROLE["SERVER"],
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x0,
                        CONF_VALUE: 0,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "S16",
                        CONF_REPORT: True,
                        CONF_SCALE: 100,
                        CONF_DEVICE: None,
                    },
                ],
            },
        ],
    },
    "on_off": {
        CONF_DEVICE_TYPE: "ON_OFF_OUTPUT",
        CONF_CLUSTERS: [
            {
                CONF_ID: "ON_OFF",
                CONF_ROLE: CLUSTER_ROLE["SERVER"],
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x0,
                        CONF_VALUE: False,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "BOOL",
                        CONF_REPORT: True,
                        CONF_SCALE: 1,
                        CONF_DEVICE: None,
                    },
                ],
            },
        ],
    },
    "on_off_light": {
        CONF_DEVICE_TYPE: "ON_OFF_LIGHT",
        CONF_CLUSTERS: [
            {
                CONF_ID: "ON_OFF",
                CONF_ROLE: CLUSTER_ROLE["SERVER"],
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x0,
                        CONF_VALUE: False,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "BOOL",
                        CONF_REPORT: True,
                        CONF_SCALE: 1,
                        CONF_DEVICE: None,
                    },
                ],
            },
        ],
    },
    "color_light": {
        CONF_DEVICE_TYPE: "COLOR_DIMMABLE_LIGHT",
        CONF_CLUSTERS: [
            {
                CONF_ID: "ON_OFF",
                CONF_ROLE: CLUSTER_ROLE["SERVER"],
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x0,
                        CONF_VALUE: False,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "BOOL",
                        CONF_REPORT: True,
                        CONF_SCALE: 1,
                        CONF_DEVICE: None,
                    },
                ],
            },
            {
                CONF_ID: "LEVEL_CONTROL",
                CONF_ROLE: CLUSTER_ROLE["SERVER"],
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x0,
                        CONF_VALUE: 255,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "U8",
                        CONF_REPORT: True,
                        CONF_SCALE: 1,
                        CONF_DEVICE: None,
                    },
                ],
            },
            {
                CONF_ID: "COLOR_CONTROL",
                CONF_ROLE: CLUSTER_ROLE["SERVER"],
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x3,
                        CONF_VALUE: 0x616B,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "U16",
                        CONF_REPORT: True,
                        CONF_SCALE: 1,
                        CONF_DEVICE: None,
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x4,
                        CONF_VALUE: 0x607D,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "U16",
                        CONF_REPORT: True,
                        CONF_SCALE: 1,
                        CONF_DEVICE: None,
                    },
                    {
                        CONF_ATTRIBUTE_ID: 0x400A,
                        CONF_VALUE: 8,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "16BITMAP",
                        CONF_REPORT: False,
                        CONF_SCALE: 1,
                    },
                ],
            },
        ],
    },
    "level_light": {
        CONF_DEVICE_TYPE: "DIMMABLE_LIGHT",
        CONF_CLUSTERS: [
            {
                CONF_ID: "ON_OFF",
                CONF_ROLE: CLUSTER_ROLE["SERVER"],
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x0,
                        CONF_VALUE: False,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "BOOL",
                        CONF_REPORT: True,
                        CONF_SCALE: 1,
                        CONF_DEVICE: None,
                    },
                ],
            },
            {
                CONF_ID: "LEVEL_CONTROL",
                CONF_ROLE: CLUSTER_ROLE["SERVER"],
                CONF_ATTRIBUTES: [
                    {
                        CONF_ATTRIBUTE_ID: 0x0,
                        CONF_VALUE: 255,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "U8",
                        CONF_REPORT: True,
                        CONF_SCALE: 1,
                        CONF_DEVICE: None,
                    },
                ],
            },
        ],
    },
}


def get_next_ep_num(eps):
    try:
        ep_num = [i for i in range(1, 240) if i not in eps][0]
        eps.append(ep_num)
    except IndexError as e:
        raise cv.Invalid(
            "Too many devices. Zigbee can define only 240 endpoints."
        ) from e
    return ep_num


ANALOG_INPUT_APPTYPE = {
    (DEVICE_CLASS_TEMPERATURE, UNIT_CELSIUS): AnalogInputType.Temp_Degrees_C,
    (DEVICE_CLASS_HUMIDITY, UNIT_PERCENT): AnalogInputType.Relative_Humidity_Percent,
    (DEVICE_CLASS_PRESSURE, UNIT_PASCAL): AnalogInputType.Pressure_Pascal,
    (DEVICE_CLASS_VOLUME_FLOW_RATE, "L/s"): AnalogInputType.Flow_Liters_Per_Sec,
    (DEVICE_CLASS_CURRENT, UNIT_AMPERE): AnalogInputType.Current_Amps,
    (DEVICE_CLASS_FREQUENCY, UNIT_HERTZ): AnalogInputType.Frequency_Hz,
    (DEVICE_CLASS_POWER, UNIT_WATT): AnalogInputType.Power_Watts,
    (DEVICE_CLASS_POWER, UNIT_KILOWATT): AnalogInputType.Power_Kilo_Watts,
    (DEVICE_CLASS_ENERGY, UNIT_KILOWATT_HOURS): AnalogInputType.Energy_Kilo_Watt_Hours,
    (DEVICE_CLASS_DURATION, UNIT_SECOND): AnalogInputType.Time_Seconds,
}
"""
not implemented:
    AnalogInputType.Percentage
    AnalogInputType.Parts_Per_Million
    AnalogInputType.Rotational_Speed_RPM
    AnalogInputType.Count
    AnalogInputType.Enthalpy_KJoules_Per_Kg
"""

BACNET_UNIT = {
    UNIT_CELSIUS: BacnetUnit.DEGREES_CELSIUS,
    "°F": BacnetUnit.DEGREES_FAHRENHEIT,
    UNIT_PERCENT: BacnetUnit.PERCENT,
    UNIT_LUX: BacnetUnit.LUXES,
    UNIT_VOLT: BacnetUnit.VOLTS,
    UNIT_MICROGRAMS_PER_CUBIC_METER: BacnetUnit.MICROGRAMS_PER_CUBIC_METER,
    UNIT_PARTS_PER_MILLION: BacnetUnit.PARTS_PER_MILLION,
    UNIT_OHM: BacnetUnit.OHMS,
    UNIT_HECTOPASCAL: BacnetUnit.HECTOPASCALS,
}


def create_device_ep(eps, dev, generic=False):
    ep = {}
    ep[CONF_NUM] = get_next_ep_num(eps)
    if dev["id"].type.inherits_from(Sensor):
        if dev.get(CONF_DEVICE_CLASS, "") in ep_configs and not generic:
            ep.update(copy.deepcopy(ep_configs[dev[CONF_DEVICE_CLASS]]))
        else:
            # get application type from device class and meas unit
            # if none get BACNET unit from meas unit
            ep.update(copy.deepcopy(ep_configs["analog_input"]))
            dev_class = dev.get(CONF_DEVICE_CLASS)
            unit = dev.get(CONF_UNIT_OF_MEASUREMENT)
            apptype = ANALOG_INPUT_APPTYPE.get((dev_class, unit))
            bacunit = BACNET_UNIT.get(unit)
            if apptype:
                ep[CONF_CLUSTERS][0][CONF_ATTRIBUTES].append(
                    {
                        CONF_ATTRIBUTE_ID: 0x100,
                        CONF_VALUE: (apptype << 16) + 0xFFFF,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "U32",
                        CONF_REPORT: False,
                        CONF_SCALE: 1,
                    },
                )
            elif bacunit:
                ep[CONF_CLUSTERS][0][CONF_ATTRIBUTES].append(
                    {
                        CONF_ATTRIBUTE_ID: 0x75,
                        CONF_VALUE: bacunit,
                        CONF_ACCESS: 0,
                        CONF_TYPE: "16BIT_ENUM",
                        CONF_REPORT: False,
                        CONF_SCALE: 1,
                    },
                )

    elif dev["id"].type.inherits_from(switch.Switch):
        if generic:
            ep.update(copy.deepcopy(ep_configs["binary_output"]))
        else:
            ep.update(copy.deepcopy(ep_configs["on_off"]))
    elif dev["id"].type.inherits_from(BinarySensor):
        ep.update(copy.deepcopy(ep_configs["binary_input"]))
    elif dev["id"].type.inherits_from(light.LightState):
        if dev["platform"] in ["binary", "status_led"] or (
            "output" in dev and dev["output"].type.inherits_from(output.BinaryOutput)
        ):
            if generic:
                ep.update(copy.deepcopy(ep_configs["binary_output"]))
            else:
                ep.update(copy.deepcopy(ep_configs["on_off_light"]))
        elif (
            dev["platform"] in ["monochromatic"]
            or "dimmer" in dev["platform"]
            or (
                "output" in dev and dev["output"].type.inherits_from(output.FloatOutput)
            )
        ):
            if generic:
                ep.update(copy.deepcopy(ep_configs["analog_output"]))
                ep[CONF_CLUSTERS].append(
                    copy.deepcopy(ep_configs["binary_output"][CONF_CLUSTERS])
                )
            else:
                ep.update(copy.deepcopy(ep_configs["level_light"]))
        else:
            ep.update(copy.deepcopy(ep_configs["color_light"]))
    for cl in ep.get(CONF_CLUSTERS, []):
        for attr in cl[CONF_ATTRIBUTES]:
            if CONF_DEVICE in attr:  # connect device
                attr[CONF_DEVICE] = dev["id"]
            if (
                attr[CONF_ATTRIBUTE_ID] == 0x1C
                and CONF_VALUE not in attr
                and "name" in dev
            ):  # set name
                name = dev["name"].encode("ascii", "ignore").decode()  # use unidecode
                attr[CONF_VALUE] = str(name)
                attr[CONF_MAX_LENGTH] = len(str(name))
            id = ID(None, is_declaration=True, type=ZigBeeAttribute)
            id.resolve(CORE.component_ids)
            CORE.component_ids.add(id.id)
            attr[CONF_ID] = id
    return ep


def get_device_entries(conf: list, component_type):
    devices = []
    for d in conf:
        if CONF_ID in d and d[CONF_ID].type.inherits_from(component_type):
            devices.append(d)
        else:
            for dd in d.values():
                if isinstance(dd, dict):
                    devices.extend(get_device_entries([dd], component_type))
    return devices


def create_ep(config, full_conf):
    eps = []
    comp_ids = len(CORE.component_ids)
    if CONF_ENDPOINTS in config:
        eps = [ep.get(CONF_NUM) for ep in config[CONF_ENDPOINTS]]
        for ep in config[CONF_ENDPOINTS]:
            if CONF_NUM not in ep:
                ep[CONF_NUM] = get_next_ep_num(eps)
    ep_list = config.get(CONF_ENDPOINTS, [])
    if CONF_COMPONENTS in config:
        devs = [
            i["id"]
            for i in get_device_entries(full_conf.get("light", []), light.LightState)
            + get_device_entries(full_conf.get("switch", []), switch.Switch)
            + get_device_entries(full_conf.get("sensor", []), Sensor)
            + get_device_entries(full_conf.get("binary_sensor", []), BinarySensor)
        ]

        add_devices = []

        if isinstance(config[CONF_COMPONENTS], list):
            list_devs = []
            for dev in config[CONF_COMPONENTS]:
                if dev in devs:
                    list_devs.append(dev)
            add_devices = [
                i
                for i in get_device_entries(
                    full_conf.get("light", []), light.LightState
                )
                + get_device_entries(full_conf.get("switch", []), switch.Switch)
                + get_device_entries(full_conf.get("sensor", []), Sensor)
                + get_device_entries(full_conf.get("binary_sensor", []), BinarySensor)
                if i["id"] in list_devs
            ]
        if config[CONF_COMPONENTS] == "all":
            add_devices = [
                i
                for i in get_device_entries(
                    full_conf.get("light", []), light.LightState
                )
                + get_device_entries(full_conf.get("switch", []), switch.Switch)
                + get_device_entries(full_conf.get("sensor", []), Sensor)
                + get_device_entries(full_conf.get("binary_sensor", []), BinarySensor)
                if ("name" in i) and not i.get("internal")
            ]
        for dev in add_devices:
            ep_list.append(create_device_ep(eps, dev, config[CONF_AS_GENERIC]))
    if not ep_list:
        ep_list = [
            {
                CONF_DEVICE_TYPE: "CUSTOM_ATTR",
                CONF_NUM: 1,
            }
        ]
    return ep_list, len(CORE.component_ids) - comp_ids
