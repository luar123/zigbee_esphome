#include "ha/esp_zigbee_ha_standard.h"
#include "zigbee_helpers.h"

esp_zb_cluster_list_t *esphome_zb_default_clusters_create(esp_zb_ha_standard_devices_t device_type) {
  esp_zb_cluster_list_t *cluster_list;
  switch (device_type) {
    case ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID: {
      esp_zb_on_off_switch_cfg_t config = ESP_ZB_DEFAULT_ON_OFF_SWITCH_CONFIG();
      cluster_list = esp_zb_on_off_switch_clusters_create(&config);
      break;
    }
    case ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID: {
      esp_zb_on_off_light_cfg_t config = ESP_ZB_DEFAULT_ON_OFF_LIGHT_CONFIG();
      cluster_list = esp_zb_on_off_light_clusters_create(&config);
      break;
    }
    case ESP_ZB_HA_COLOR_DIMMER_SWITCH_DEVICE_ID: {
      esp_zb_color_dimmable_switch_cfg_t config = ESP_ZB_DEFAULT_COLOR_DIMMABLE_SWITCH_CONFIG();
      cluster_list = esp_zb_color_dimmable_switch_clusters_create(&config);
      break;
    }
    case ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID: {
      esp_zb_color_dimmable_light_cfg_t config = ESP_ZB_DEFAULT_COLOR_DIMMABLE_LIGHT_CONFIG();
      cluster_list = esp_zb_color_dimmable_light_clusters_create(&config);
      break;
    }
    case ESP_ZB_HA_MAINS_POWER_OUTLET_DEVICE_ID: {
      esp_zb_mains_power_outlet_cfg_t config = ESP_ZB_DEFAULT_MAINS_POWER_OUTLET_CONFIG();
      cluster_list = esp_zb_mains_power_outlet_clusters_create(&config);
      break;
    }
    case ESP_ZB_HA_SHADE_DEVICE_ID: {
      esp_zb_shade_cfg_t config = ESP_ZB_DEFAULT_SHADE_CONFIG();
      cluster_list = esp_zb_shade_clusters_create(&config);
      break;
    }
    case ESP_ZB_HA_SHADE_CONTROLLER_DEVICE_ID: {
      esp_zb_shade_controller_cfg_t config = ESP_ZB_DEFAULT_SHADE_CONTROLLER_CONFIG();
      cluster_list = esp_zb_shade_controller_clusters_create(&config);
      break;
    }
    case ESP_ZB_HA_DOOR_LOCK_DEVICE_ID: {
      esp_zb_door_lock_cfg_t config = ESP_ZB_DEFAULT_DOOR_LOCK_CONFIG();
      cluster_list = esp_zb_door_lock_clusters_create(&config);
      break;
    }
    case ESP_ZB_HA_DOOR_LOCK_CONTROLLER_DEVICE_ID: {
      esp_zb_door_lock_controller_cfg_t config = ESP_ZB_DEFAULT_DOOR_LOCK_CONTROLLER_CONFIG();
      cluster_list = esp_zb_door_lock_controller_clusters_create(&config);
      break;
    }
    case ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID: {
      esp_zb_temperature_sensor_cfg_t config = ESP_ZB_DEFAULT_TEMPERATURE_SENSOR_CONFIG();
      cluster_list = esp_zb_temperature_sensor_clusters_create(&config);
      break;
    }
    case ESP_ZB_HA_CONFIGURATION_TOOL_DEVICE_ID: {
      esp_zb_configuration_tool_cfg_t config = ESP_ZB_DEFAULT_CONFIGURATION_TOOL_CONFIG();
      cluster_list = esp_zb_configuration_tool_clusters_create(&config);
      break;
    }
    case ESP_ZB_HA_THERMOSTAT_DEVICE_ID: {
      esp_zb_thermostat_cfg_t config = ESP_ZB_DEFAULT_THERMOSTAT_CONFIG();
      cluster_list = esp_zb_thermostat_clusters_create(&config);
      break;
    }
    case ESP_ZB_HA_WINDOW_COVERING_DEVICE_ID: {
      esp_zb_window_covering_cfg_t config = ESP_ZB_DEFAULT_WINDOW_COVERING_CONFIG();
      cluster_list = esp_zb_window_covering_clusters_create(&config);
      break;
    }
    case ESP_ZB_HA_WINDOW_COVERING_CONTROLLER_DEVICE_ID: {
      esp_zb_window_covering_controller_cfg_t config = ESP_ZB_DEFAULT_WINDOW_COVERING_CONTROLLER_CONFIG();
      cluster_list = esp_zb_window_covering_controller_clusters_create(&config);
      break;
    }
    default:
      // create empty cluster list;
      cluster_list = esp_zb_zcl_cluster_list_create();
  }

  return cluster_list;
}

esp_err_t esphome_zb_cluster_add_or_update_attr(uint16_t cluster_id, esp_zb_attribute_list_t *attr_list,
                                                uint16_t attr_id, uint8_t attr_type, uint8_t attr_access,
                                                void *value_p) {
  esp_err_t ret;
  ret = esp_zb_cluster_update_attr(attr_list, attr_id, value_p);
  if (ret != ESP_OK) {
    ESP_LOGE("zigbee_helper", "Ignore previous attribute not found error");
    if (attr_access > 0) {
      ret = esp_zb_cluster_add_attr(attr_list, cluster_id, attr_id, attr_type, attr_access, value_p);
    } else {
      ret = esphome_zb_cluster_add_attr(cluster_id, attr_list, attr_id, value_p);
    }
  }
  return ret;
}

esp_err_t esphome_zb_cluster_list_add_or_update_cluster(uint16_t cluster_id, esp_zb_cluster_list_t *cluster_list,
                                                        esp_zb_attribute_list_t *attr_list, uint8_t role_mask) {
  esp_err_t ret;
  ret = esp_zb_cluster_list_update_cluster(cluster_list, attr_list, cluster_id, role_mask);
  if (ret != ESP_OK) {
    ESP_LOGE("zigbee_helper", "Ignore previous cluster not found error");
    switch (cluster_id) {
      case ESP_ZB_ZCL_CLUSTER_ID_BASIC:
        ret = esp_zb_cluster_list_add_basic_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG:
        ret = esp_zb_cluster_list_add_power_config_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY:
        ret = esp_zb_cluster_list_add_identify_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_GROUPS:
        ret = esp_zb_cluster_list_add_groups_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_SCENES:
        ret = esp_zb_cluster_list_add_scenes_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_ON_OFF:
        ret = esp_zb_cluster_list_add_on_off_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_ON_OFF_SWITCH_CONFIG:
        ret = esp_zb_cluster_list_add_on_off_switch_config_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL:
        ret = esp_zb_cluster_list_add_level_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_TIME:
        ret = esp_zb_cluster_list_add_time_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT:
        ret = esp_zb_cluster_list_add_analog_input_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT:
        ret = esp_zb_cluster_list_add_analog_output_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_ANALOG_VALUE:
        ret = esp_zb_cluster_list_add_analog_value_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT:
        ret = esp_zb_cluster_list_add_binary_input_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_MULTI_VALUE:
        ret = esp_zb_cluster_list_add_multistate_value_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_COMMISSIONING:
        ret = esp_zb_cluster_list_add_commissioning_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_OTA_UPGRADE:
        ret = esp_zb_cluster_list_add_ota_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_SHADE_CONFIG:
        ret = esp_zb_cluster_list_add_shade_config_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_DOOR_LOCK:
        ret = esp_zb_cluster_list_add_door_lock_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING:
        ret = esp_zb_cluster_list_add_window_covering_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT:
        ret = esp_zb_cluster_list_add_thermostat_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_FAN_CONTROL:
        ret = esp_zb_cluster_list_add_fan_control_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_DEHUMIDIFICATION_CONTROL:
        ret = esp_zb_cluster_list_add_dehumidification_control_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT_UI_CONFIG:
        ret = esp_zb_cluster_list_add_thermostat_ui_config_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL:
        ret = esp_zb_cluster_list_add_color_control_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT:
        ret = esp_zb_cluster_list_add_illuminance_meas_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT:
        ret = esp_zb_cluster_list_add_temperature_meas_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT:
        ret = esp_zb_cluster_list_add_pressure_meas_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT:
        ret = esp_zb_cluster_list_add_flow_meas_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT:
        ret = esp_zb_cluster_list_add_humidity_meas_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING:
        ret = esp_zb_cluster_list_add_occupancy_sensing_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_PH_MEASUREMENT:
        ret = esp_zb_cluster_list_add_ph_measurement_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_EC_MEASUREMENT:
        ret = esp_zb_cluster_list_add_ec_measurement_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_WIND_SPEED_MEASUREMENT:
        ret = esp_zb_cluster_list_add_wind_speed_measurement_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT:
        ret = esp_zb_cluster_list_add_carbon_dioxide_measurement_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_PM2_5_MEASUREMENT:
        ret = esp_zb_cluster_list_add_pm2_5_measurement_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE:
        ret = esp_zb_cluster_list_add_ias_zone_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_IAS_ACE:
        ret = esp_zb_cluster_list_add_ias_ace_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_IAS_WD:
        ret = esp_zb_cluster_list_add_ias_wd_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_PRICE:
        ret = esp_zb_cluster_list_add_price_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_DRLC:
        ret = esp_zb_cluster_list_add_drlc_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_METERING:
        ret = esp_zb_cluster_list_add_metering_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_METER_IDENTIFICATION:
        ret = esp_zb_cluster_list_add_meter_identification_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT:
        ret = esp_zb_cluster_list_add_electrical_meas_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_DIAGNOSTICS:
        ret = esp_zb_cluster_list_add_diagnostics_cluster(cluster_list, attr_list, role_mask);
        break;
      default:
        ret = esp_zb_cluster_list_add_custom_cluster(cluster_list, attr_list, role_mask);
    }
  }
  return ret;
}

esp_zb_attribute_list_t *esphome_zb_default_attr_list_create(uint16_t cluster_id) {
  switch (cluster_id) {
    case ESP_ZB_ZCL_CLUSTER_ID_BASIC:
      return esp_zb_basic_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG:
      return esp_zb_power_config_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY:
      return esp_zb_identify_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_GROUPS:
      return esp_zb_groups_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_SCENES:
      return esp_zb_scenes_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_ON_OFF:
      return esp_zb_on_off_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_ON_OFF_SWITCH_CONFIG:
      return esp_zb_on_off_switch_config_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL:
      return esp_zb_level_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_TIME:
      return esp_zb_time_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT:
      return esp_zb_analog_input_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT:
      return esp_zb_analog_output_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_ANALOG_VALUE:
      return esp_zb_analog_value_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT:
      return esp_zb_binary_input_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_MULTI_VALUE:
      return esp_zb_multistate_value_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_COMMISSIONING:
      return esp_zb_commissioning_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_OTA_UPGRADE:
      return esp_zb_ota_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_SHADE_CONFIG:
      return esp_zb_shade_config_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_DOOR_LOCK:
      return esp_zb_door_lock_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING:
      return esp_zb_window_covering_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT:
      return esp_zb_thermostat_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_FAN_CONTROL:
      return esp_zb_fan_control_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_DEHUMIDIFICATION_CONTROL:
      return esp_zb_dehumidification_control_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT_UI_CONFIG:
      return esp_zb_thermostat_ui_config_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL:
      return esp_zb_color_control_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT:
      return esp_zb_illuminance_meas_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT:
      return esp_zb_temperature_meas_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT:
      return esp_zb_pressure_meas_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT:
      return esp_zb_flow_meas_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT:
      return esp_zb_humidity_meas_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING:
      return esp_zb_occupancy_sensing_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_PH_MEASUREMENT:
      return esp_zb_ph_measurement_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_EC_MEASUREMENT:
      return esp_zb_ec_measurement_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_WIND_SPEED_MEASUREMENT:
      return esp_zb_wind_speed_measurement_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT:
      return esp_zb_carbon_dioxide_measurement_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_PM2_5_MEASUREMENT:
      return esp_zb_pm2_5_measurement_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE:
      return esp_zb_ias_zone_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_IAS_ACE:
      return esp_zb_ias_ace_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_IAS_WD:
      return esp_zb_ias_wd_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_PRICE:
      return esp_zb_price_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_DRLC:
      return esp_zb_drlc_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_METERING:
      return esp_zb_metering_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_METER_IDENTIFICATION:
      return esp_zb_meter_identification_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT:
      return esp_zb_electrical_meas_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_DIAGNOSTICS:
      return esp_zb_diagnostics_cluster_create(NULL);
    default:
      return esp_zb_zcl_attr_list_create(cluster_id);
  }
}

esp_err_t esphome_zb_cluster_add_attr(uint16_t cluster_id, esp_zb_attribute_list_t *attr_list, uint16_t attr_id,
                                      void *value_p) {
  switch (cluster_id) {
    case ESP_ZB_ZCL_CLUSTER_ID_BASIC:
      return esp_zb_basic_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG:
      return esp_zb_power_config_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY:
      return esp_zb_identify_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_GROUPS:
      return esp_zb_groups_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_SCENES:
      return esp_zb_scenes_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_ON_OFF:
      return esp_zb_on_off_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_ON_OFF_SWITCH_CONFIG:
      return esp_zb_on_off_switch_config_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL:
      return esp_zb_level_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_TIME:
      return esp_zb_time_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT:
      return esp_zb_analog_input_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT:
      return esp_zb_analog_output_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_ANALOG_VALUE:
      return esp_zb_analog_value_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT:
      return esp_zb_binary_input_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_MULTI_VALUE:
      return esp_zb_multistate_value_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_COMMISSIONING:
      return esp_zb_commissioning_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_OTA_UPGRADE:
      return esp_zb_ota_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_SHADE_CONFIG:
      return esp_zb_shade_config_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_DOOR_LOCK:
      return esp_zb_door_lock_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING:
      return esp_zb_window_covering_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT:
      return esp_zb_thermostat_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_FAN_CONTROL:
      return esp_zb_fan_control_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_DEHUMIDIFICATION_CONTROL:
      return esp_zb_dehumidification_control_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT_UI_CONFIG:
      return esp_zb_thermostat_ui_config_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL:
      return esp_zb_color_control_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT:
      return esp_zb_illuminance_meas_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT:
      return esp_zb_temperature_meas_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT:
      return esp_zb_pressure_meas_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT:
      return esp_zb_flow_meas_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT:
      return esp_zb_humidity_meas_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING:
      return esp_zb_occupancy_sensing_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_PH_MEASUREMENT:
      return esp_zb_ph_measurement_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_EC_MEASUREMENT:
      return esp_zb_ec_measurement_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_WIND_SPEED_MEASUREMENT:
      return esp_zb_wind_speed_measurement_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT:
      return esp_zb_carbon_dioxide_measurement_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_PM2_5_MEASUREMENT:
      return esp_zb_pm2_5_measurement_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE:
      return esp_zb_ias_zone_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_IAS_WD:
      return esp_zb_ias_wd_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_DRLC:
      return esp_zb_drlc_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_METER_IDENTIFICATION:
      return esp_zb_meter_identification_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT:
      return esp_zb_electrical_meas_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_DIAGNOSTICS:
      return esp_zb_diagnostics_cluster_add_attr(attr_list, attr_id, value_p);
    default:
      return ESP_FAIL;
  }
}
