#pragma once

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

  template<typename T> void add_attr(uint8_t attr_access, T value_p);
  void set_report();
  template<typename T> void set_attr(T *value_p);

  uint8_t attr_type() { return attr_type_; }

  void add_on_value_callback(std::function<void(esp_zb_zcl_attribute_t attribute)> callback) {
    on_value_callback_.add(std::move(callback));
  }
  void on_value(esp_zb_zcl_attribute_t attribute) { this->on_value_callback_.call(attribute); }

#ifdef USE_SENSOR
  template<typename T> void connect(sensor::Sensor *sensor);
  template<typename T> void connect(sensor::Sensor *sensor, std::function<T(float)> &&f);
#endif
#ifdef USE_BINARY_SENSOR
  template<typename T> void connect(binary_sensor::BinarySensor *sensor);
  template<typename T> void connect(binary_sensor::BinarySensor *sensor, std::function<T(bool)> &&f);
#endif

 protected:
  void set_attr_();
  ZigBeeComponent *zb_;
  uint8_t endpoint_id_;
  uint16_t cluster_id_;
  uint8_t role_;
  uint16_t attr_id_;
  uint8_t attr_type_;
  float scale_;
  CallbackManager<void(esp_zb_zcl_attribute_t attribute)> on_value_callback_{};
  void *value_p{nullptr};
  bool set_attr_requested_{false};
};

template<typename T> void ZigBeeAttribute::add_attr(uint8_t attr_access, T value_p) {
  this->zb_->add_attr(this, this->endpoint_id_, this->cluster_id_, this->role_, this->attr_id_, this->attr_type_,
                      attr_access, value_p);
}

template<typename T> void ZigBeeAttribute::set_attr(T *value_p) {
  if (this->value_p != nullptr) {
    delete (T *) this->value_p;
  }
  this->value_p = (void *) value_p;
  this->set_attr_requested_ = true;
}

#ifdef USE_SENSOR
template<typename T> void ZigBeeAttribute::connect(sensor::Sensor *sensor) {
  sensor->add_on_state_callback([=, this](float value) {
    T *value_p = new T;
    *value_p = (T) (this->scale_ * value);
    this->set_attr(value_p);
  });
}

template<typename T> void ZigBeeAttribute::connect(sensor::Sensor *sensor, std::function<T(float)> &&f) {
  sensor->add_on_state_callback([=, this](float value) {
    T *value_p = new T;
    *value_p = f(value);
    this->set_attr(value_p);
  });
}
#endif

#ifdef USE_BINARY_SENSOR
template<typename T> void ZigBeeAttribute::connect(binary_sensor::BinarySensor *sensor) {
  sensor->add_on_state_callback([=, this](bool value) {
    T *value_p = new T;
    *value_p = (T) (this->scale_ * value);
    this->set_attr(value_p);
  });
}

template<typename T> void ZigBeeAttribute::connect(binary_sensor::BinarySensor *sensor, std::function<T(bool)> &&f) {
  sensor->add_on_state_callback([=, this](bool value) {
    T *value_p = new T;
    *value_p = f(value);
    this->set_attr(value_p);
  });
}
#endif

}  // namespace zigbee
}  // namespace esphome
