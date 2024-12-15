#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "zigbee_attribute.h"
#include "zigbee.h"
#include "esphome/core/log.h"
#include "zigbee_helpers.h"
#ifdef CONFIG_WIFI_COEX
#include "esp_coexist.h"
#endif

#if !defined ZB_ED_ROLE
#error Define ZB_ED_ROLE in idf.py menuconfig to compile light (End Device) source code.
#endif

namespace esphome {
namespace zigbee {

ZigBeeComponent *zigbeeC;

device_params_t coord;

/********************* Define functions **************************/
uint8_t *get_character_string(std::string str) {
  uint8_t *cstr = new uint8_t[(str.size() + 2)];
  std::snprintf((char *) (cstr + 1), str.size() + 1, "%s", str.c_str());
  cstr[0] = str.size();

  return cstr;
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask) {
  if (esp_zb_bdb_start_top_level_commissioning(mode_mask) != ESP_OK) {
    ESP_LOGE(TAG, "Start network steering failed!");
  }
}

void ZigBeeComponent::set_report(uint8_t endpoint_id, uint16_t cluster_id, uint8_t role, uint16_t attr_id) {
  /* Config the reporting info  */
  esp_zb_zcl_reporting_info_t reporting_info = {
      .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
      .ep = endpoint_id,
      .cluster_id = cluster_id,
      .cluster_role = role,
      .attr_id = attr_id,
      .manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC,
  };

  reporting_info.dst.short_addr = 0;
  reporting_info.dst.endpoint = 1;
  reporting_info.dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  reporting_info.u.send_info.min_interval = 10;     /*!< Actual minimum reporting interval */
  reporting_info.u.send_info.max_interval = 0;      /*!< Actual maximum reporting interval */
  reporting_info.u.send_info.def_min_interval = 10; /*!< Default minimum reporting interval */
  reporting_info.u.send_info.def_max_interval = 0;  /*!< Default maximum reporting interval */
  reporting_info.u.send_info.delta.s16 = 0;         /*!< Actual reportable change */

  this->reporting_list.push_back(reporting_info);
}

void ZigBeeComponent::report() {
  esp_zb_zcl_report_attr_cmd_t cmd = {
      .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
  };
  cmd.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000;
  cmd.zcl_basic_cmd.dst_endpoint = 1;

  for (auto reporting_info : zigbeeC->reporting_list) {
    cmd.zcl_basic_cmd.src_endpoint = reporting_info.ep;
    cmd.clusterID = reporting_info.cluster_id;
    cmd.attributeID = reporting_info.attr_id;
    cmd.cluster_role = reporting_info.cluster_role;
    esp_zb_zcl_report_attr_cmd_req(&cmd);
  }
}

static void bind_cb(esp_zb_zdp_status_t zdo_status, void *user_ctx) {
  ESP_LOGD(TAG, "Bind response from address(0x%x), endpoint(%d) with status(%d)",
           ((zdo_info_user_ctx_t *) user_ctx)->short_addr, ((zdo_info_user_ctx_t *) user_ctx)->endpoint, zdo_status);
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct) {
  static uint8_t steering_retry_count = 0;
  uint32_t *p_sg_p = signal_struct->p_app_signal;
  esp_err_t err_status = signal_struct->esp_err_status;
  esp_zb_app_signal_type_t sig_type = (esp_zb_app_signal_type_t) *p_sg_p;
  esp_zb_zdo_signal_leave_params_t *leave_params = NULL;
  switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
      // Notifies the application that ZBOSS framework (scheduler, buffer pool, etc.) has started, but no
      // join/rejoin/formation/BDB initialization has been done yet.
      ESP_LOGI(TAG, "SKIP_STARTUP. Device started up in %sfactory-reset mode",
               esp_zb_bdb_is_factory_new() ? "" : "non ");
      ESP_LOGI(TAG, "Initialize Zigbee stack");
      esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
      break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
      // Device started for the first time after the NVRAM erase
      if (err_status == ESP_OK) {
        ESP_LOGI(TAG, "FIRST_START. Device started up in %sfactory-reset mode",
                 esp_zb_bdb_is_factory_new() ? "" : "non ");
        if (esp_zb_bdb_is_factory_new()) {
          ESP_LOGI(TAG, "Start network steering");
          esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        }
      } else {
        ESP_LOGE(TAG, "FIRST_START.  Device started up in %sfactory-reset mode with an error %d (%s)",
                 esp_zb_bdb_is_factory_new() ? "" : "non ", err_status, esp_err_to_name(err_status));
      }
      break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
      // Device started using the NVRAM contents.
      if (err_status == ESP_OK) {
        ESP_LOGI(TAG, "Start network steering after reboot");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
      } else {
        ESP_LOGE(TAG, "FIRST_START.  Device started up in %sfactory-reset mode with an error %d (%s)",
                 esp_zb_bdb_is_factory_new() ? "" : "non ", err_status, esp_err_to_name(err_status));
        /* commissioning failed */
        ESP_LOGW(TAG, "Failed to initialize Zigbee stack (status: %s)", esp_err_to_name(err_status));
        esp_zb_scheduler_alarm((esp_zb_callback_t) bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_INITIALIZATION,
                               1000);
      }
      break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
      // BDB network steering completed (Network steering only)
      if (err_status == ESP_OK) {
        steering_retry_count = 0;
        esp_zb_ieee_addr_t extended_pan_id;
        esp_zb_get_extended_pan_id(extended_pan_id);
        ESP_LOGI(TAG,
                 "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: "
                 "0x%04hx, Channel:%d)",
                 extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4], extended_pan_id[3],
                 extended_pan_id[2], extended_pan_id[1], extended_pan_id[0], esp_zb_get_pan_id(),
                 esp_zb_get_current_channel());
        zigbeeC->on_join_callback_.call();
        zigbeeC->connected = true;

        memcpy(&(coord.ieee_addr), extended_pan_id, sizeof(esp_zb_ieee_addr_t));
        coord.endpoint = 1;
        coord.short_addr = 0;
        /* bind the reporting clusters to ep */
        esp_zb_zdo_bind_req_param_t bind_req;
        memcpy(&(bind_req.dst_address_u.addr_long), coord.ieee_addr, sizeof(esp_zb_ieee_addr_t));
        bind_req.dst_endp = coord.endpoint;
        bind_req.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
        esp_zb_get_long_address(bind_req.src_address);
        bind_req.req_dst_addr = esp_zb_get_short_address();
        static zdo_info_user_ctx_t test_info_ctx;
        test_info_ctx.short_addr = coord.short_addr;
        for (auto reporting_info : zigbeeC->reporting_list) {
          bind_req.cluster_id = reporting_info.cluster_id;
          bind_req.src_endp = reporting_info.ep;
          test_info_ctx.endpoint = reporting_info.ep;
          esp_zb_zdo_device_bind_req(&bind_req, bind_cb, (void *) &(test_info_ctx));
        }
      } else {
        ESP_LOGI(TAG, "Network steering was not successful (status: %s)", esp_err_to_name(err_status));
        if (steering_retry_count < 10) {
          steering_retry_count++;
          esp_zb_scheduler_alarm((esp_zb_callback_t) bdb_start_top_level_commissioning_cb,
                                 ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        } else {
          esp_zb_scheduler_alarm((esp_zb_callback_t) bdb_start_top_level_commissioning_cb,
                                 ESP_ZB_BDB_MODE_NETWORK_STEERING, 600 * 1000);
        }
      }
      break;
    case ESP_ZB_ZDO_SIGNAL_LEAVE:
      leave_params = (esp_zb_zdo_signal_leave_params_t *) esp_zb_app_signal_get_params(p_sg_p);
      if (leave_params->leave_type == ESP_ZB_NWK_LEAVE_TYPE_RESET) {
        ESP_LOGI(TAG, "Reset device");
        esp_zb_factory_reset();
      } else {
        ESP_LOGI(TAG, "Leave_type: %u", leave_params->leave_type);
      }
      break;
    default:
      ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
               esp_err_to_name(err_status));
      break;
  }
}

static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message) {
  esp_err_t ret = ESP_OK;

  ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
  ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG,
                      "Received message: error status(%d)", message->info.status);
  ESP_LOGI(TAG, "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)",
           message->info.dst_endpoint, message->info.cluster, message->attribute.id, message->attribute.data.size);
  zigbeeC->handle_attribute(message->info, message->attribute);
  return ret;
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message) {
  esp_err_t ret = ESP_OK;
  switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
      ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *) message);
      break;
    case ESP_ZB_CORE_CMD_DEFAULT_RESP_CB_ID:
      ESP_LOGD(TAG, "Receive Zigbee default response callback");
      break;
    default:
      ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
      break;
  }
  return ret;
}

void ZigBeeComponent::handle_attribute(esp_zb_device_cb_common_info_t info, esp_zb_zcl_attribute_t attribute) {
  if (this->attributes_.find({info.dst_endpoint, info.cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, attribute.id}) !=
      this->attributes_.end()) {
    this->attributes_[{info.dst_endpoint, info.cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, attribute.id}]->on_value(
        attribute);
  }
}

void ZigBeeComponent::create_default_cluster(uint8_t endpoint_id, esp_zb_ha_standard_devices_t device_id) {
  this->cluster_list_[endpoint_id] = esphome_zb_default_clusters_create(device_id);
  this->endpoint_list_[endpoint_id] = device_id;
}

void ZigBeeComponent::add_cluster(uint8_t endpoint_id, uint16_t cluster_id, uint8_t role) {
  esp_zb_attribute_list_t *attr_list;
  switch (cluster_id) {
    case 0:
      attr_list = create_basic_cluster_();
      break;
    case 3:
      attr_list = create_ident_cluster_();
      break;
    default:
      attr_list = esphome_zb_default_attr_list_create(cluster_id);
  }
  this->attribute_list_[{endpoint_id, cluster_id, role}] = attr_list;
}

void ZigBeeComponent::set_basic_cluster(std::string model, std::string manufacturer, std::string date, uint8_t power,
                                        uint8_t app_version, uint8_t stack_version, uint8_t hw_version,
                                        std::string area, uint8_t physical_env) {
  this->basic_cluster_data_ = {
      .model = model,
      .manufacturer = manufacturer,
      .date = date,
      .power = power,
      .app_version = app_version,
      .stack_version = stack_version,
      .hw_version = hw_version,
      .area = area,
      .physical_env = physical_env,
  };
}

esp_zb_attribute_list_t *ZigBeeComponent::create_basic_cluster_() {
  // ------------------------------ Cluster BASIC ------------------------------
  esp_zb_basic_cluster_cfg_t basic_cluster_cfg = {
      .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
      .power_source = this->basic_cluster_data_.power,
  };
  ESP_LOGI(TAG, "Model: %s", this->basic_cluster_data_.model.c_str());
  ESP_LOGI(TAG, "Manufacturer: %s", this->basic_cluster_data_.manufacturer.c_str());
  ESP_LOGI(TAG, "Date: %s", this->basic_cluster_data_.date.c_str());
  ESP_LOGI(TAG, "Area: %s", this->basic_cluster_data_.area.c_str());
  uint8_t *ManufacturerName =
      get_character_string(this->basic_cluster_data_.manufacturer);  // warning: this is in format {length, 'string'} :
  uint8_t *ModelIdentifier = get_character_string(this->basic_cluster_data_.model);
  uint8_t *DateCode = get_character_string(this->basic_cluster_data_.date);
  uint8_t *Location = get_character_string(this->basic_cluster_data_.area);
  esp_zb_attribute_list_t *attr_list = esp_zb_basic_cluster_create(&basic_cluster_cfg);
  esp_zb_basic_cluster_add_attr(attr_list, ESP_ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID,
                                &(this->basic_cluster_data_.app_version));
  esp_zb_basic_cluster_add_attr(attr_list, ESP_ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID,
                                &(this->basic_cluster_data_.stack_version));
  esp_zb_basic_cluster_add_attr(attr_list, ESP_ZB_ZCL_ATTR_BASIC_HW_VERSION_ID,
                                &(this->basic_cluster_data_.hw_version));
  esp_zb_basic_cluster_add_attr(attr_list, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, ManufacturerName);
  esp_zb_basic_cluster_add_attr(attr_list, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, ModelIdentifier);
  esp_zb_basic_cluster_add_attr(attr_list, ESP_ZB_ZCL_ATTR_BASIC_DATE_CODE_ID, DateCode);
  esp_zb_basic_cluster_add_attr(attr_list, ESP_ZB_ZCL_ATTR_BASIC_LOCATION_DESCRIPTION_ID, Location);
  esp_zb_basic_cluster_add_attr(attr_list, ESP_ZB_ZCL_ATTR_BASIC_PHYSICAL_ENVIRONMENT_ID,
                                &(this->basic_cluster_data_.physical_env));
  return attr_list;
}

void ZigBeeComponent::set_ident_time(uint8_t ident_time) {
  // ------------------------------ Cluster IDENTIFY ------------------------------
  this->ident_time_ = ident_time;
}

esp_zb_attribute_list_t *ZigBeeComponent::create_ident_cluster_() {
  // ------------------------------ Cluster IDENTIFY ------------------------------
  esp_zb_identify_cluster_cfg_t identify_cluster_cfg = {
      .identify_time = this->ident_time_,
  };
  return esp_zb_identify_cluster_create(&identify_cluster_cfg);
}

esp_err_t ZigBeeComponent::create_endpoint(uint8_t endpoint_id, esp_zb_ha_standard_devices_t device_id) {
  esp_zb_cluster_list_t *esp_zb_cluster_list = this->cluster_list_[endpoint_id];
  // ------------------------------ Create endpoint list ------------------------------
  esp_zb_endpoint_config_t endpoint_config = {.endpoint = endpoint_id,
                                              .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
                                              .app_device_id = device_id,
                                              .app_device_version = 0};
  return esp_zb_ep_list_add_ep(this->esp_zb_ep_list_, esp_zb_cluster_list, endpoint_config);
}

void ZigBeeComponent::esp_zb_task_() {
  /* initialize Zigbee stack */
  esp_zb_zed_cfg_t zb_zed_cfg = {
      .ed_timeout = ED_AGING_TIMEOUT,
      .keep_alive = ED_KEEP_ALIVE,
  };
  esp_zb_cfg_t zb_nwk_cfg = {
      .esp_zb_role = this->device_role_,
      .install_code_policy = INSTALLCODE_POLICY_ENABLE,
  };
  zb_nwk_cfg.nwk_cfg.zed_cfg = zb_zed_cfg;
  esp_zb_init(&zb_nwk_cfg);
  esp_err_t ret;

  // clusters
  for (auto const &[key, val] : this->attribute_list_) {
    esp_zb_cluster_list_t *esp_zb_cluster_list = this->cluster_list_[std::get<0>(key)];
    ret = esphome_zb_cluster_list_add_or_update_cluster(std::get<1>(key), esp_zb_cluster_list, val, std::get<2>(key));
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Could not create cluster 0x%04X with role %u: %s", std::get<1>(key), std::get<2>(key),
               esp_err_to_name(ret));
    }
  }

  // endpoints
  for (auto const &[ep_id, dev_id] : this->endpoint_list_) {
    // create_default_cluster(key, val);
    if (create_endpoint(ep_id, dev_id) != ESP_OK) {
      ESP_LOGE(TAG, "Could not create endpoint %u", ep_id);
    }
  }

  // ------------------------------ Register Device ------------------------------
  if (esp_zb_device_register(this->esp_zb_ep_list_) != ESP_OK) {
    ESP_LOGE(TAG, "Could not register the endpoint list");
    this->mark_failed();
    vTaskDelete(NULL);
  }
  esp_zb_core_action_handler_register(zb_action_handler);

  // reporting
  for (auto reporting_info : this->reporting_list) {
    ESP_LOGI(TAG, "set reporting for cluster: %u", reporting_info.cluster_id);
    esp_zb_zcl_attr_location_info_t attr_info = {
        .endpoint_id = reporting_info.ep,
        .cluster_id = reporting_info.cluster_id,
        .cluster_role = reporting_info.cluster_role,
        .manuf_code = reporting_info.manuf_code,
        .attr_id = reporting_info.attr_id,
    };
    if (esp_zb_zcl_update_reporting_info(&reporting_info) != ESP_OK) {
      ESP_LOGE(TAG, "Could not configure reporting for attribute 0x%04X in cluster 0x%04X in endpoint %u",
               reporting_info.attr_id, reporting_info.cluster_id, reporting_info.ep);
    }
    // ESP_ERROR_CHECK(esp_zb_zcl_start_attr_reporting(attr_info));  // is this needed?
  }

  if (esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK) != ESP_OK) {
    ESP_LOGE(TAG, "Could not setup Zigbee");
    this->mark_failed();
    vTaskDelete(NULL);
  }
  if (esp_zb_start(false) != ESP_OK) {
    ESP_LOGE(TAG, "Could not setup Zigbee");
    this->mark_failed();
    vTaskDelete(NULL);
  }
  this->started_ = true;
  esp_zb_stack_main_loop();
}

void ZigBeeComponent::setup() {
  zigbeeC = this;
  esp_zb_platform_config_t config = {
      .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
      .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
  };
#ifdef CONFIG_WIFI_COEX
  if (esp_coex_wifi_i154_enable() != ESP_OK) {
    this->mark_failed();
    return;
  }
#endif
  // ESP_ERROR_CHECK(nvs_flash_init()); not needed, called by esp32 component
  if (esp_zb_platform_config(&config) != ESP_OK) {
    this->mark_failed();
    return;
  }
  xTaskCreate([](void *arg) { static_cast<ZigBeeComponent *>(arg)->esp_zb_task_(); }, "Zigbee_main", 4096, this, 24,
              NULL);
}

void ZigBeeComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "ZigBee:");
  for (auto const &[key, val] : this->endpoint_list_) {
    ESP_LOGCONFIG(TAG, "Endpoint: %u, %d", key, val);
  }
}

}  // namespace zigbee
}  // namespace esphome
