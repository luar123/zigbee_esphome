#pragma once

//#include <stdfloat> //deactive because not working with esp-idf 5.1.4

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
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

template<typename... Ts> class ResetZigbeeAction : public Action<Ts...>, public Parented<ZigBeeComponent> {
 public:
  void play(Ts... x) override { this->parent_->reset(); }
};

template<typename... Ts> class ReportAction : public Action<Ts...>, public Parented<ZigBeeComponent> {
 public:
  void play(Ts... x) override { this->parent_->report(); }
};

template<typename... Ts> class ReportAttrAction : public Action<Ts...>, public Parented<ZigBeeAttribute> {
 public:
  void play(Ts... x) override { this->parent_->report(); }
};

template<typename T, typename... Ts> class SetAttrAction : public Action<Ts...> {
 public:
  SetAttrAction(ZigBeeAttribute *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(T, value);

  void play(Ts... x) override { this->parent_->set_attr(this->value_.value(x...)); }

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

void set_light_color(uint8_t ep, light::LightCall *call, uint16_t value, bool is_x);

}  // namespace zigbee
}  // namespace esphome
