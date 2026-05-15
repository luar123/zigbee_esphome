#include "zigbee.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "zigbee_attribute.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "zigbee_helpers.h"
#ifdef CONFIG_WIFI_COEX
#include "esp_coexist.h"
#endif

namespace esphome {
namespace zigbee {

ZigBeeComponent *global_zigbee;

zb_device_params_t coord;

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

void ZigBeeComponent::esp_zigbee_alarm_bdb_commissioning(ezb_bdb_comm_mode_mask_t mode) {
  if (!esp_zigbee_lock_acquire(10 / portTICK_PERIOD_MS)) {
    global_zigbee->set_timeout("zb_init", 10, [mode]() { ZigBeeComponent::esp_zigbee_alarm_bdb_commissioning(mode); });
    return;
  }
  (void) ezb_bdb_start_top_level_commissioning(mode);
  esp_zigbee_lock_release();
}

void ZigBeeComponent::report() {
  for (const auto &[_, attribute] : this->attributes_) {
    attribute->report();
  }
}

bool ZigBeeComponent::app_signal_handler(const ezb_app_signal_t *app_signal) {
  static uint8_t steering_retry_count = 0;
  ezb_zdo_signal_leave_params_t *leave_params = NULL;
  ezb_app_signal_type_t signal_type = ezb_app_signal_get_type(app_signal);
  switch (signal_type) {
    case EZB_ZDO_SIGNAL_SKIP_STARTUP:
      ESP_LOGD(TAG, "Zigbee stack initialized");
      ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_INITIALIZATION);
      break;
    case EZB_BDB_SIGNAL_DEVICE_FIRST_START:
      // Device started for the first time after the NVRAM erase
    case EZB_BDB_SIGNAL_DEVICE_REBOOT: {
      // Device started using the NVRAM contents.
      ezb_bdb_comm_status_t status = *((ezb_bdb_comm_status_t *) ezb_app_signal_get_params(app_signal));
      if (status == EZB_BDB_STATUS_SUCCESS) {
        ESP_LOGD(TAG, "Device started up in %sfactory-reset mode", ezb_bdb_is_factory_new() ? "" : "non ");
        global_zigbee->started_ = true;
        if (ezb_bdb_is_factory_new()) {
          ESP_LOGD(TAG, "Start network steering");
          ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
        } else {
          ESP_LOGD(TAG, "Device rebooted");
          global_zigbee->searchBindings();
        }
      } else {
        ESP_LOGW(TAG, "The %s failed with status(0x%02x), please retry", ezb_app_signal_to_string(signal_type), status);
        global_zigbee->set_timeout("zb_init", 1000, []() {
          ZigBeeComponent::esp_zigbee_alarm_bdb_commissioning(EZB_BDB_MODE_INITIALIZATION);
        });
      }
    } break;
    case EZB_BDB_SIGNAL_STEERING: {
      // BDB network steering completed (Network steering only)
      ezb_bdb_comm_status_t status = *((ezb_bdb_comm_status_t *) ezb_app_signal_get_params(app_signal));
      if (status == EZB_BDB_STATUS_SUCCESS) {
        steering_retry_count = 0;
        ezb_extpanid_t extended_pan_id;
        ezb_nwk_get_extended_panid(&extended_pan_id);
        ESP_LOGI(TAG, "Joined network successfully: PAN ID(0x%04hx, EXT: 0x%llx), Channel(%d), Short Address(0x%04hx)",
                 ezb_nwk_get_panid(), extended_pan_id.u64, ezb_nwk_get_current_channel(), ezb_nwk_get_short_address());
        global_zigbee->joined_ = true;
        global_zigbee->enable_loop_soon_any_context();
      } else {
        ESP_LOGI(TAG, "Failed to join network with status(0x%02x)", status);
        if (steering_retry_count < 10) {
          steering_retry_count++;
          global_zigbee->set_timeout("zb_init", 1000, []() {
            ZigBeeComponent::esp_zigbee_alarm_bdb_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
          });
        } else {
          global_zigbee->set_timeout("zb_init", 600 * 1000, []() {
            ZigBeeComponent::esp_zigbee_alarm_bdb_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
          });
        }
      }
    } break;
    case EZB_ZDO_SIGNAL_LEAVE: {
      const ezb_zdo_signal_leave_params_t *leave_params =
          (const ezb_zdo_signal_leave_params_t *) ezb_app_signal_get_params(app_signal);
      if (leave_params->leave_type == EZB_ZDO_LEAVE_TYPE_RESET) {
        ESP_LOGD(TAG, "Reset device");
        esp_zigbee_factory_reset();
      } else {
        ESP_LOGD(TAG, "Leave_type: %u", leave_params->leave_type);
      }
    } break;
    default:
      ESP_LOGI(TAG, "Zigbee APP Signal: %s(type: 0x%02x)", ezb_app_signal_to_string(signal_type), signal_type);
      break;
  }
  return true;
}

// Recall bounded devices from the binding table after reboot
void ZigBeeComponent::bindingTableCb(const ezb_zdo_nwk_mgmt_bind_req_result_t *result, void *user_ctx) {
  bool done = true;
  ezb_zdo_nwk_mgmt_bind_req_t *req = (ezb_zdo_nwk_mgmt_bind_req_t *) user_ctx;
  uint16_t dst_nwk_addr = req->dst_nwk_addr;
  ezb_zdp_status_t status = result->rsp->status;
  ESP_LOGD(TAG, "Binding table callback for address 0x%04x with status %d", req->dst_nwk_addr, status);
  if (status == EZB_ZDP_STATUS_SUCCESS) {
    // Print binding table log simple
    ESP_LOGD(TAG, "Binding table info: total %d, index %d, count %d", result->rsp->binding_table_entries,
             result->rsp->start_index, result->rsp->binding_table_list_count);

    if (result->rsp->binding_table_entries == 0) {
      ESP_LOGD(TAG, "No binding table entries found");
      free(req);
      global_zigbee->connected_ = true;
      return;
    }

    ezb_zdp_nwk_mgmt_bind_table_entry_t *record = result->rsp->binding_table_list;
    for (int i = 0; i < result->rsp->binding_table_list_count; i++) {
      ESP_LOGD(TAG, "Binding table record: src_endp %d, dst_endp %d, cluster_id 0x%04x, dst_addr_mode %d",
               record->src_ep, record->dst_ep, record->cluster_id, record->dst_addr_mode);

      zb_device_params_t *device = (zb_device_params_t *) calloc(1, sizeof(zb_device_params_t));
      device->endpoint = record->dst_ep;
      if (record->dst_addr_mode == EZB_APS_ADDR_MODE_16_ENDP_PRESENT ||
          record->dst_addr_mode == EZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT) {
        device->short_addr = record->dst_addr.short_addr;
      } else {  // EZB_APS_ADDR_MODE_64_ENDP_PRESENT
        memcpy(&device->ieee_addr, &record->dst_addr.extended_addr, sizeof(ezb_extaddr_t));
      }
      ESP_LOGD(TAG,
               "Device bound to EP %d -> device endpoint: %d, short addr: 0x%04x, ieee addr: "
               "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
               record->src_ep, device->endpoint, device->short_addr, device->ieee_addr.u8[7], device->ieee_addr.u8[6],
               device->ieee_addr.u8[5], device->ieee_addr.u8[4], device->ieee_addr.u8[3], device->ieee_addr.u8[2],
               device->ieee_addr.u8[1], device->ieee_addr.u8[0]);
      record++;
    }

    // Continue reading the binding table
    if (result->rsp->start_index + result->rsp->binding_table_list_count < result->rsp->binding_table_entries) {
      /* There are unreported binding table entries, request for them. */
      req->field.start_index = result->rsp->start_index + result->rsp->binding_table_list_count;
      ezb_zdo_nwk_mgmt_bind_req(req);
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
  ezb_zdo_nwk_mgmt_bind_req_t *mb_req = (ezb_zdo_nwk_mgmt_bind_req_t *) malloc(sizeof(ezb_zdo_nwk_mgmt_bind_req_t));
  mb_req->dst_nwk_addr = ezb_get_short_address();
  mb_req->field.start_index = 0;
  mb_req->cb = ZigBeeComponent::bindingTableCb;
  mb_req->user_ctx = mb_req;  // Pass the request as user context to the callback for later reference
  ESP_LOGD(TAG, "Requesting binding table for address 0x%04x", mb_req->dst_nwk_addr);
  ezb_zdo_nwk_mgmt_bind_req(mb_req);
}

void load_zb_event(ZBEvent *event, ezb_zcl_message_info_t info, ezb_zcl_attribute_t attribute, uint8_t *current_level) {
  event->load_set_attr_value_event(info, attribute, current_level);
}

void load_zb_event(ZBEvent *event, const ezb_zcl_cmd_report_attr_message_t *message) {
  event->load_report_attr_event(message);
}

void load_zb_event(ZBEvent *event, ezb_zcl_message_info_t info, ezb_zcl_read_attr_rsp_variable_t *variables) {
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
template void enqueue_zb_event(ezb_zcl_message_info_t info, ezb_zcl_attribute_t attribute, uint8_t *current_level);
template void enqueue_zb_event(const ezb_zcl_cmd_report_attr_message_t *message);
template void enqueue_zb_event(ezb_zcl_message_info_t info, ezb_zcl_read_attr_rsp_variable_t *variables);

static void zb_attribute_handler(ezb_zcl_set_attr_value_message_t *message) {
  ESP_RETURN_ON_FALSE(message, , TAG, "Empty message");
  ESP_RETURN_ON_FALSE(message->info.status == EZB_ZCL_STATUS_SUCCESS, , TAG, "Received message: error status(%d)",
                      message->info.status);
  ESP_LOGD(TAG, "ZCL SetAttributeValue message for endpoint(%d) cluster(0x%04x) %s with status(0x%02x)",
           message->info.dst_ep, message->info.cluster_id,
           message->info.cluster_role == EZB_ZCL_CLUSTER_SERVER ? "server" : "client", message->info.status);

  // if the attribute is On/Off and it is set to Off, restore the previous level
  ezb_zcl_attr_desc_t current_level;
  if (message->info.cluster_id == EZB_ZCL_CLUSTER_ID_ON_OFF) {
    if (message->in.attribute.id == EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && !*(bool *) message->in.attribute.data.value) {
      ESP_LOGD(TAG, "turned off");
      if (ezb_zcl_get_cluster_desc(message->info.dst_ep, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER) != NULL) {
        current_level = ezb_zcl_get_attr_desc(message->info.dst_ep, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                                              EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID, EZB_ZCL_STD_MANUF_CODE);
        if (current_level) {
          ESP_LOGD(TAG, "got level");
          void *val = NULL;
          ezb_zcl_attr_desc_get_value(current_level, val);
          ezb_zcl_set_attr_value(message->info.dst_ep, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                                 EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID, EZB_ZCL_STD_MANUF_CODE, val, false);
        }
      }
    }
  }
  if (current_level != NULL) {
    void *val = NULL;
    ezb_zcl_attr_desc_get_value(current_level, val);
    enqueue_zb_event(message->info, message->in.attribute, (uint8_t *) val);
  } else {
    enqueue_zb_event(message->info, message->in.attribute, nullptr);
  }
  return;
}

static void zb_cmd_attribute_handler(ezb_zcl_cmd_read_attr_rsp_message_t *message) {
  ESP_RETURN_ON_FALSE(message, , TAG, "Empty message");
  ESP_RETURN_ON_FALSE(message->info.status == EZB_ZCL_STATUS_SUCCESS, , TAG, "Received message: error status(%d)",
                      message->info.status);
  ESP_LOGD(TAG, "ZCL Read Attribute Response message for endpoint(%d) cluster(0x%04x) %s with status(0x%02x)",
           message->info.dst_ep, message->info.cluster_id,
           message->info.cluster_role == EZB_ZCL_CLUSTER_SERVER ? "server" : "client", message->info.status);
  enqueue_zb_event(message->info, message->in.variables);
  return;
}

static void zb_report_attribute_handler(ezb_zcl_cmd_report_attr_message_t *message) {
  ESP_RETURN_ON_FALSE(message, , TAG, "Empty message");
  ESP_RETURN_ON_FALSE(message->info.status == EZB_ZCL_STATUS_SUCCESS, , TAG, "Received message: error status(%d)",
                      message->info.status);
  ESP_LOGD(TAG, "ZCL Report Attribute message for endpoint(%d) cluster(0x%04x) %s with status(0x%02x)",
           message->info.dst_ep, message->info.cluster_id,
           message->info.cluster_role == EZB_ZCL_CLUSTER_SERVER ? "server" : "client", message->info.status);
  enqueue_zb_event(message);
  return;
}

static void zb_action_handler(ezb_zcl_core_action_callback_id_t callback_id, void *message) {
  switch (callback_id) {
    case EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID:
      zb_attribute_handler((ezb_zcl_set_attr_value_message_t *) message);
      break;
    case EZB_ZCL_CORE_READ_ATTR_RSP_CB_ID:
      zb_cmd_attribute_handler((ezb_zcl_cmd_read_attr_rsp_message_t *) message);
      break;
    case EZB_ZCL_CORE_REPORT_ATTR_CB_ID:
      zb_report_attribute_handler((ezb_zcl_cmd_report_attr_message_t *) message);
      break;
    case EZB_ZCL_CORE_DEFAULT_RSP_CB_ID: {
      ezb_zcl_cmd_default_rsp_message_t *default_rsp = (ezb_zcl_cmd_default_rsp_message_t *) message;
      ESP_LOGD(TAG, "Received ZCL Default Response: 0x%02x", default_rsp->in.status_code);
    } break;
    default:
      ESP_LOGW(TAG, "Receive Zigbee action(0x%04lx) callback", callback_id);
      break;
  }
}

void ZigBeeComponent::handle_attribute(ezb_zcl_message_info_t info, ezb_zcl_attribute_t attribute,
                                       uint8_t *current_level) {
  if (this->attributes_.find({info.dst_ep, info.cluster_id, EZB_ZCL_CLUSTER_SERVER, attribute.id}) !=
      this->attributes_.end()) {
    this->attributes_[{info.dst_ep, info.cluster_id, EZB_ZCL_CLUSTER_SERVER, attribute.id}]->on_value(attribute);
    // if the attribute is On/Off and it is set to Off, restore the previous level
    if (info.cluster_id == EZB_ZCL_CLUSTER_ID_ON_OFF && current_level != nullptr) {
      if (attribute.id == EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && attribute.data.type == EZB_ZCL_ATTR_TYPE_BOOL &&
          !*(bool *) attribute.data.value) {
        ESP_LOGD(TAG, "turned off");
        if (this->attributes_.find({info.dst_ep, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                                    EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID}) != this->attributes_.end()) {
          ESP_LOGD(TAG, "found level");
          ezb_zcl_attribute_t lvl_attr = {
              .id = EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID,
              .data =
                  {
                      .type = EZB_ZCL_ATTR_TYPE_UINT8,
                      .size = sizeof(uint8_t),
                      .value = current_level,
                  },
          };
          this->attributes_[{info.dst_ep, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                             EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID}]
              ->on_value(lvl_attr);
          ESP_LOGD(TAG, "Light set to restore-level: %d", *current_level);
        }
      }
    }
  }
}

void ZigBeeComponent::handle_report_attribute(uint8_t dst_endpoint, uint16_t cluster,
                                              ezb_zcl_report_attr_variable_t variables, ezb_address_t src_address,
                                              uint8_t src_endpoint) {
  // TODO loop through all attributes in the report (currently only handles the first one) and find matching attribute
  // handlers for each of them
  auto attr = this->attributes_.find({dst_endpoint, cluster, EZB_ZCL_CLUSTER_CLIENT, variables.attr_id});
  if (attr == this->attributes_.end()) {
    ESP_LOGD(TAG, "No attributes configured for report (endpoint %d; cluster 0x%04x; attribute id 0x%04x)",
             dst_endpoint, cluster, variables.attr_id);
    return;
  }
  attr->second->on_report(variables.attr_value, src_address, src_endpoint);
}

void ZigBeeComponent::handle_read_attribute_response(ezb_zcl_message_info_t info,
                                                     ezb_zcl_read_attr_rsp_variable_t *variables) {
  switch (info.cluster_id) {
    case EZB_ZCL_CLUSTER_ID_TIME:
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
                 variables->status, info.cluster_id, variables->attr_id, variables->attr_type,
                 variables->attr_value ? *(uint8_t *) variables->attr_value : 0);
        variables = variables->next;
      }
  }
}

void ZigBeeComponent::create_default_cluster(uint8_t endpoint_id, uint16_t device_id) {
  ezb_af_ep_desc_t ep_desc = esphome_zb_zha_default_ep_desc_create(endpoint_id, device_id, this->device_version_,
                                                                   this->basic_cluster_data_.power);
  this->endpoint_list_[endpoint_id] = std::tuple<uint16_t, ezb_af_ep_desc_t>(device_id, ep_desc);
  // Add basic cluster
  this->update_basic_cluster_(ep_desc);
  // Add identify cluster if not already present
  if (ezb_af_endpoint_get_cluster_desc(ep_desc, EZB_ZCL_CLUSTER_ID_IDENTIFY, EZB_ZCL_CLUSTER_SERVER) == NULL) {
    this->add_cluster(endpoint_id, EZB_ZCL_CLUSTER_ID_IDENTIFY, EZB_ZCL_CLUSTER_SERVER);
  }
}

void ZigBeeComponent::add_cluster(uint8_t endpoint_id, uint16_t cluster_id, uint8_t role) {
  ezb_zcl_cluster_desc_t cluster_desc;
  switch (cluster_id) {
    case 0:
      return;
    default:
      cluster_desc = esphome_zb_default_cluster_dscr_create(cluster_id, role);
  }
  esphome_zb_add_or_update_cluster(cluster_id, std::get<1>(this->endpoint_list_[endpoint_id]), cluster_desc, role);
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

void ZigBeeComponent::update_basic_cluster_(ezb_af_ep_desc_t ep_desc) {
  // ------------------------------ Cluster BASIC ------------------------------
  ezb_zcl_cluster_desc_t cluster_desc =
      ezb_af_endpoint_get_cluster_desc(ep_desc, EZB_ZCL_CLUSTER_ID_BASIC, EZB_ZCL_CLUSTER_SERVER);
  if (cluster_desc == NULL) {
    ezb_zcl_basic_cluster_config_t basic_cluster_cfg = {
        .zcl_version = EZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = this->basic_cluster_data_.power,
    };
    cluster_desc = ezb_zcl_basic_create_cluster_desc(&basic_cluster_cfg, EZB_ZCL_CLUSTER_SERVER);
  }

  ESP_LOGD(TAG, "Model: %s", this->basic_cluster_data_.model.c_str());
  ESP_LOGD(TAG, "Manufacturer: %s", this->basic_cluster_data_.manufacturer.c_str());
  ESP_LOGD(TAG, "Date: %s", this->basic_cluster_data_.date.c_str());
  ESP_LOGD(TAG, "Area: %s", this->basic_cluster_data_.area.c_str());
  uint8_t *ManufacturerName = get_zcl_string(this->basic_cluster_data_.manufacturer.c_str(),
                                             32);  // warning: this is in format {length, 'string'} :
  uint8_t *ModelIdentifier = get_zcl_string(this->basic_cluster_data_.model.c_str(), 32);
  uint8_t *DateCode = get_zcl_string(this->basic_cluster_data_.date.c_str(), 16);
  uint8_t *Location = get_zcl_string(this->basic_cluster_data_.area.c_str(), 16);
  ezb_zcl_basic_cluster_desc_add_attr(cluster_desc, EZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID,
                                      &(this->basic_cluster_data_.app_version));
  ezb_zcl_basic_cluster_desc_add_attr(cluster_desc, EZB_ZCL_ATTR_BASIC_STACK_VERSION_ID,
                                      &(this->basic_cluster_data_.stack_version));
  ezb_zcl_basic_cluster_desc_add_attr(cluster_desc, EZB_ZCL_ATTR_BASIC_HW_VERSION_ID,
                                      &(this->basic_cluster_data_.hw_version));
  ezb_zcl_basic_cluster_desc_add_attr(cluster_desc, EZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, ManufacturerName);
  ezb_zcl_basic_cluster_desc_add_attr(cluster_desc, EZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, ModelIdentifier);
  ezb_zcl_basic_cluster_desc_add_attr(cluster_desc, EZB_ZCL_ATTR_BASIC_DATE_CODE_ID, DateCode);
  ezb_zcl_basic_cluster_desc_add_attr(cluster_desc, EZB_ZCL_ATTR_BASIC_LOCATION_DESCRIPTION_ID, Location);
  ezb_zcl_basic_cluster_desc_add_attr(cluster_desc, EZB_ZCL_ATTR_BASIC_PHYSICAL_ENVIRONMENT_ID,
                                      &(this->basic_cluster_data_.physical_env));
  delete[] ManufacturerName;
  delete[] ModelIdentifier;
  delete[] DateCode;
  delete[] Location;
  ezb_af_endpoint_add_cluster_desc(ep_desc, cluster_desc);
}

static void ezb_task_(void *pvParameters) {
  if (esp_zigbee_start(false) != ESP_OK) {
    ESP_LOGE(TAG, "Could not setup Zigbee");
    // this->mark_failed();
    vTaskDelete(NULL);
  }

  esp_zigbee_launch_mainloop();

  esp_zigbee_deinit();

  vTaskDelete(NULL);
}

void ZigBeeComponent::setup() {
  global_zigbee = this;
#ifdef CONFIG_WIFI_COEX
  if (esp_coex_wifi_i154_enable() != ESP_OK) {
    this->mark_failed();
    return;
  }
#endif
  // ESP_ERROR_CHECK(nvs_flash_init()); not needed, called by esp32 component
  ESP_ERROR_CHECK(nvs_flash_init_partition("zb_storage"));

  /* initialize Zigbee stack */
  esp_zigbee_platform_config_t platform_config = {
      .storage_partition_name = ESP_ZIGBEE_STORAGE_PARTITION_NAME,
      .radio_config = EZB_DEFAULT_RADIO_CONFIG(),
  };
  esp_zigbee_device_config_t device_config = {
      .device_type = this->device_role_,
      .install_code_policy = INSTALLCODE_POLICY_ENABLE,
  };
#ifdef ZB_ROUTER_ROLE
  esp_zigbee_zczr_config_s zb_zczr_cfg = {
      .max_children = MAX_CHILDREN,
  };
  device_config.zczr_config = zb_zczr_cfg;
#else
  esp_zigbee_zed_config_s zb_zed_cfg = {
      .ed_timeout = ED_AGING_TIMEOUT,
      .keep_alive = ED_KEEP_ALIVE,
  };
  device_config.zed_config = zb_zed_cfg;
#endif
  esp_zigbee_config_t config = {.device_config = device_config, .platform_config = platform_config};
  if (esp_zigbee_init(&config) != ESP_OK) {
    ESP_LOGE(TAG, "Could not initialize Zigbee");
    this->mark_failed();
    return;
  }
  ezb_aps_secur_enable_distributed_security(false);
  if (ezb_bdb_set_primary_channel_set(EZB_PRIMARY_CHANNEL_MASK)) {
    ESP_LOGE(TAG, "Could not set primary channel mask");
    this->mark_failed();
    return;
  }
  if (ezb_app_signal_add_handler(this->app_signal_handler) != ESP_OK) {
    ESP_LOGE(TAG, "Could not set application signal handler");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Zigbee stack initialized");
  if (this->custom_trust_center_key_) {
    //  ezb_enable_joining_to_distributed(true);
    //  ezb_secur_TC_standard_distributed_key_set(this->trustkey_);
  }

  ezb_err_t ret;

  // clusters
  // for (auto const &[key, val] : this->attribute_list_) {
  //   ezb_af_ep_desc_t ep_desc = std::get<1>(this->endpoint_list_[std::get<0>(key)]);
  //   ret = esphome_zb_add_or_update_cluster(std::get<1>(key), ep_desc, val, std::get<2>(key));
  //   if (ret != EZB_ERR_NONE) {
  //     ESP_LOGE(TAG, "Could not create cluster 0x%04X with role %u: %s", std::get<1>(key), std::get<2>(key),
  //              esp_err_to_name(ret));
  //   }
  // }
  // this->attribute_list_.clear();

  // endpoints
  for (auto const &[ep_id, dev] : this->endpoint_list_) {
    if (ezb_af_device_add_endpoint_desc(this->dev_desc_, std::get<1>(dev)) != EZB_ERR_NONE) {
      ESP_LOGE(TAG, "Could not create endpoint %u", ep_id);
    }
  }

  // ------------------------------ Register Device ------------------------------
  if (ezb_af_device_desc_register(this->dev_desc_) != EZB_ERR_NONE) {
    ESP_LOGE(TAG, "Could not register the endpoint list");
    this->mark_failed();
    return;
  }

  ezb_zcl_core_action_handler_register(zb_action_handler);

  // if (ezb_bdb_set_primary_channel_set(EZB_PRIMARY_CHANNEL_MASK) != ESP_OK) {
  //   ESP_LOGE(TAG, "Could not setup Zigbee");
  //   this->mark_failed();
  //   return;
  // }

  // reporting
  for (auto &[_, attribute] : this->attributes_) {
    if (attribute->report_enabled) {
      // ezb_zcl_reporting_info_t reporting_info = attribute->get_reporting_info();
      // ESP_LOGD(TAG, "set reporting for cluster: %u", reporting_info.cluster_id);
      // if (ezb_zcl_update_reporting_info(&reporting_info) != ESP_OK) {
      //   ESP_LOGE(TAG, "Could not configure reporting for attribute 0x%04X in cluster 0x%04X in endpoint %u",
      //            reporting_info.attr_id, reporting_info.cluster_id, reporting_info.ep);
      // }
    }
  }
  xTaskCreate(ezb_task_, "Zigbee_main", 4096, NULL, 24, NULL);
  this->disable_loop();  // loop is only needed for processing events, so disable until we join a network
}

void ZigBeeComponent::loop() {
  // Process all pending events
  ZBEvent *event = this->zb_events_.pop();
  while (event != nullptr) {
    // Handle the event
    switch (event->callback_id_) {
      case EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID:
        this->handle_attribute(
            event->event_.set_attr.info, event->event_.set_attr.attribute,
            event->event_.set_attr.has_current_level ? &event->event_.set_attr.current_level : nullptr);
        break;
      case EZB_ZCL_CORE_READ_ATTR_RSP_CB_ID:
        this->handle_read_attribute_response(event->event_.read_attr_resp.info,
                                             &(event->event_.read_attr_resp.variables));
        break;
      case EZB_ZCL_CORE_REPORT_ATTR_CB_ID:
        this->handle_report_attribute(event->event_.report_attr.dst_endpoint, event->event_.report_attr.cluster,
                                      event->event_.report_attr.variables, event->event_.report_attr.src_address,
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
  char trustkey_hex[format_hex_pretty_size(sizeof(this->trustkey_))];
  ESP_LOGCONFIG(TAG, "ZigBee:");
  ESP_LOGCONFIG(TAG, "  Device Version: %u", this->device_version_);
  if (this->custom_trust_center_key_) {
    ESP_LOGCONFIG(TAG, "  Custom Trust Center Key: %s",
                  format_hex_pretty_to(trustkey_hex, this->trustkey_, sizeof(this->trustkey_), '.'));
  }
  for (auto const &[key, val] : this->endpoint_list_) {
    ESP_LOGCONFIG(TAG, "  Endpoint: %u, %d", key, std::get<0>(val));
  }
}

void ZigBeeComponent::set_trust_center_key(const char *trust_center_key) {
  parse_hex(trust_center_key, this->trustkey_, sizeof(this->trustkey_));
  this->custom_trust_center_key_ = true;
}

}  // namespace zigbee
}  // namespace esphome
