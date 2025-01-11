#!/usr/bin/env python3
"""
Create python enums
"""

from pycparser import c_ast, parse_file

filename = "esp_zigbee_zcl_common.h"

ast = parse_file(filename, use_cpp=True)

typedefs = [t[1].type for t in ast.children() if isinstance(t[1], c_ast.Typedef)]
my_enums = [t for t in typedefs if isinstance(t.type, c_ast.Enum)]


def print_enum(enum):
    print(enum.declname + ":")
    print(
        "\n".join([e.name + ": " + e.value.value for e in enum.type.values.enumerators])
    )


def write_profileIDs(enums):
    enum = list(filter(lambda e: e.declname == "esp_zb_ha_standard_devices_t", enums))[
        0
    ]
    enum_name = "ha_standard_devices"
    to_py = f'{enum_name} = cg.esphome_ns.enum("{enum.declname}")\nDEVICE_ID' + " = {\n"
    for e in enum.type.values.enumerators:
        to_py += f'    "{e.name.removeprefix("ESP_ZB_HA_").removesuffix("_DEVICE_ID")}": {enum_name}.{e.name},\n'
        to_py += f'    {(e.value.value.removesuffix("U").upper().replace("X","x"))}: {enum_name}.{e.name},\n'
    to_py += "}\n"
    return to_py


def write_clusterIDs(enums):
    enum = list(filter(lambda e: e.declname == "esp_zb_zcl_cluster_id_t", enums))[0]
    enum_name = "cluster_id"
    to_py = (
        f'{enum_name} = cg.esphome_ns.enum("{enum.declname}")\nCLUSTER_ID' + " = {\n"
    )
    for e in enum.type.values.enumerators:
        to_py += f'    "{e.name.removeprefix("ESP_ZB_ZCL_CLUSTER_ID_")}": {enum_name}.{e.name},\n'
        to_py += f'    {(e.value.value.removesuffix("U").upper().replace("X","x"))}: {enum_name}.{e.name},\n'
    to_py += "}\n"
    return to_py


def write_clusterRoles(enums):
    enum = list(filter(lambda e: e.declname == "esp_zb_zcl_cluster_role_t", enums))[0]
    enum_name = "cluster_role"
    to_py = (
        f'{enum_name} = cg.esphome_ns.enum("{enum.declname}")\nCLUSTER_ROLE' + " = {\n"
    )
    for e in enum.type.values.enumerators:
        to_py += f'    "{e.name.removeprefix("ESP_ZB_ZCL_CLUSTER_").removesuffix("_ROLE")}": {enum_name}.{e.name},\n'
    to_py += "}\n"
    return to_py


def write_ZBtypes(enums):
    enum = list(filter(lambda e: e.declname == "esp_zb_zcl_attr_type_t", enums))[0]
    enum_name = "attr_type"
    to_py = f'{enum_name} = cg.esphome_ns.enum("{enum.declname}")\nATTR_TYPE' + " = {\n"
    for e in enum.type.values.enumerators:
        to_py += f'    "{e.name.removeprefix("ESP_ZB_ZCL_ATTR_TYPE_")}": {enum_name}.{e.name},\n'
    to_py += "}\n"
    return to_py


with open("zigbee_const.py", "w", encoding="utf-8") as f:
    f.write("import esphome.codegen as cg\n\n")
    f.write(write_profileIDs(my_enums))
    f.write(write_clusterIDs(my_enums))
    f.write(write_clusterRoles(my_enums))
    f.write(write_ZBtypes(my_enums))
    f.write(
        """ATTR_ACCESS = {
    "READ_ONLY": 1,
    "WRITE_ONLY": 2,
    "READ_WRITE": 3,
}
"""
    )
# [print_enum(e) for e in enums]

replacements_cl = {
    "level_control": "level",
    "multi_value": "multistate_value",
    "ota_upgrade": "ota",
    "illuminance_measurement": "illuminance_meas",
    "temp_measurement": "temperature_meas",
    "pressure_measurement": "pressure_meas",
    "rel_humidity_measurement": "humidity_meas",
    "electrical_measurement": "electrical_meas",
    "flow_measurement": "flow_meas",
}

remove_cl = [
    "device_temp_config",
    "alarms",
    "rssi_location",
    "binary_output",
    "binary_value",
    "multi_input",
    "multi_output",
    "poll_control",
    "green_power",
    "keep_alive",
    "pump_config_control",
    "ballast_config",
]


def write_cluster_list_aou(enums):
    enum = list(filter(lambda e: e.declname == "esp_zb_zcl_cluster_id_t", enums))[0]
    ids = [e.name for e in enum.type.values.enumerators]
    to_py = "esp_err_t esphome_zb_cluster_list_add_or_update_cluster(uint16_t cluster_id, esp_zb_cluster_list_t *cluster_list, esp_zb_attribute_list_t *attr_list, uint8_t role_mask) {\n  esp_err_t ret;\n"
    to_py += "  ret = esp_zb_cluster_list_update_cluster(cluster_list, attr_list, cluster_id, role_mask);\n"
    to_py += "  if (ret != ESP_OK) {\n"
    to_py += (
        '    ESP_LOGE("zigbee_helper", "Ignore previous cluster not found error");\n'
    )
    to_py += "    switch (cluster_id) {\n"
    for id in ids:
        cluster_name = id.removeprefix("ESP_ZB_ZCL_CLUSTER_ID_").lower()
        if cluster_name in remove_cl:
            continue
        for k, i in replacements_cl.items():
            cluster_name = cluster_name.replace(k, i)
        to_py += f"      case {id}:\n"
        to_py += f"        ret = esp_zb_cluster_list_add_{cluster_name}_cluster(cluster_list, attr_list, role_mask);\n"
        to_py += "        break;\n"
    to_py += "      default:\n        ret = esp_zb_cluster_list_add_custom_cluster(cluster_list, attr_list, role_mask);\n    }\n  }\n  return ret;\n}\n\n"
    return to_py


replacements_attrl = {
    "level_control": "level",
    "multi_value": "multistate_value",
    "ota_upgrade": "ota",
    "illuminance_measurement": "illuminance_meas",
    "temp_measurement": "temperature_meas",
    "pressure_measurement": "pressure_meas",
    "rel_humidity_measurement": "humidity_meas",
    "electrical_measurement": "electrical_meas",
    "flow_measurement": "flow_meas",
}

void_list = []


def write_attr_list_create(enums):
    enum = list(filter(lambda e: e.declname == "esp_zb_zcl_cluster_id_t", enums))[0]
    ids = [e.name for e in enum.type.values.enumerators]
    to_py = "esp_zb_attribute_list_t *esphome_zb_default_attr_list_create(uint16_t cluster_id) {\n"
    to_py += "  switch (cluster_id) {\n"
    for id in ids:
        cluster_name = id.removeprefix("ESP_ZB_ZCL_CLUSTER_ID_").lower()
        if cluster_name in remove_cl:
            continue
        for k, i in replacements_attrl.items():
            cluster_name = cluster_name.replace(k, i)
        to_py += f"    case {id}:\n"
        if cluster_name in void_list:
            to_py += f"      return esp_zb_{cluster_name}_cluster_create();\n"
        else:
            to_py += f"      return esp_zb_{cluster_name}_cluster_create(NULL);\n"
    to_py += "    default:\n      return esp_zb_zcl_attr_list_create(cluster_id);\n  }\n}\n\n"
    return to_py


remove_attra = [
    "device_temp_config",
    "alarms",
    "rssi_location",
    "binary_output",
    "binary_value",
    "multi_input",
    "multi_output",
    "poll_control",
    "green_power",
    "keep_alive",
    "pump_config_control",
    "ballast_config",
    "ias_ace",
    "price",
    "metering",
]


def write_attr_add(enums):
    enum = list(filter(lambda e: e.declname == "esp_zb_zcl_cluster_id_t", enums))[0]
    ids = [e.name for e in enum.type.values.enumerators]
    to_py = "esp_err_t esphome_zb_cluster_add_attr(uint16_t cluster_id, esp_zb_attribute_list_t *attr_list, uint16_t attr_id, void *value_p) {\n"
    to_py += "  switch (cluster_id) {\n"
    for id in ids:
        cluster_name = id.removeprefix("ESP_ZB_ZCL_CLUSTER_ID_").lower()
        if cluster_name in remove_attra:
            continue
        for k, i in replacements_cl.items():
            cluster_name = cluster_name.replace(k, i)
        to_py += f"    case {id}:\n"
        to_py += f"      return esp_zb_{cluster_name}_cluster_add_attr(attr_list, attr_id, value_p);\n"
    to_py += "    default:\n      return ESP_FAIL;\n  }\n}\n\n"
    return to_py


with open("c_helpers.c", "w", encoding="utf-8") as f:
    f.write(write_cluster_list_aou(my_enums))
    f.write(write_attr_list_create(my_enums))
    f.write(write_attr_add(my_enums))
