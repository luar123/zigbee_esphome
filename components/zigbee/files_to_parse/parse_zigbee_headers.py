#!/usr/bin/env python3
"""
Create python enums
"""

from pycparser import c_ast, parse_file

filename = "zcl_type.h"
zha_filename = "zha.h"

ast = parse_file(filename, use_cpp=True)
zha_ast = parse_file(zha_filename, use_cpp=True)

# typedefs = [t[1].type for t in ast.children() if isinstance(t[1], c_ast.Typedef)]
# my_enums = [t for t in typedefs if isinstance(t.type, c_ast.Enum)]
my_enums = [
    e[1].type
    for e in ast.children()
    if isinstance(e[1], c_ast.Decl) and isinstance(e[1].type, c_ast.Enum)
]
zha_enums = [
    e[1].type
    for e in zha_ast.children()
    if isinstance(e[1], c_ast.Decl) and isinstance(e[1].type, c_ast.Enum)
]


def print_enum(enum):
    print(enum.name + ":")
    print(
        "\n".join([e.name + ": " + e.value.value for e in enum.type.values.enumerators])
    )


def write_profileIDs(enums):
    enum = enums[0]
    enum_name = "ha_standard_devices"
    to_py = f'{enum_name} = cg.esphome_ns.enum("{enum.name}")\nDEVICE_ID' + " = {\n"
    for e in enum.values.enumerators:
        to_py += f'    "{e.name.removeprefix("EZB_ZHA_").removesuffix("_DEVICE_ID")}": {enum_name}.{e.name},\n'
        to_py += f'    {(e.value.value.removesuffix("U").upper().replace("X","x"))}: {enum_name}.{e.name},\n'
    to_py += "}\n"
    return to_py


def write_clusterIDs(enums):
    enum = list(filter(lambda e: e.name == "ezb_zcl_cluster_id_e", enums))[0]
    enum_name = "cluster_id"
    to_py = f'{enum_name} = cg.esphome_ns.enum("{enum.name}")\nCLUSTER_ID' + " = {\n"
    for e in enum.values.enumerators:
        to_py += f'    "{e.name.removeprefix("EZB_ZCL_CLUSTER_ID_")}": {enum_name}.{e.name},\n'
        to_py += f'    {(e.value.value.removesuffix("U").upper().replace("X","x"))}: {enum_name}.{e.name},\n'
    to_py += "}\n"
    return to_py


def write_clusterRoles(enums):
    to_py = "CLUSTER_ROLE" + " = {\n"
    to_py += '    "SERVER": cg.RawExpression("EZB_ZCL_CLUSTER_SERVER"),\n'
    to_py += '    "CLIENT": cg.RawExpression("EZB_ZCL_CLUSTER_CLIENT"),\n'
    to_py += "}\n"
    return to_py


def write_ZBtypes(enums):
    enum = list(filter(lambda e: e.name == "ezb_zcl_attr_type_e", enums))[0]
    enum_name = "attr_type"
    to_py = f'{enum_name} = cg.esphome_ns.enum("{enum.name}")\nATTR_TYPE' + " = {\n"
    for e in enum.values.enumerators:
        to_py += f'    "{e.name.removeprefix("EZB_ZCL_ATTR_TYPE_")}": {enum_name}.{e.name},\n'
    to_py += "}\n"
    return to_py


with open("zigbee_const.py", "w", encoding="utf-8") as f:
    # print_enum(my_enums[0])
    f.write("import esphome.codegen as cg\n\n")
    f.write(write_profileIDs(zha_enums))
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
    "multi_input": "multistate_input",
    "multi_output": "multistate_output",
    # "ota_upgrade": "ota",
    # "illuminance_measurement": "illuminance_meas",
    # "temp_measurement": "temperature_meas",
    # "pressure_measurement": "pressure_meas",
    # "rel_humidity_measurement": "humidity_meas",
    # "electrical_measurement": "electrical_meas",
    # "flow_measurement": "flow_meas",
}

remove_cl = [
    "rssi_location",
    "commissioning",
    # "ota",
    "green_power",
    "keep_alive",
    "pump_config_control",
    "ballast_config",
    "drlc",
    "tunnel",
    "diagnostics",
    # "ias_ace",
    # "price",
    # "metering",
]

remove_dev_ids = [
    "level_control_switch",
    "on_off_output",
    "level_controllable_output",
    "scene_selector",
    "remote_control",
    "combined_interface",
    "range_extender",
    "simple_sensor",
    "consumption_awareness",
    "home_gateway",
    "smart_plug",
    "white_goods",
    "meter_interface",
    "on_off_light_switch",
    "occupancy_sensor",
    "heating_cooling_unit",
    "pump",
    "pump_controller",
    "pressure_sensor",
    "flow_sensor",
    "mini_split_ac",
    "ias_control_indicating_equipment",
    "ias_ancillary_control_equipment",
    "ias_zone",
    "ias_warning",
]


def write_zha_default_clusters(enums):
    enum = enums[0]
    ids = [e.name for e in enum.values.enumerators]
    to_py = "ezb_af_ep_desc_t esphome_zb_zha_default_ep_desc_create(uint8_t ep_id, uint16_t device_id, uint8_t device_version, uint8_t power_source) {\n"
    to_py += "  ezb_af_ep_desc_t ep_desc; \n"
    to_py += "  switch (device_id) {\n"
    for id in ids:
        device_name = (
            id.removeprefix("EZB_ZHA_")
            .removesuffix("_ID")
            .removesuffix("_DEVICE")
            .lower()
        )
        if device_name in remove_dev_ids:
            continue
        to_py += f"    case {id}" + ": {\n"
        to_py += f"      ezb_zha_{device_name}_config_t config = EZB_ZHA_{device_name.upper()}_CONFIG();\n"
        to_py += "      config.basic_cfg.power_source = power_source;\n"
        to_py += f"      ep_desc = ezb_zha_create_{device_name}(ep_id, &config);\n"
        to_py += "      break;\n    }\n"
    to_py += "    default:\n      ezb_af_ep_config_t config = {\n          .ep_id = ep_id,\n          .app_profile_id = EZB_AF_HA_PROFILE_ID,\n          .app_device_id = device_id,\n          .app_device_version = device_version,\n      };\n"
    to_py += "      ep_desc = ezb_af_create_endpoint_desc(&config);\n  }\n  return ep_desc;\n}\n\n"
    return to_py


def write_cluster_list_aou(enums):
    enum = list(filter(lambda e: e.name == "ezb_zcl_cluster_id_e", enums))[0]
    ids = [e.name for e in enum.values.enumerators]
    to_py = "esp_err_t esphome_zb_cluster_list_add_or_update_cluster(uint16_t cluster_id, ezb_cluster_list_t *cluster_list, ezb_attribute_list_t *attr_list, uint8_t role_mask) {\n  esp_err_t ret;\n"
    to_py += "  ret = ezb_cluster_list_update_cluster(cluster_list, attr_list, cluster_id, role_mask);\n"
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
        to_py += f"        ret = ezb_cluster_list_add_{cluster_name}_cluster(cluster_list, attr_list, role_mask);\n"
        to_py += "        break;\n"
    to_py += "      default:\n        ret = ezb_cluster_list_add_custom_cluster(cluster_list, attr_list, role_mask);\n    }\n  }\n  return ret;\n}\n\n"
    return to_py


replacements_attrl = {
    "level_control": "level",
    "multi_value": "multistate_value",
    "multi_input": "multistate_input",
    "multi_output": "multistate_output",
    # "ota_upgrade": "ota",
    # "illuminance_measurement": "illuminance_meas",
    # "temp_measurement": "temperature_meas",
    # "pressure_measurement": "pressure_meas",
    # "rel_humidity_measurement": "humidity_meas",
    # "electrical_measurement": "electrical_meas",
    # "flow_measurement": "flow_meas",
}


def write_attr_list_create(enums):
    enum = list(filter(lambda e: e.name == "ezb_zcl_cluster_id_e", enums))[0]
    ids = [e.name for e in enum.values.enumerators]
    to_py = "ezb_zcl_cluster_desc_t esphome_zb_default_cluster_dscr_create(uint16_t cluster_id, uint8_t role_mask) {\n"
    to_py += "  switch (cluster_id) {\n"
    for id in ids:
        cluster_name = id.removeprefix("EZB_ZCL_CLUSTER_ID_").lower()
        if cluster_name in remove_cl:
            continue
        for k, i in replacements_attrl.items():
            cluster_name = cluster_name.replace(k, i)
        to_py += f"    case {id}:\n"
        to_py += f"      return ezb_zcl_{cluster_name}_create_cluster_desc(NULL, role_mask);\n"
    to_py += "    default:\n      ezb_zcl_custom_cluster_config_t config = {0};\n"
    to_py += "      config.cluster_id = cluster_id;\n"
    to_py += "      return ezb_zcl_custom_create_cluster_desc(&config, role_mask);\n  }\n}\n\n"
    return to_py


remove_attra = [
    "rssi_location",
    "commissioning",
    # "ota",
    "green_power",
    "keep_alive",
    "pump_config_control",
    "ballast_config",
    "drlc",
    "tunnel",
    "diagnostics",
    # "ias_ace",
    # "price",
    # "metering",
]


def write_attr_add(enums):
    enum = list(filter(lambda e: e.name == "ezb_zcl_cluster_id_e", enums))[0]
    ids = [e.name for e in enum.values.enumerators]
    to_py = "ezb_err_t esphome_zb_cluster_add_attr(uint16_t cluster_id, ezb_zcl_cluster_desc_t cluster_desc, uint16_t attr_id, void *value_p) {\n"
    to_py += "  switch (cluster_id) {\n"
    for id in ids:
        cluster_name = id.removeprefix("EZB_ZCL_CLUSTER_ID_").lower()
        if cluster_name in remove_attra:
            continue
        for k, i in replacements_cl.items():
            cluster_name = cluster_name.replace(k, i)
        to_py += f"    case {id}:\n"
        to_py += f"      return ezb_zcl_{cluster_name}_cluster_desc_add_attr(cluster_desc, attr_id, value_p);\n"
    to_py += "    default:\n      return ESP_FAIL;\n  }\n}\n\n"
    return to_py


with open("c_helpers.c", "w", encoding="utf-8") as f:
    # f.write(write_cluster_list_aou(my_enums))
    f.write(write_zha_default_clusters(zha_enums))
    f.write(write_attr_list_create(my_enums))
    f.write(write_attr_add(my_enums))
