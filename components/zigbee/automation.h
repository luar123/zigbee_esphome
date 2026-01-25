#pragma once

//#include <stdfloat> //deactive because not working with esp-idf 5.1.4

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/version.h"
#include "esphome/core/log.h"
#include "zigbee_attribute.h"
#include "zigbee.h"
#ifdef USE_LIGHT
#include "esphome/components/light/light_state.h"
#endif

namespace esphome {
namespace zigbee {

class ZigBeeJoinTrigger : public Trigger<> {
 public:
  explicit ZigBeeJoinTrigger(ZigBeeComponent *parent) {
    parent->add_on_join_callback([this]() { this->trigger(); });
  }
};

class ZigbeeIdentifyEffectTrigger : public Trigger<uint8_t, uint8_t> {
 public:
  explicit ZigbeeIdentifyEffectTrigger(ZigBeeComponent *parent) {
    parent->on_identify_effect_callback_.add(
        [this](uint8_t effect_id, uint8_t effect_variant) { this->trigger(effect_id, effect_variant); });
  }
};

class ZigbeeCustomCommandTrigger : public Trigger<uint16_t, uint8_t, uint16_t, void *> {
 public:
  explicit ZigbeeCustomCommandTrigger(ZigBeeComponent *parent) {
    parent->on_custom_command_callback_.add([this](uint16_t cluster_id, uint8_t command_id, uint16_t size, void *value) {
      this->trigger(cluster_id, command_id, size, value);
    });
  }
};

template<typename... Ts> class ResetZigbeeAction : public Action<Ts...>, public Parented<ZigBeeComponent> {
 public:
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2025, 11, 0)
  void play(const Ts &...x) override { this->parent_->reset(); }
#else
  void play(Ts... x) override { this->parent_->reset(); }
#endif
};

template<typename... Ts> class ReportAction : public Action<Ts...>, public Parented<ZigBeeComponent> {
 public:
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2025, 11, 0)
  void play(const Ts &...x) override { this->parent_->report(); }
#else
  void play(Ts... x) override { this->parent_->report(); }
#endif
};

template<typename... Ts> class ReportAttrAction : public Action<Ts...>, public Parented<ZigBeeAttribute> {
 public:
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2025, 11, 0)
  void play(const Ts &...x) override { this->parent_->report(); }
#else
  void play(Ts... x) override { this->parent_->report(); }
#endif
};

template<typename T, typename... Ts> class SetAttrAction : public Action<Ts...> {
 public:
  SetAttrAction(ZigBeeAttribute *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(T, value);

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2025, 11, 0)
  void play(const Ts &...x) override { this->parent_->set_attr(this->value_.value(x...)); }
#else
  void play(Ts... x) override { this->parent_->set_attr(this->value_.value(x...)); }
#endif

 protected:
  ZigBeeAttribute *parent_;
};

template<typename Ts> class ZigBeeOnValueTrigger : public Trigger<Ts>, public Component {
 public:
  explicit ZigBeeOnValueTrigger(ZigBeeAttribute *parent) : parent_(parent) {}
  void setup() override {
    this->parent_->add_on_value_callback([this](esp_zb_zcl_attribute_t attribute) { this->on_value_(attribute); });
  }

 protected:
  void on_value_(esp_zb_zcl_attribute_t attribute) {
    if (attribute.data.type == parent_->attr_type() && attribute.data.value) {
      this->trigger(get_value_by_type<Ts>(parent_->attr_type(), attribute.data.value));
    }
  }
  ZigBeeAttribute *parent_;
};

template<typename T> struct ZigBeeReportData {
  T value;
  esp_zb_zcl_addr_t src_address;
  uint8_t src_endpoint;
};

template<typename T> class ZigBeeOnReportTrigger : public Trigger<ZigBeeReportData<T>>, public Component {
 public:
  explicit ZigBeeOnReportTrigger(ZigBeeAttribute *parent) : parent_(parent) {}
  void setup() override {
    this->parent_->add_on_report_callback(
        [this](esp_zb_zcl_attribute_t attribute, esp_zb_zcl_addr_t src_address, uint8_t src_endpoint) {
          this->on_report_(attribute, src_address, src_endpoint);
        });
  }

 protected:
  void on_report_(esp_zb_zcl_attribute_t attribute, esp_zb_zcl_addr_t src_address, uint8_t src_endpoint) {
    if (attribute.data.type == parent_->attr_type() && attribute.data.value) {
      this->trigger(ZigBeeReportData<T>{
          .value = get_value_by_type<T>(parent_->attr_type(), attribute.data.value),
          .src_address = src_address,
          .src_endpoint = src_endpoint,
      });
    }
  }
  ZigBeeAttribute *parent_;
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

#ifdef USE_LIGHT
void set_light_color(uint8_t ep, light::LightCall *call, uint16_t value, bool is_x);
#endif

}  // namespace zigbee
}  // namespace esphome
