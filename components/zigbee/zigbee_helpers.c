#include "ezbee/zha.h"
#include "zigbee_helpers.h"
#include "esp_log.h"

static const char *TAG = "zigbee_helpers";

ezb_err_t esphome_zb_cluster_add_or_update_attr(uint16_t cluster_id, ezb_zcl_cluster_desc_t cluster_desc,
                                                uint16_t attr_id, uint8_t attr_type, uint8_t attr_access,
                                                void *value_p) {
  ezb_zcl_attr_desc_t attr_desc = ezb_zcl_cluster_get_attr_desc(cluster_desc, attr_id, EZB_ZCL_STD_MANUF_CODE);
  if (attr_desc != NULL) {
    attr_access = ezb_zcl_attr_desc_get_access(attr_desc);
    ezb_zcl_free_attr_desc(attr_desc);
    // ESP_LOGI(TAG, "Attribute 0x%04X already exists in cluster 0x%04X, ignore", attr_id, cluster_id);
    // return EZB_ERR_NONE;
  }
  if (attr_access > 0) {
    attr_desc = ezb_zcl_create_attr_desc(attr_id, attr_type, attr_access, EZB_ZCL_STD_MANUF_CODE, value_p);
    return ezb_zcl_cluster_add_attr_desc(cluster_desc, attr_desc);
  } else {
    ESP_LOGI(TAG, "Attribute 0x%04X has no access flags, add", attr_id);
    return esphome_zb_cluster_add_attr(cluster_id, cluster_desc, attr_id, value_p);
  }
}

ezb_err_t esphome_zb_add_or_update_cluster(uint16_t cluster_id, ezb_af_ep_desc_t ep_desc,
                                           ezb_zcl_cluster_desc_t cluster_desc, uint8_t role_mask) {
  ezb_zcl_cluster_desc_t cl_desc = ezb_af_endpoint_get_cluster_desc(ep_desc, cluster_id, role_mask);
  if (cl_desc != NULL) {
    // return EZB_ERR_NONE;
    ezb_zcl_free_cluster_desc(cl_desc);
  }
  return ezb_af_endpoint_add_cluster_desc(ep_desc, cluster_desc);
}

ezb_af_ep_desc_t esphome_zb_zha_default_ep_desc_create(uint8_t ep_id, uint16_t device_id, uint8_t device_version) {
  ezb_af_ep_desc_t ep_desc;
  switch (device_id) {
    case EZB_ZHA_ON_OFF_SWITCH_DEVICE_ID: {
      const ezb_zha_on_off_switch_config_t config = EZB_ZHA_ON_OFF_SWITCH_CONFIG();
      ep_desc = ezb_zha_create_on_off_switch(ep_id, &config);
      break;
    }
    case EZB_ZHA_CONFIGURATION_TOOL_DEVICE_ID: {
      const ezb_zha_configuration_tool_config_t config = EZB_ZHA_CONFIGURATION_TOOL_CONFIG();
      ep_desc = ezb_zha_create_configuration_tool(ep_id, &config);
      break;
    }
    case EZB_ZHA_MAINS_POWER_OUTLET_DEVICE_ID: {
      const ezb_zha_mains_power_outlet_config_t config = EZB_ZHA_MAINS_POWER_OUTLET_CONFIG();
      ep_desc = ezb_zha_create_mains_power_outlet(ep_id, &config);
      break;
    }
    case EZB_ZHA_DOOR_LOCK_DEVICE_ID: {
      const ezb_zha_door_lock_config_t config = EZB_ZHA_DOOR_LOCK_CONFIG();
      ep_desc = ezb_zha_create_door_lock(ep_id, &config);
      break;
    }
    case EZB_ZHA_DOOR_LOCK_CONTROLLER_DEVICE_ID: {
      const ezb_zha_door_lock_controller_config_t config = EZB_ZHA_DOOR_LOCK_CONTROLLER_CONFIG();
      ep_desc = ezb_zha_create_door_lock_controller(ep_id, &config);
      break;
    }
    case EZB_ZHA_ON_OFF_LIGHT_DEVICE_ID: {
      const ezb_zha_on_off_light_config_t config = EZB_ZHA_ON_OFF_LIGHT_CONFIG();
      ep_desc = ezb_zha_create_on_off_light(ep_id, &config);
      break;
    }
    case EZB_ZHA_DIMMABLE_LIGHT_DEVICE_ID: {
      const ezb_zha_dimmable_light_config_t config = EZB_ZHA_DIMMABLE_LIGHT_CONFIG();
      ep_desc = ezb_zha_create_dimmable_light(ep_id, &config);
      break;
    }
    case EZB_ZHA_COLOR_DIMMABLE_LIGHT_DEVICE_ID: {
      const ezb_zha_color_dimmable_light_config_t config = EZB_ZHA_COLOR_DIMMABLE_LIGHT_CONFIG();
      ep_desc = ezb_zha_create_color_dimmable_light(ep_id, &config);
      break;
    }
    case EZB_ZHA_DIMMER_SWITCH_DEVICE_ID: {
      const ezb_zha_dimmer_switch_config_t config = EZB_ZHA_DIMMER_SWITCH_CONFIG();
      ep_desc = ezb_zha_create_dimmer_switch(ep_id, &config);
      break;
    }
    case EZB_ZHA_COLOR_DIMMER_SWITCH_DEVICE_ID: {
      const ezb_zha_color_dimmer_switch_config_t config = EZB_ZHA_COLOR_DIMMER_SWITCH_CONFIG();
      ep_desc = ezb_zha_create_color_dimmer_switch(ep_id, &config);
      break;
    }
    case EZB_ZHA_LIGHT_SENSOR_DEVICE_ID: {
      const ezb_zha_light_sensor_config_t config = EZB_ZHA_LIGHT_SENSOR_CONFIG();
      ep_desc = ezb_zha_create_light_sensor(ep_id, &config);
      break;
    }
    case EZB_ZHA_SHADE_DEVICE_ID: {
      const ezb_zha_shade_config_t config = EZB_ZHA_SHADE_CONFIG();
      ep_desc = ezb_zha_create_shade(ep_id, &config);
      break;
    }
    case EZB_ZHA_SHADE_CONTROLLER_DEVICE_ID: {
      const ezb_zha_shade_controller_config_t config = EZB_ZHA_SHADE_CONTROLLER_CONFIG();
      ep_desc = ezb_zha_create_shade_controller(ep_id, &config);
      break;
    }
    case EZB_ZHA_WINDOW_COVERING_DEVICE_ID: {
      const ezb_zha_window_covering_config_t config = EZB_ZHA_WINDOW_COVERING_CONFIG();
      ep_desc = ezb_zha_create_window_covering(ep_id, &config);
      break;
    }
    case EZB_ZHA_WINDOW_COVERING_CONTROLLER_DEVICE_ID: {
      const ezb_zha_window_covering_controller_config_t config = EZB_ZHA_WINDOW_COVERING_CONTROLLER_CONFIG();
      ep_desc = ezb_zha_create_window_covering_controller(ep_id, &config);
      break;
    }
    case EZB_ZHA_THERMOSTAT_DEVICE_ID: {
      const ezb_zha_thermostat_config_t config = EZB_ZHA_THERMOSTAT_CONFIG();
      ep_desc = ezb_zha_create_thermostat(ep_id, &config);
      break;
    }
    case EZB_ZHA_TEMPERATURE_SENSOR_DEVICE_ID: {
      const ezb_zha_temperature_sensor_config_t config = EZB_ZHA_TEMPERATURE_SENSOR_CONFIG();
      ep_desc = ezb_zha_create_temperature_sensor(ep_id, &config);
      break;
    }
    case EZB_ZHA_CUSTOM_GATEWAY_DEVICE_ID: {
      const ezb_zha_custom_gateway_config_t config = EZB_ZHA_CUSTOM_GATEWAY_CONFIG();
      ep_desc = ezb_zha_create_custom_gateway(ep_id, &config);
      break;
    }
    default:
      ezb_af_ep_config_t config = {
          .ep_id = ep_id,
          .app_profile_id = EZB_AF_HA_PROFILE_ID,
          .app_device_id = device_id,
          .app_device_version = device_version,
      };
      ep_desc = ezb_af_create_endpoint_desc(&config);
  }
  return ep_desc;
}

ezb_zcl_cluster_desc_t esphome_zb_default_cluster_dscr_create(uint16_t cluster_id, uint8_t role_mask) {
  switch (cluster_id) {
    case EZB_ZCL_CLUSTER_ID_BASIC:
      return ezb_zcl_basic_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_POWER_CONFIG:
      return ezb_zcl_power_config_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_DEVICE_TEMP_CONFIG:
      return ezb_zcl_device_temp_config_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_IDENTIFY:
      return ezb_zcl_identify_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_GROUPS:
      return ezb_zcl_groups_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_SCENES:
      return ezb_zcl_scenes_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_ON_OFF:
      return ezb_zcl_on_off_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_ON_OFF_SWITCH_CONFIG:
      return ezb_zcl_on_off_switch_config_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_LEVEL:
      return ezb_zcl_level_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_ALARMS:
      return ezb_zcl_alarms_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_TIME:
      return ezb_zcl_time_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_ANALOG_INPUT:
      return ezb_zcl_analog_input_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT:
      return ezb_zcl_analog_output_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_ANALOG_VALUE:
      return ezb_zcl_analog_value_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_BINARY_INPUT:
      return ezb_zcl_binary_input_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_BINARY_OUTPUT:
      return ezb_zcl_binary_output_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_BINARY_VALUE:
      return ezb_zcl_binary_value_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_MULTISTATE_INPUT:
      return ezb_zcl_multistate_input_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_MULTISTATE_OUTPUT:
      return ezb_zcl_multistate_output_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_MULTISTATE_VALUE:
      return ezb_zcl_multistate_value_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_OTA_UPGRADE:
      return ezb_zcl_ota_upgrade_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_POLL_CONTROL:
      return ezb_zcl_poll_control_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_SHADE_CONFIG:
      return ezb_zcl_shade_config_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_DOOR_LOCK:
      return ezb_zcl_door_lock_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_WINDOW_COVERING:
      return ezb_zcl_window_covering_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_THERMOSTAT:
      return ezb_zcl_thermostat_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_FAN_CONTROL:
      return ezb_zcl_fan_control_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_DEHUMIDIFICATION_CONTROL:
      return ezb_zcl_dehumidification_control_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_THERMOSTAT_UI_CONFIG:
      return ezb_zcl_thermostat_ui_config_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_COLOR_CONTROL:
      return ezb_zcl_color_control_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT:
      return ezb_zcl_illuminance_measurement_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT:
      return ezb_zcl_temperature_measurement_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT:
      return ezb_zcl_pressure_measurement_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT:
      return ezb_zcl_flow_measurement_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT:
      return ezb_zcl_rel_humidity_measurement_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING:
      return ezb_zcl_occupancy_sensing_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_PH_MEASUREMENT:
      return ezb_zcl_ph_measurement_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_EC_MEASUREMENT:
      return ezb_zcl_ec_measurement_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_WIND_SPEED_MEASUREMENT:
      return ezb_zcl_wind_speed_measurement_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT:
      return ezb_zcl_carbon_dioxide_measurement_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_PM2_5_MEASUREMENT:
      return ezb_zcl_pm2_5_measurement_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_IAS_ZONE:
      return ezb_zcl_ias_zone_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_IAS_ACE:
      return ezb_zcl_ias_ace_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_IAS_WD:
      return ezb_zcl_ias_wd_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_PRICE:
      return ezb_zcl_price_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_METERING:
      return ezb_zcl_metering_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_METER_IDENTIFICATION:
      return ezb_zcl_meter_identification_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT:
      return ezb_zcl_electrical_measurement_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_TOUCHLINK_COMMISSIONING:
      return ezb_zcl_touchlink_commissioning_create_cluster_desc(NULL, role_mask);
    default:
      ezb_zcl_custom_cluster_config_t config = {0};
      config.cluster_id = cluster_id;
      return ezb_zcl_custom_create_cluster_desc(&config, role_mask);
  }
}

ezb_err_t esphome_zb_cluster_add_attr(uint16_t cluster_id, ezb_zcl_cluster_desc_t cluster_desc, uint16_t attr_id,
                                      void *value_p) {
  switch (cluster_id) {
    case EZB_ZCL_CLUSTER_ID_BASIC:
      return ezb_zcl_basic_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_POWER_CONFIG:
      return ezb_zcl_power_config_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_DEVICE_TEMP_CONFIG:
      return ezb_zcl_device_temp_config_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_IDENTIFY:
      return ezb_zcl_identify_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_GROUPS:
      return ezb_zcl_groups_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_SCENES:
      return ezb_zcl_scenes_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_ON_OFF:
      return ezb_zcl_on_off_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_ON_OFF_SWITCH_CONFIG:
      return ezb_zcl_on_off_switch_config_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_LEVEL:
      return ezb_zcl_level_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_ALARMS:
      return ezb_zcl_alarms_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_TIME:
      return ezb_zcl_time_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_ANALOG_INPUT:
      return ezb_zcl_analog_input_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT:
      return ezb_zcl_analog_output_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_ANALOG_VALUE:
      return ezb_zcl_analog_value_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_BINARY_INPUT:
      return ezb_zcl_binary_input_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_BINARY_OUTPUT:
      return ezb_zcl_binary_output_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_BINARY_VALUE:
      return ezb_zcl_binary_value_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_MULTISTATE_INPUT:
      return ezb_zcl_multistate_input_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_MULTISTATE_OUTPUT:
      return ezb_zcl_multistate_output_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_MULTISTATE_VALUE:
      return ezb_zcl_multistate_value_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_OTA_UPGRADE:
      return ezb_zcl_ota_upgrade_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_POLL_CONTROL:
      return ezb_zcl_poll_control_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_SHADE_CONFIG:
      return ezb_zcl_shade_config_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_DOOR_LOCK:
      return ezb_zcl_door_lock_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_WINDOW_COVERING:
      return ezb_zcl_window_covering_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_THERMOSTAT:
      return ezb_zcl_thermostat_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_FAN_CONTROL:
      return ezb_zcl_fan_control_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_DEHUMIDIFICATION_CONTROL:
      return ezb_zcl_dehumidification_control_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_THERMOSTAT_UI_CONFIG:
      return ezb_zcl_thermostat_ui_config_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_COLOR_CONTROL:
      return ezb_zcl_color_control_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT:
      return ezb_zcl_illuminance_measurement_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT:
      return ezb_zcl_temperature_measurement_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT:
      return ezb_zcl_pressure_measurement_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT:
      return ezb_zcl_flow_measurement_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT:
      return ezb_zcl_rel_humidity_measurement_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING:
      return ezb_zcl_occupancy_sensing_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_PH_MEASUREMENT:
      return ezb_zcl_ph_measurement_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_EC_MEASUREMENT:
      return ezb_zcl_ec_measurement_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_WIND_SPEED_MEASUREMENT:
      return ezb_zcl_wind_speed_measurement_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT:
      return ezb_zcl_carbon_dioxide_measurement_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_PM2_5_MEASUREMENT:
      return ezb_zcl_pm2_5_measurement_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_IAS_ZONE:
      return ezb_zcl_ias_zone_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_IAS_ACE:
      return ezb_zcl_ias_ace_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_IAS_WD:
      return ezb_zcl_ias_wd_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_PRICE:
      return ezb_zcl_price_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_METERING:
      return ezb_zcl_metering_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_METER_IDENTIFICATION:
      return ezb_zcl_meter_identification_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT:
      return ezb_zcl_electrical_measurement_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_TOUCHLINK_COMMISSIONING:
      return ezb_zcl_touchlink_commissioning_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    default:
      return ESP_FAIL;
  }
}
