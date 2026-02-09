#include "zigbee.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "zigbee_attribute.h"
#include "esphome/core/log.h"
#include "zigbee_helpers.h"
#ifdef CONFIG_WIFI_COEX
#include "esp_coexist.h"
#endif

#if !(defined ZB_ED_ROLE || defined ZB_ROUTER_ROLE)
#error Define ZB_ED_ROLE or ZB_ROUTER_ROLE in idf.py menuconfig.
#endif

namespace esphome {
namespace zigbee {

ZigBeeComponent *global_zigbee;

device_params_t coord;

/**
 * Creates a ZCL string from the given input string.
 *
 * @param str          Pointer to the input null-terminated C-style string.
 * @param max_size     Maximum allowable size for the resulting ZCL string. Maximum value: 254.
 * @param use_max_size Optional. If true, the `max_size` is used directly,
 *                     overriding the actual size of the input string.
 * @return             Pointer to a dynamically allocated ZCL string.
 *                     NOTE: Caller is responsible for freeing the allocated memory with `delete[]`.
 *
 */
uint8_t *get_zcl_string(const char *str, uint8_t max_size, bool use_max_size) {
  uint8_t str_len = static_cast<uint8_t>(strlen(str));
  uint8_t zcl_str_size = use_max_size ? max_size : std::min(max_size, str_len);

  uint8_t *zcl_str = new uint8_t[zcl_str_size + 1];  // string + length octet
  zcl_str[0] = zcl_str_size;
  memcpy(zcl_str + 1, str, str_len);

  return zcl_str;
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask) {
  if (esp_zb_bdb_start_top_level_commissioning(mode_mask) != ESP_OK) {
    ESP_LOGE(TAG, "Start network steering failed!");
  }
}

void ZigBeeComponent::report() {
  for (const auto &[_, attribute] : this->attributes_) {
    attribute->report();
  }
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
      ESP_LOGD(TAG, "Zigbee stack initialized");
      esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
      break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
      // Device started for the first time after the NVRAM erase
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
      // Device started using the NVRAM contents.
      if (err_status == ESP_OK) {
        ESP_LOGD(TAG, "Device started up in %sfactory-reset mode", esp_zb_bdb_is_factory_new() ? "" : "non ");
        global_zigbee->started_ = true;
        if (esp_zb_bdb_is_factory_new()) {
          ESP_LOGD(TAG, "Start network steering");
          esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
          ESP_LOGD(TAG, "Device rebooted");
          global_zigbee->searchBindings();
        }
      } else {
        ESP_LOGE(TAG, "FIRST_START.  Device started up in %sfactory-reset mode with an error %d (%s)",
                 esp_zb_bdb_is_factory_new() ? "" : "non ", err_status, esp_err_to_name(err_status));
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
        global_zigbee->joined_ = true;
        global_zigbee->enable_loop_soon_any_context();
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
        ESP_LOGD(TAG, "Reset device");
        esp_zb_factory_reset();
      } else {
        ESP_LOGD(TAG, "Leave_type: %u", leave_params->leave_type);
      }
      break;
    default:
      ESP_LOGD(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
               esp_err_to_name(err_status));
      break;
  }
}

// Recall bounded devices from the binding table after reboot
void ZigBeeComponent::bindingTableCb(const esp_zb_zdo_binding_table_info_t *table_info, void *user_ctx) {
  bool done = true;
  esp_zb_zdo_mgmt_bind_param_t *req = (esp_zb_zdo_mgmt_bind_param_t *) user_ctx;
  esp_zb_zdp_status_t zdo_status = (esp_zb_zdp_status_t) table_info->status;
  ESP_LOGD(TAG, "Binding table callback for address 0x%04x with status %d", req->dst_addr, zdo_status);
  if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
    // Print binding table log simple
    ESP_LOGD(TAG, "Binding table info: total %d, index %d, count %d", table_info->total, table_info->index,
             table_info->count);

    if (table_info->total == 0) {
      ESP_LOGD(TAG, "No binding table entries found");
      free(req);
      global_zigbee->connected_ = true;
      return;
    }

    esp_zb_zdo_binding_table_record_t *record = table_info->record;
    for (int i = 0; i < table_info->count; i++) {
      ESP_LOGD(TAG, "Binding table record: src_endp %d, dst_endp %d, cluster_id 0x%04x, dst_addr_mode %d",
               record->src_endp, record->dst_endp, record->cluster_id, record->dst_addr_mode);

      zb_device_params_t *device = (zb_device_params_t *) calloc(1, sizeof(zb_device_params_t));
      device->endpoint = record->dst_endp;
      if (record->dst_addr_mode == ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT ||
          record->dst_addr_mode == ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT) {
        device->short_addr = record->dst_address.addr_short;
      } else {  // ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT
        memcpy(device->ieee_addr, record->dst_address.addr_long, sizeof(esp_zb_ieee_addr_t));
      }
      ESP_LOGD(TAG,
               "Device bound to EP %d -> device endpoint: %d, short addr: 0x%04x, ieee addr: "
               "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
               record->src_endp, device->endpoint, device->short_addr, device->ieee_addr[7], device->ieee_addr[6],
               device->ieee_addr[5], device->ieee_addr[4], device->ieee_addr[3], device->ieee_addr[2],
               device->ieee_addr[1], device->ieee_addr[0]);
      record = record->next;
    }

    // Continue reading the binding table
    if (table_info->index + table_info->count < table_info->total) {
      /* There are unreported binding table entries, request for them. */
      req->start_index = table_info->index + table_info->count;
      esp_zb_zdo_binding_table_req(req, bindingTableCb, req);
      done = false;
    }
  }

  if (done) {
    // Print bound devices
    ESP_LOGD(TAG, "Filling bounded devices finished");
    free(req);
    global_zigbee->connected_ = true;
  }
}

void ZigBeeComponent::searchBindings() {
  esp_zb_zdo_mgmt_bind_param_t *mb_req = (esp_zb_zdo_mgmt_bind_param_t *) malloc(sizeof(esp_zb_zdo_mgmt_bind_param_t));
  mb_req->dst_addr = esp_zb_get_short_address();
  mb_req->start_index = 0;
  ESP_LOGD(TAG, "Requesting binding table for address 0x%04x", mb_req->dst_addr);
  esp_zb_zdo_binding_table_req(mb_req, bindingTableCb, (void *) mb_req);
}

void load_zb_event(ZBEvent *event, esp_zb_device_cb_common_info_t info, esp_zb_zcl_attribute_t attribute,
                   uint8_t *current_level) {
  event->load_set_attr_value_event(info, attribute, current_level);
}

void load_zb_event(ZBEvent *event, const esp_zb_zcl_report_attr_message_t *message) {
  event->load_report_attr_event(message);
}

void load_zb_event(ZBEvent *event, esp_zb_zcl_cmd_info_t info, esp_zb_zcl_read_attr_resp_variable_t *variables) {
  event->load_read_attr_resp_event(info, variables);
}

template<typename... Args> void enqueue_zb_event(Args... args) {
  // Allocate an event from the pool
  ZBEvent *event = global_zigbee->zb_event_pool_.allocate();
  if (event == nullptr) {
    // No events available - queue is full or we're out of memory
    global_zigbee->zb_events_.increment_dropped_count();
    return;
  }

  // Load new event data (replaces previous event)
  load_zb_event(event, args...);

  // Push the event to the queue
  global_zigbee->zb_events_.push(event);
  // Push always succeeds because we're the only producer and the pool ensures we never exceed queue size
  global_zigbee->enable_loop_soon_any_context();
}

// Explicit template instantiations for the friend function
template void enqueue_zb_event(esp_zb_device_cb_common_info_t info, esp_zb_zcl_attribute_t attribute,
                               uint8_t *current_level);
template void enqueue_zb_event(const esp_zb_zcl_report_attr_message_t *message);
template void enqueue_zb_event(esp_zb_zcl_cmd_info_t info, esp_zb_zcl_read_attr_resp_variable_t *variables);

static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message) {
  ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
  ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG,
                      "Received message: error status(%d)", message->info.status);
  ESP_LOGD(TAG, "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)",
           message->info.dst_endpoint, message->info.cluster, message->attribute.id, message->attribute.data.size);

  // if the attribute is On/Off and it is set to Off, restore the previous level
  esp_zb_zcl_attr_t *current_level = nullptr;
  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID &&
        message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL && !*(bool *) message->attribute.data.value) {
      ESP_LOGD(TAG, "turned off");
      current_level =
          esp_zb_zcl_get_attribute(message->info.dst_endpoint, ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
                                   ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID);
      if (current_level) {
        ESP_LOGD(TAG, "got level");
        esp_zb_zcl_set_attribute_val(message->info.dst_endpoint, ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
                                     ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID,
                                     current_level->data_p, false);
      }
    }
  }
  if (current_level != nullptr) {
    enqueue_zb_event(message->info, message->attribute, (uint8_t *) current_level->data_p);
  } else {
    enqueue_zb_event(message->info, message->attribute, nullptr);
  }
  return ESP_OK;
}

static esp_err_t zb_cmd_attribute_handler(const esp_zb_zcl_cmd_read_attr_resp_message_t *message) {
  ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
  ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG,
                      "Received message: error status(%d)", message->info.status);
  enqueue_zb_event(message->info, message->variables);
  return ESP_OK;
}

static esp_err_t zb_report_attribute_handler(const esp_zb_zcl_report_attr_message_t *message) {
  ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
  ESP_RETURN_ON_FALSE(message->status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG,
                      "Received message: error status(%d)", message->status);
  enqueue_zb_event(message);
  return ESP_OK;
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message) {
  esp_err_t ret = ESP_OK;
  switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
      ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *) message);
      break;
    case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID:
      ret = zb_cmd_attribute_handler((esp_zb_zcl_cmd_read_attr_resp_message_t *) message);
      break;
    case ESP_ZB_CORE_REPORT_ATTR_CB_ID:
      ret = zb_report_attribute_handler((esp_zb_zcl_report_attr_message_t *) message);
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

void ZigBeeComponent::handle_attribute(esp_zb_device_cb_common_info_t info, esp_zb_zcl_attribute_t attribute,
                                       uint8_t *current_level) {
  if (this->attributes_.find({info.dst_endpoint, info.cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, attribute.id}) !=
      this->attributes_.end()) {
    this->attributes_[{info.dst_endpoint, info.cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, attribute.id}]->on_value(
        attribute);
    // if the attribute is On/Off and it is set to Off, restore the previous level
    if (info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF && current_level != nullptr) {
      if (attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL &&
          !*(bool *) attribute.data.value) {
        ESP_LOGD(TAG, "turned off");
        if (this->attributes_.find({info.dst_endpoint, ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
                                    ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID}) !=
            this->attributes_.end()) {
          ESP_LOGD(TAG, "found level");
          esp_zb_zcl_attribute_t lvl_attr = {
              .id = ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID,
              .data =
                  {
                      .type = ESP_ZB_ZCL_ATTR_TYPE_U8,
                      .size = sizeof(uint8_t),
                      .value = current_level,
                  },
          };
          this->attributes_[{info.dst_endpoint, ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                             ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID}]
              ->on_value(lvl_attr);
          ESP_LOGD(TAG, "Light set to restore-level: %d", *current_level);
        }
      }
    }
  }
}

void ZigBeeComponent::handle_report_attribute(uint8_t dst_endpoint, uint16_t cluster, esp_zb_zcl_attribute_t attribute,
                                              esp_zb_zcl_addr_t src_address, uint8_t src_endpoint) {
  auto attr = this->attributes_.find({dst_endpoint, cluster, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE, attribute.id});
  if (attr == this->attributes_.end()) {
    ESP_LOGD(TAG, "No attributes configured for report (endpoint %d; cluster 0x%04x; attribute id 0x%04x)",
             dst_endpoint, cluster, attribute.id);
    return;
  }
  attr->second->on_report(attribute, src_address, src_endpoint);
}

void ZigBeeComponent::handle_read_attribute_response(esp_zb_zcl_cmd_info_t info,
                                                     esp_zb_zcl_read_attr_resp_variable_t *variables) {
  switch (info.cluster) {
    case ESP_ZB_ZCL_CLUSTER_ID_TIME:
      ESP_LOGD(TAG, "Recieved time information");
#ifdef USE_ZIGBEE_TIME
      if (this->zt_ == nullptr) {
        ESP_LOGD(TAG, "No time component linked to update time!");
      } else {
        this->zt_->recieve_timesync_response(variables);
      }
#else
      ESP_LOGD(TAG, "No zigbee time component included at build time!");
#endif
      break;
    default:
      ESP_LOGD(TAG, "Attribute data recieved (but not yet handled):");
      while (variables) {
        ESP_LOGD(TAG, "Read attribute response: status(%d), cluster(0x%x), attribute(0x%x), type(0x%x), value(%d)",
                 variables->status, info.cluster, variables->attribute.id, variables->attribute.data.type,
                 variables->attribute.data.value ? *(uint8_t *) variables->attribute.data.value : 0);
        variables = variables->next;
      }
  }
}

void ZigBeeComponent::create_default_cluster(uint8_t endpoint_id, esp_zb_ha_standard_devices_t device_id) {
  esp_zb_cluster_list_t *cluster_list = esphome_zb_default_clusters_create(device_id);
  this->endpoint_list_[endpoint_id] =
      std::tuple<esp_zb_ha_standard_devices_t, esp_zb_cluster_list_t *>(device_id, cluster_list);
  // Add basic cluster
  this->add_cluster(endpoint_id, ESP_ZB_ZCL_CLUSTER_ID_BASIC, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  // Add identify cluster if not already present
  if (esp_zb_cluster_list_get_cluster(cluster_list, ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE) ==
      nullptr) {
    this->add_cluster(endpoint_id, ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  }
}

void ZigBeeComponent::add_cluster(uint8_t endpoint_id, uint16_t cluster_id, uint8_t role) {
  esp_zb_attribute_list_t *attr_list;
  switch (cluster_id) {
    case 0:
      attr_list = create_basic_cluster_();
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
  ESP_LOGD(TAG, "Model: %s", this->basic_cluster_data_.model.c_str());
  ESP_LOGD(TAG, "Manufacturer: %s", this->basic_cluster_data_.manufacturer.c_str());
  ESP_LOGD(TAG, "Date: %s", this->basic_cluster_data_.date.c_str());
  ESP_LOGD(TAG, "Area: %s", this->basic_cluster_data_.area.c_str());
  uint8_t *ManufacturerName = get_zcl_string(this->basic_cluster_data_.manufacturer.c_str(),
                                             32);  // warning: this is in format {length, 'string'} :
  uint8_t *ModelIdentifier = get_zcl_string(this->basic_cluster_data_.model.c_str(), 32);
  uint8_t *DateCode = get_zcl_string(this->basic_cluster_data_.date.c_str(), 16);
  uint8_t *Location = get_zcl_string(this->basic_cluster_data_.area.c_str(), 16);
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
  delete[] ManufacturerName;
  delete[] ModelIdentifier;
  delete[] DateCode;
  delete[] Location;
  return attr_list;
}

esp_err_t ZigBeeComponent::create_endpoint(uint8_t endpoint_id, esp_zb_ha_standard_devices_t device_id,
                                           esp_zb_cluster_list_t *esp_zb_cluster_list) {
  // ------------------------------ Create endpoint list ------------------------------
  esp_zb_endpoint_config_t endpoint_config = {.endpoint = endpoint_id,
                                              .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
                                              .app_device_id = device_id,
                                              .app_device_version = 0};
  return esp_zb_ep_list_add_ep(this->esp_zb_ep_list_, esp_zb_cluster_list, endpoint_config);
}

static void esp_zb_task_(void *pvParameters) {
  if (esp_zb_start(false) != ESP_OK) {
    ESP_LOGE(TAG, "Could not setup Zigbee");
    // this->mark_failed();
    vTaskDelete(NULL);
  }

  if ((global_zigbee->device_role_ == ESP_ZB_DEVICE_TYPE_ED) && (global_zigbee->basic_cluster_data_.power == 0x03)) {
    ESP_LOGD(TAG, "Battery powered!");
    // zb_set_ed_node_descriptor(0, 1, 1);  // workaround, rx_on_when_idle should be 0 for battery powered devices.
    esp_zb_set_node_descriptor_power_source(0);
  } else {
    esp_zb_set_node_descriptor_power_source(1);
  }
  esp_zb_stack_main_loop();
}

void ZigBeeComponent::setup() {
  global_zigbee = this;
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

  /* initialize Zigbee stack */
  esp_zb_zed_cfg_t zb_zed_cfg = {
      .ed_timeout = ED_AGING_TIMEOUT,
      .keep_alive = ED_KEEP_ALIVE,
  };
  esp_zb_zczr_cfg_t zb_zczr_cfg = {
      .max_children = MAX_CHILDREN,
  };
  esp_zb_cfg_t zb_nwk_cfg = {
      .esp_zb_role = this->device_role_,
      .install_code_policy = INSTALLCODE_POLICY_ENABLE,
  };
#ifdef ZB_ROUTER_ROLE
  zb_nwk_cfg.nwk_cfg.zczr_cfg = zb_zczr_cfg;
#else
  zb_nwk_cfg.nwk_cfg.zed_cfg = zb_zed_cfg;
#endif
  esp_zb_init(&zb_nwk_cfg);

  esp_err_t ret;

  // clusters
  for (auto const &[key, val] : this->attribute_list_) {
    esp_zb_cluster_list_t *esp_zb_cluster_list = std::get<1>(this->endpoint_list_[std::get<0>(key)]);
    ret = esphome_zb_cluster_list_add_or_update_cluster(std::get<1>(key), esp_zb_cluster_list, val, std::get<2>(key));
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Could not create cluster 0x%04X with role %u: %s", std::get<1>(key), std::get<2>(key),
               esp_err_to_name(ret));
    }
  }
  this->attribute_list_.clear();

  // endpoints
  for (auto const &[ep_id, dev] : this->endpoint_list_) {
    // create_default_cluster(key, val);
    if (create_endpoint(ep_id, std::get<0>(dev), std::get<1>(dev)) != ESP_OK) {
      ESP_LOGE(TAG, "Could not create endpoint %u", ep_id);
    }
  }

  // ------------------------------ Register Device ------------------------------
  if (esp_zb_device_register(this->esp_zb_ep_list_) != ESP_OK) {
    ESP_LOGE(TAG, "Could not register the endpoint list");
    this->mark_failed();
    return;
  }

  esp_zb_core_action_handler_register(zb_action_handler);

  if (esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK) != ESP_OK) {
    ESP_LOGE(TAG, "Could not setup Zigbee");
    this->mark_failed();
    return;
  }

  // reporting
  for (auto &[_, attribute] : this->attributes_) {
    if (attribute->report_enabled) {
      esp_zb_zcl_reporting_info_t reporting_info = attribute->get_reporting_info();
      ESP_LOGD(TAG, "set reporting for cluster: %u", reporting_info.cluster_id);
      if (esp_zb_zcl_update_reporting_info(&reporting_info) != ESP_OK) {
        ESP_LOGE(TAG, "Could not configure reporting for attribute 0x%04X in cluster 0x%04X in endpoint %u",
                 reporting_info.attr_id, reporting_info.cluster_id, reporting_info.ep);
      }
    }
  }
  xTaskCreate(esp_zb_task_, "Zigbee_main", 4096, NULL, 24, NULL);
  this->disable_loop();  // loop is only needed for processing events, so disable until we join a network
}

void ZigBeeComponent::loop() {
  // Process all pending events
  ZBEvent *event = this->zb_events_.pop();
  while (event != nullptr) {
    // Handle the event
    switch (event->callback_id_) {
      case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        this->handle_attribute(
            event->event_.set_attr.info, event->event_.set_attr.attribute,
            event->event_.set_attr.has_current_level ? &event->event_.set_attr.current_level : nullptr);
        break;
      case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID:
        this->handle_read_attribute_response(event->event_.read_attr_resp.info,
                                             &(event->event_.read_attr_resp.variables));
        break;
      case ESP_ZB_CORE_REPORT_ATTR_CB_ID:
        this->handle_report_attribute(event->event_.report_attr.dst_endpoint, event->event_.report_attr.cluster,
                                      event->event_.report_attr.attribute, event->event_.report_attr.src_address,
                                      event->event_.report_attr.src_endpoint);

        break;
    }

    // Free the event back to the pool
    this->zb_event_pool_.release(event);
    // Get the next event
    event = this->zb_events_.pop();
  }
  // Log dropped events periodically
  uint16_t dropped = this->zb_events_.get_and_reset_dropped_count();
  if (dropped > 0) {
    ESP_LOGW(TAG, "Dropped %u Zigbee events due to buffer overflow", dropped);
  }

  if (this->joined_) {
    this->on_join_callback_.call();
    this->joined_ = false;  // only call once
    this->connected_ = true;
  } else if (this->connected_) {
    this->disable_loop();  // only disable once connected
  }
}

void ZigBeeComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "ZigBee:");
  for (auto const &[key, val] : this->endpoint_list_) {
    ESP_LOGCONFIG(TAG, "Endpoint: %u, %d", key, std::get<0>(val));
  }
}

}  // namespace zigbee
}  // namespace esphome
