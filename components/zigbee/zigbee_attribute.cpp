#include "zigbee_attribute.h"

namespace esphome {
namespace zigbee {

void ZigBeeAttribute::set_attr(void *value_p) {
  if (!this->zb_->is_started()) {
    return;
  }
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_status_t state =
      esp_zb_zcl_set_attribute_val(this->endpoint_id_, this->cluster_id_, this->role_, this->attr_id_, value_p, false);

  // Check for error
  if (state != ESP_ZB_ZCL_STATUS_SUCCESS) {
    ESP_LOGE(TAG, "Setting attribute failed!");
    return;
  }
  ESP_LOGD(TAG, "Attribute set!");
  esp_zb_lock_release();
}

void ZigBeeAttribute::set_report() {
  this->zb_->set_report(this->endpoint_id_, this->cluster_id_, this->role_, this->attr_id_);
}

}  // namespace zigbee
}  // namespace esphome
