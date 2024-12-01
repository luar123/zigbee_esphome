#pragma once

//#include <stdfloat> //deactive because not working with esp-idf 5.1.4

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "zigbee.h"

namespace esphome {
namespace zigbee {

class ZigBeeJoinTrigger : public Trigger<> {
 public:
  explicit ZigBeeJoinTrigger(ZigBeeComponent *parent) {
    parent->add_on_join_callback([this]() { this->trigger(); });
  }
};

template<typename... Ts> class ResetZigbeeAction : public Action<Ts...>, public Parented<ZigBeeComponent> {
 public:
  void play(Ts... x) override { this->parent_->reset(); }
};

template<typename... Ts> class ReportAction : public Action<Ts...>, public Parented<ZigBeeComponent> {
 public:
  void play(Ts... x) override { this->parent_->report(); }
};

template<typename T, typename... Ts> class SetAttrAction : public Action<Ts...> {
 public:
  SetAttrAction(ZigBeeComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(T, value);

  void set_target(uint8_t ep, uint16_t cluster, uint8_t role, uint16_t attr) {
    this->ep_ = ep;
    this->cluster_ = cluster;
    this->role_ = role;
    this->attr_ = attr;
  }

  void play(Ts... x) override {
    T value = this->value_.value(x...);
    this->parent_->set_attr(this->ep_, this->cluster_, this->role_, this->attr_, &value);
  }

 protected:
  ZigBeeComponent *parent_;
  uint8_t ep_;
  uint16_t cluster_;
  uint8_t role_;
  uint16_t attr_;
};

template<typename Ts> class ZigBeeOnValueTrigger : public Trigger<Ts>, public Component {
 public:
  explicit ZigBeeOnValueTrigger(ZigBeeComponent *parent) : parent_(parent) {}
  void set_attr(uint8_t endpoint_id, uint16_t cluster_id, uint16_t attr_id, uint8_t attr_type) {
    this->ep_id_ = endpoint_id;
    this->cl_id_ = cluster_id;
    this->attr_id_ = attr_id;
    this->attr_type_ = attr_type;
  }
  void setup() override {
    this->parent_->add_on_value_callback([this](esp_zb_device_cb_common_info_t info, esp_zb_zcl_attribute_t attribute) {
      this->on_value_(info, attribute);
    });
  }

 protected:
  void on_value_(esp_zb_device_cb_common_info_t info, esp_zb_zcl_attribute_t attribute) {
    if (info.dst_endpoint == this->ep_id_) {
      if (info.cluster == this->cl_id_) {
        if (attribute.id == this->attr_id_ && attribute.data.type == attr_type_ && attribute.data.value) {
          this->trigger(get_value_by_type<Ts>(attr_type_, attribute.data.value));
        }
      }
    }
  }
  ZigBeeComponent *parent_;
  uint8_t ep_id_;
  uint16_t cl_id_;
  uint16_t attr_id_;
  uint8_t attr_type_;
};

template<class T> T get_value_by_type(uint8_t attr_type, void *data) {
  switch (attr_type) {
    case ESP_ZB_ZCL_ATTR_TYPE_SEMI:
      return 0;  //(T) * (std::float16_t *) data;
    case ESP_ZB_ZCL_ATTR_TYPE_OCTET_STRING:
      return 0;
    case ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING:
      return 0;
    case ESP_ZB_ZCL_ATTR_TYPE_LONG_OCTET_STRING:
      return 0;
    case ESP_ZB_ZCL_ATTR_TYPE_LONG_CHAR_STRING:
      return 0;
    case ESP_ZB_ZCL_ATTR_TYPE_ARRAY:
      return 0;
    case ESP_ZB_ZCL_ATTR_TYPE_16BIT_ARRAY:
      return 0;
    case ESP_ZB_ZCL_ATTR_TYPE_32BIT_ARRAY:
      return 0;
    case ESP_ZB_ZCL_ATTR_TYPE_STRUCTURE:
      return 0;
    case ESP_ZB_ZCL_ATTR_TYPE_SET:
      return 0;
    case ESP_ZB_ZCL_ATTR_TYPE_BAG:
      return 0;
    case ESP_ZB_ZCL_ATTR_TYPE_TIME_OF_DAY:
      return (T) * (uint32_t *) data;
    case ESP_ZB_ZCL_ATTR_TYPE_DATE:
      return (T) * (uint32_t *) data;
    case ESP_ZB_ZCL_ATTR_TYPE_UTC_TIME:
      return (T) * (uint32_t *) data;
    case ESP_ZB_ZCL_ATTR_TYPE_CLUSTER_ID:
      return (T) * (uint16_t *) data;
    case ESP_ZB_ZCL_ATTR_TYPE_ATTRIBUTE_ID:
      return (T) * (uint16_t *) data;
    case ESP_ZB_ZCL_ATTR_TYPE_BACNET_OID:
      return 0;
    case ESP_ZB_ZCL_ATTR_TYPE_IEEE_ADDR:
      return (T) * (uint64_t *) data;
    case ESP_ZB_ZCL_ATTR_TYPE_128_BIT_KEY:
      return 0;
    default:
      return *(T *) data;
  }
}

float get_r_from_xy(float x, float y);
float get_g_from_xy(float x, float y);
float get_b_from_xy(float x, float y);

}  // namespace zigbee
}  // namespace esphome
