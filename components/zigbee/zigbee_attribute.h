#pragma once

#include <type_traits>

#include "esp_zigbee_core.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "zigbee.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome {
namespace zigbee {

class ZigBeeAttribute : public Component {
 public:
  ZigBeeAttribute(ZigBeeComponent *parent, uint8_t endpoint_id, uint16_t cluster_id, uint8_t role, uint16_t attr_id,
                  uint8_t attr_type, float scale)
      : zb_(parent),
        endpoint_id_(endpoint_id),
        cluster_id_(cluster_id),
        role_(role),
        attr_id_(attr_id),
        attr_type_(attr_type),
        scale_(scale) {}
  // void dump_config() override;
  void loop() override;

  template<typename T> void add_attr(uint8_t attr_access, uint8_t max_size, T value);
  void set_report(bool force);
  void report();
  template<typename T> void set_attr(const T &value);

  uint8_t attr_type() { return attr_type_; }

  void add_on_value_callback(std::function<void(esp_zb_zcl_attribute_t attribute)> callback) {
    on_value_callback_.add(std::move(callback));
  }
  void on_value(esp_zb_zcl_attribute_t attribute) { this->on_value_callback_.call(attribute); }

  void add_on_report_callback(
      std::function<void(esp_zb_zcl_attribute_t attribute, esp_zb_zcl_addr_t src_address, uint8_t src_endpoint)>
          callback) {
    on_report_callback_.add(std::move(callback));
  }
  void on_report(esp_zb_zcl_attribute_t attribute, esp_zb_zcl_addr_t src_address, uint8_t src_endpoint) {
    this->on_report_callback_.call(attribute, src_address, src_endpoint);
  }

#ifdef USE_SENSOR
  template<typename T> void connect(sensor::Sensor *sensor);
  template<typename T> void connect(sensor::Sensor *sensor, std::function<T(float)> &&f);
#endif
#ifdef USE_BINARY_SENSOR
  template<typename T> void connect(binary_sensor::BinarySensor *sensor);
  template<typename T> void connect(binary_sensor::BinarySensor *sensor, std::function<T(bool)> &&f);
#endif
#ifdef USE_TEXT_SENSOR
  template<typename T> void connect(text_sensor::TextSensor *sensor);
  template<typename T> void connect(text_sensor::TextSensor *sensor, std::function<T(std::string)> &&f);
#endif

 protected:
  void set_attr_();
  void report_();
  void report_(bool has_lock);
  ZigBeeComponent *zb_;
  uint8_t endpoint_id_;
  uint16_t cluster_id_;
  uint8_t role_;
  uint16_t attr_id_;
  uint8_t attr_type_;
  uint8_t max_size_;
  float scale_;
  CallbackManager<void(esp_zb_zcl_attribute_t attribute)> on_value_callback_{};
  CallbackManager<void(esp_zb_zcl_attribute_t attribute, esp_zb_zcl_addr_t src_address, uint8_t src_endpoint)>
      on_report_callback_{};
  void *value_p{nullptr};
  bool set_attr_requested_{false};
  bool report_requested_{false};
  bool force_report_{false};
};

template<typename T> void ZigBeeAttribute::add_attr(uint8_t attr_access, uint8_t max_size, T value) {
  this->max_size_ = max_size;
  this->zb_->add_attr(this, this->endpoint_id_, this->cluster_id_, this->role_, this->attr_id_, this->attr_type_,
                      attr_access, max_size, std::move(value));
}

template<typename T> void ZigBeeAttribute::set_attr(const T &value) {
  if constexpr (std::is_convertible<T, const char *>::value) {
    auto zcl_str = get_zcl_string(value, this->max_size_);

    if (this->value_p != nullptr) {
      delete[](char *) this->value_p;
    }
    this->value_p = (void *) zcl_str;
  } else if constexpr (std::is_same<T, std::string>::value) {
    auto zcl_str = get_zcl_string(value.c_str(), this->max_size_);

    if (this->value_p != nullptr) {
      delete[](char *) this->value_p;
    }
    this->value_p = (void *) zcl_str;
  } else {
    if (this->value_p != nullptr) {
      delete (T *) this->value_p;
    }
    T *value_p = new T;
    *value_p = value;
    this->value_p = (void *) value_p;
  }
  this->set_attr_requested_ = true;
}

#ifdef USE_SENSOR
template<typename T> void ZigBeeAttribute::connect(sensor::Sensor *sensor) {
  sensor->add_on_state_callback([=, this](float value) { this->set_attr((T) (this->scale_ * value)); });
}

template<typename T> void ZigBeeAttribute::connect(sensor::Sensor *sensor, std::function<T(float)> &&f) {
  sensor->add_on_state_callback([=, this](float value) { this->set_attr(f(value)); });
}
#endif

#ifdef USE_BINARY_SENSOR
template<typename T> void ZigBeeAttribute::connect(binary_sensor::BinarySensor *sensor) {
  sensor->add_on_state_callback([=, this](bool value) { this->set_attr((T) (this->scale_ * value)); });
}

template<typename T> void ZigBeeAttribute::connect(binary_sensor::BinarySensor *sensor, std::function<T(bool)> &&f) {
  sensor->add_on_state_callback([=, this](bool value) { this->set_attr(f(value)); });
}
#endif

#ifdef USE_TEXT_SENSOR
template<typename T> void ZigBeeAttribute::connect(text_sensor::TextSensor *sensor) {
  sensor->add_on_state_callback([=, this](std::string value) { this->set_attr((T) (value)); });
}

template<typename T> void ZigBeeAttribute::connect(text_sensor::TextSensor *sensor, std::function<T(std::string)> &&f) {
  sensor->add_on_state_callback([=, this](std::string value) { this->set_attr(f(value)); });
}
#endif

}  // namespace zigbee
}  // namespace esphome
