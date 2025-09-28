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
    if (this->force_report_) {
      this->report_(true);
    }
    this->set_attr_requested_ = false;
    // Check for error
    if (state != ESP_ZB_ZCL_STATUS_SUCCESS) {
      ESP_LOGE(TAG, "Setting attribute failed: %s", esp_err_to_name(state));
    }
    ESP_LOGD(TAG, "Attribute set!");
    esp_zb_lock_release();
  }
}

void ZigBeeAttribute::report_() { this->report_(false); }

void ZigBeeAttribute::report_(bool has_lock) {
  if (!this->zb_->is_started()) {
    return;
  }
  if (has_lock or esp_zb_lock_acquire(20 / portTICK_PERIOD_MS)) {
    esp_zb_zcl_report_attr_cmd_t cmd = {
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI,
    };
    cmd.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000;
    cmd.zcl_basic_cmd.dst_endpoint = 1;
    cmd.zcl_basic_cmd.src_endpoint = this->endpoint_id_;

    cmd.clusterID = this->cluster_id_;
    cmd.attributeID = this->attr_id_;

    // cmd.cluster_role = reporting_info.cluster_role;
    esp_zb_zcl_report_attr_cmd_req(&cmd);
    this->report_requested_ = false;
    if (!has_lock) {
      esp_zb_lock_release();
    }
  }
}

void ZigBeeAttribute::set_report(bool force) {
  esp_zb_zcl_reporting_info_t reporting_info = {
      .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
      .ep = this->endpoint_id_,
      .cluster_id = this->cluster_id_,
      .cluster_role = this->role_,
      .attr_id = this->attr_id_,
      .manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC,
  };

  // reporting_info.dst.short_addr = 0;
  // reporting_info.dst.endpoint = 1;
  reporting_info.dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  reporting_info.u.send_info.min_interval = 10;     /*!< Actual minimum reporting interval */
  reporting_info.u.send_info.max_interval = 0;      /*!< Actual maximum reporting interval */
  reporting_info.u.send_info.def_min_interval = 10; /*!< Default minimum reporting interval */
  reporting_info.u.send_info.def_max_interval = 0;  /*!< Default maximum reporting interval */
  reporting_info.u.send_info.delta.s16 = 0;         /*!< Actual reportable change */

  this->zb_->set_report(this, reporting_info);
  this->force_report_ = force;
}

void ZigBeeAttribute::report() {
  this->report_requested_ = true;
  this->enable_loop();
}

void ZigBeeAttribute::loop() {
  if (this->set_attr_requested_) {
    this->set_attr_();
  }

  if (this->report_requested_) {
    this->report_();
  }

  if (!this->report_requested_ && !this->set_attr_requested_) {
    this->disable_loop();
  }
}

}  // namespace zigbee
}  // namespace esphome
