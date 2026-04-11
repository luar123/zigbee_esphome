#pragma once

#ifdef USE_ESP32

#include <cstddef>  // for offsetof
#include <cstring>  // for memcpy
#include "esp_zigbee_core.h"
#include "zboss_api.h"
#include "ha/esp_zigbee_ha_standard.h"

namespace esphome::zigbee {

class ZBEvent {
 public:
  ZBEvent(esp_zb_device_cb_common_info_t info, esp_zb_zcl_attribute_t attribute, uint8_t *current_level) {
    this->callback_id_ = ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID;
    this->init_set_attr_value_data(info, attribute, current_level);
  }

  ZBEvent(const esp_zb_zcl_report_attr_message_t *message) {
    this->callback_id_ = ESP_ZB_CORE_REPORT_ATTR_CB_ID;
    this->init_report_attr_data(message);
  }

  ZBEvent(esp_zb_zcl_cmd_info_t info, esp_zb_zcl_read_attr_resp_variable_t *variables) {
    this->callback_id_ = ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID;
    this->init_read_attr_resp_data(info, variables);
  }

  ~ZBEvent() { this->release(); }

  ZBEvent() : event_{}, callback_id_(ESP_ZB_CORE_BASIC_RESET_TO_FACTORY_RESET_CB_ID) {}

  void release() {
    // Free any allocated memory within the event
    switch (this->callback_id_) {
      case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        if (this->event_.set_attr.attribute.data.value != nullptr &&
            this->event_.set_attr.attribute.data.value != this->event_.set_attr.inline_data) {
          free(this->event_.set_attr.attribute.data.value);
          this->event_.set_attr.attribute.data.value = nullptr;
        }
        break;
      case ESP_ZB_CORE_REPORT_ATTR_CB_ID:
        if (this->event_.report_attr.attribute.data.value != nullptr &&
            this->event_.report_attr.attribute.data.value != this->event_.report_attr.inline_data) {
          free(this->event_.report_attr.attribute.data.value);
          this->event_.report_attr.attribute.data.value = nullptr;
        }
        break;
      case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID: {
        esp_zb_zcl_read_attr_resp_variable_t *var = &(this->event_.read_attr_resp.variables);
        while (var != nullptr) {
          if (var->attribute.data.value != nullptr &&
              var->attribute.data.value != this->event_.read_attr_resp.inline_data) {
            free(var->attribute.data.value);
            var->attribute.data.value = nullptr;
          }
          var = var->next;
        }
      }
        // Reset the event data
        this->callback_id_ = ESP_ZB_CORE_BASIC_RESET_TO_FACTORY_RESET_CB_ID;  // or some invalid value
        break;
      default:
        break;
    }
  }

  void load_set_attr_value_event(esp_zb_device_cb_common_info_t info, esp_zb_zcl_attribute_t attribute,
                                 uint8_t *current_level) {
    this->release();
    this->callback_id_ = ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID;
    this->init_set_attr_value_data(info, attribute, current_level);
  }

  void load_report_attr_event(const esp_zb_zcl_report_attr_message_t *message) {
    this->release();
    this->callback_id_ = ESP_ZB_CORE_REPORT_ATTR_CB_ID;
    this->init_report_attr_data(message);
  }

  void load_read_attr_resp_event(esp_zb_zcl_cmd_info_t info, esp_zb_zcl_read_attr_resp_variable_t *variables) {
    this->release();
    this->callback_id_ = ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID;
    this->init_read_attr_resp_data(info, variables);
  }

  // Disable copy to prevent double-delete
  ZBEvent(const ZBEvent &) = delete;
  ZBEvent &operator=(const ZBEvent &) = delete;

  union {
    struct set_attr_event {
      esp_zb_device_cb_common_info_t info;
      esp_zb_zcl_attribute_t attribute;
      uint8_t current_level;
      bool has_current_level;
      uint8_t inline_data[4];  // For small data types
    } set_attr;
    struct report_attr_event {
      uint8_t dst_endpoint;
      uint16_t cluster;
      esp_zb_zcl_attribute_t attribute;
      esp_zb_zcl_addr_t src_address;
      uint8_t src_endpoint;
      uint8_t inline_data[4];  // For small data types
    } report_attr;
    struct read_attr_resp_event {
      esp_zb_zcl_cmd_info_t info;
      esp_zb_zcl_read_attr_resp_variable_t variables;
      uint8_t inline_data[4];  // For small data types
    } read_attr_resp;
  } event_;

  esp_zb_core_action_callback_id_t callback_id_;

 private:
  void init_set_attr_value_data(esp_zb_device_cb_common_info_t info, esp_zb_zcl_attribute_t attribute,
                                uint8_t *current_level) {
    this->event_.set_attr.info = info;
    this->event_.set_attr.attribute = attribute;
    // get attribute.data.value with correct type
    this->event_.set_attr.has_current_level = (current_level != nullptr);
    if (current_level != nullptr) {
      this->event_.set_attr.current_level = *current_level;
    }
    if (attribute.data.value != nullptr) {
      // Copy the attribute value to avoid dangling pointer issues
      size_t value_size = this->get_attribute_value_size_(attribute.data);
      if (value_size > 4) {
        this->event_.set_attr.attribute.data.value = malloc(value_size);
        if (this->event_.set_attr.attribute.data.value != nullptr) {
          memcpy(this->event_.set_attr.attribute.data.value, attribute.data.value, value_size);
        }
      } else {
        memcpy(this->event_.set_attr.inline_data, attribute.data.value, value_size);
        this->event_.set_attr.attribute.data.value = this->event_.set_attr.inline_data;
      }
    }
  }

  void init_report_attr_data(const esp_zb_zcl_report_attr_message_t *message) {
    this->event_.report_attr.dst_endpoint = message->dst_endpoint;
    this->event_.report_attr.cluster = message->cluster;
    this->event_.report_attr.attribute = message->attribute;
    this->event_.report_attr.src_address = message->src_address;
    this->event_.report_attr.src_endpoint = message->src_endpoint;
    if (message->attribute.data.value != nullptr) {
      // Copy the attribute value to avoid dangling pointer issues
      size_t value_size = this->get_attribute_value_size_(message->attribute.data);
      if (value_size > 4) {
        this->event_.report_attr.attribute.data.value = malloc(value_size);
        if (this->event_.report_attr.attribute.data.value != nullptr) {
          memcpy(this->event_.report_attr.attribute.data.value, message->attribute.data.value, value_size);
        }
      } else {
        memcpy(this->event_.report_attr.inline_data, message->attribute.data.value, value_size);
        this->event_.report_attr.attribute.data.value = this->event_.report_attr.inline_data;
      }
    }
  }

  void init_read_attr_resp_data(esp_zb_zcl_cmd_info_t info, esp_zb_zcl_read_attr_resp_variable_t *variables) {
    this->event_.read_attr_resp.info = info;
    if (variables == nullptr) {
      return;
    }
    this->event_.read_attr_resp.variables.status = variables->status;
    this->event_.read_attr_resp.variables.attribute.id = variables->attribute.id;
    this->event_.read_attr_resp.variables.attribute.data.type = variables->attribute.data.type;
    this->event_.read_attr_resp.variables.attribute.data.size = variables->attribute.data.size;
    // Note: variables is a pointer to a struct/list; deep copy needed
    if (variables->attribute.data.value != nullptr) {
      size_t value_size = this->get_attribute_value_size_(variables->attribute.data);
      if (value_size > 4) {
        this->event_.read_attr_resp.variables.attribute.data.value = malloc(value_size);
        if (this->event_.read_attr_resp.variables.attribute.data.value != nullptr) {
          memcpy(this->event_.read_attr_resp.variables.attribute.data.value, variables->attribute.data.value,
                 value_size);
        }
      } else {
        memcpy(this->event_.read_attr_resp.inline_data, variables->attribute.data.value, value_size);
        this->event_.read_attr_resp.variables.attribute.data.value = this->event_.read_attr_resp.inline_data;
      }
    }
    esp_zb_zcl_read_attr_resp_variable_t *var = variables;
    esp_zb_zcl_read_attr_resp_variable_t *var_last = &(this->event_.read_attr_resp.variables);
    while (var->next != nullptr) {
      var = var->next;
      size_t value_size = this->get_attribute_value_size_(var->attribute.data);
      var_last->next = new esp_zb_zcl_read_attr_resp_variable_t();
      if (var_last->next != nullptr) {
        var_last->next->attribute.id = var->attribute.id;
        var_last->next->status = var->status;
        var_last->next->attribute.data.type = var->attribute.data.type;
        var_last->next->attribute.data.size = var->attribute.data.size;
        var_last->next->attribute.data.value = malloc(value_size);
        if (var_last->next->attribute.data.value != nullptr) {
          memcpy(var_last->next->attribute.data.value, var->attribute.data.value, value_size);
        }
        var_last = var_last->next;
      }
    }
  }

  size_t get_attribute_value_size_(esp_zb_zcl_attribute_data_t data) {
    // Determine attribute value size based on attribute type (and size)
    uint8_t len = 0;
    switch (data.type) {
      case ESP_ZB_ZCL_ATTR_TYPE_BOOL:
        return sizeof(bool);
      case ESP_ZB_ZCL_ATTR_TYPE_U8:
        return sizeof(uint8_t);
      case ESP_ZB_ZCL_ATTR_TYPE_U16:
        return sizeof(uint16_t);
      case ESP_ZB_ZCL_ATTR_TYPE_U24:
        return 3;  // 24 bits = 3 bytes
      case ESP_ZB_ZCL_ATTR_TYPE_U32:
        return sizeof(uint32_t);
      case ESP_ZB_ZCL_ATTR_TYPE_U40:
        return 5;  // 40 bits = 5 bytes
      case ESP_ZB_ZCL_ATTR_TYPE_U48:
        return 6;  // 48 bits = 6 bytes
      case ESP_ZB_ZCL_ATTR_TYPE_U56:
        return 7;  // 56 bits = 7 bytes
      case ESP_ZB_ZCL_ATTR_TYPE_U64:
        return sizeof(uint64_t);
      case ESP_ZB_ZCL_ATTR_TYPE_S8:
        return sizeof(int8_t);
      case ESP_ZB_ZCL_ATTR_TYPE_S16:
        return sizeof(int16_t);
      case ESP_ZB_ZCL_ATTR_TYPE_S24:
        return 3;  // 24 bits = 3 bytes
      case ESP_ZB_ZCL_ATTR_TYPE_S32:
        return sizeof(int32_t);
      case ESP_ZB_ZCL_ATTR_TYPE_S40:
        return 5;  // 40 bits = 5 bytes
      case ESP_ZB_ZCL_ATTR_TYPE_S48:
        return 6;  // 48 bits = 6 bytes
      case ESP_ZB_ZCL_ATTR_TYPE_S56:
        return 7;  // 56 bits = 7 bytes
      case ESP_ZB_ZCL_ATTR_TYPE_S64:
        return sizeof(int64_t);
      case ESP_ZB_ZCL_ATTR_TYPE_8BITMAP:
        return sizeof(uint8_t);
      case ESP_ZB_ZCL_ATTR_TYPE_16BITMAP:
        return sizeof(uint16_t);
      case ESP_ZB_ZCL_ATTR_TYPE_24BITMAP:
        return 3;  // 24 bits = 3 bytes
      case ESP_ZB_ZCL_ATTR_TYPE_32BITMAP:
        return sizeof(uint32_t);
      case ESP_ZB_ZCL_ATTR_TYPE_40BITMAP:
        return 5;  // 40 bits = 5 bytes
      case ESP_ZB_ZCL_ATTR_TYPE_48BITMAP:
        return 6;  // 48 bits = 6 bytes
      case ESP_ZB_ZCL_ATTR_TYPE_56BITMAP:
        return 7;  // 56 bits = 7 bytes
      case ESP_ZB_ZCL_ATTR_TYPE_64BITMAP:
        return sizeof(uint64_t);
      case ESP_ZB_ZCL_ATTR_TYPE_8BIT:
        return sizeof(uint8_t);
      case ESP_ZB_ZCL_ATTR_TYPE_16BIT:
        return sizeof(uint16_t);
      case ESP_ZB_ZCL_ATTR_TYPE_24BIT:
        return 3;  // 24 bits = 3 bytes
      case ESP_ZB_ZCL_ATTR_TYPE_32BIT:
        return sizeof(uint32_t);
      case ESP_ZB_ZCL_ATTR_TYPE_40BIT:
        return 5;  // 40 bits = 5 bytes
      case ESP_ZB_ZCL_ATTR_TYPE_48BIT:
        return 6;  // 48 bits = 6 bytes
      case ESP_ZB_ZCL_ATTR_TYPE_56BIT:
        return 7;  // 56 bits = 7 bytes
      case ESP_ZB_ZCL_ATTR_TYPE_64BIT:
        return sizeof(uint64_t);
      case ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM:
        return sizeof(uint8_t);
      case ESP_ZB_ZCL_ATTR_TYPE_16BIT_ENUM:
        return sizeof(uint16_t);
      case ESP_ZB_ZCL_ATTR_TYPE_SEMI:
        return sizeof(uint16_t);
      case ESP_ZB_ZCL_ATTR_TYPE_SINGLE:
        return sizeof(float);
      case ESP_ZB_ZCL_ATTR_TYPE_DOUBLE:
        return sizeof(double);
      case ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING:
        len = *((uint8_t *) data.value);  // first byte is length
        return len + 1;                   // length byte + string
      case ESP_ZB_ZCL_ATTR_TYPE_TIME_OF_DAY:
        return sizeof(uint32_t);
      case ESP_ZB_ZCL_ATTR_TYPE_DATE:
        return sizeof(uint32_t);
      case ESP_ZB_ZCL_ATTR_TYPE_UTC_TIME:
        return sizeof(uint32_t);
      case ESP_ZB_ZCL_ATTR_TYPE_CLUSTER_ID:
        return sizeof(uint16_t);
      case ESP_ZB_ZCL_ATTR_TYPE_ATTRIBUTE_ID:
        return sizeof(uint16_t);
      case ESP_ZB_ZCL_ATTR_TYPE_BACNET_OID:
        return sizeof(uint32_t);
      case ESP_ZB_ZCL_ATTR_TYPE_IEEE_ADDR:
        return sizeof(uint64_t);
      case ESP_ZB_ZCL_ATTR_TYPE_128_BIT_KEY:
        return 16;  // 128 bits = 16 bytes
      default:
        return 0;  // Unknown type
    }
  }
};
}  // namespace esphome::zigbee
#endif  // USE_ESP32