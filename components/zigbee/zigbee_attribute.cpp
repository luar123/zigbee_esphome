#include "zigbee_attribute.h"

namespace esphome {
namespace zigbee {

void ZigBeeAttribute::set_attr_() {
  if (!this->zb_->is_started()) {
    return;
  }
  if (esp_zb_lock_acquire(20 / portTICK_PERIOD_MS)) {
    esp_zb_zcl_status_t state = esp_zb_zcl_set_attribute_val(this->endpoint_id_, this->cluster_id_, this->role_,
                                                             this->attr_id_, this->value_p, false);
    this->set_attr_requested_ = false;
    // Check for error
    if (state != ESP_ZB_ZCL_STATUS_SUCCESS) {
      ESP_LOGE(TAG, "Setting attribute failed: %s", esp_err_to_name(state));
    }
    ESP_LOGD(TAG, "Attribute set!");
    esp_zb_lock_release();
  }
}

/**void ZigBeeAttribute::set_attr(const std::string &str) {
  if (this->value_p != nullptr) {
    delete[](char *) this->value_p;
  }

  auto zcl_str = get_zcl_string(str.c_str(), str.size());
  this->value_p = (void *) zcl_str;
  this->set_attr_requested_ = true;
}**/

void ZigBeeAttribute::set_report() {
  this->zb_->set_report(this->endpoint_id_, this->cluster_id_, this->role_, this->attr_id_);
}

void ZigBeeAttribute::loop() {
  if (this->set_attr_requested_) {
    this->set_attr_();
  }
}

}  // namespace zigbee
}  // namespace esphome
