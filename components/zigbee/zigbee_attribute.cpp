#include "zigbee_attribute.h"

namespace esphome {
namespace zigbee {

void ZigBeeAttribute::set_attr_() {
  if (!this->zb_->is_connected()) {
    return;
  }
  if (esp_zigbee_lock_acquire(20 / portTICK_PERIOD_MS)) {
    ezb_zcl_status_t state = ezb_zcl_set_attr_value(this->endpoint_id_, this->cluster_id_, this->role_, this->attr_id_,
                                                    EZB_ZCL_STD_MANUF_CODE, this->value_p, false);
    if (this->force_report_) {
      this->report_(true);
    }
    this->set_attr_requested_ = false;
    // Check for error
    if (state != EZB_ZCL_STATUS_SUCCESS) {
      ESP_LOGE(TAG, "Setting attribute failed: %s", state);
    }
    ESP_LOGD(TAG, "Attribute set!");
    esp_zigbee_lock_release();
  }
}

void ZigBeeAttribute::report_() { this->report_(false); }

void ZigBeeAttribute::report_(bool has_lock) {
  if (!this->zb_->is_connected()) {
    return;
  }
  if (has_lock or esp_zigbee_lock_acquire(20 / portTICK_PERIOD_MS)) {
    ezb_zcl_report_attr_cmd_t cmd;
    cmd.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
    cmd.cmd_ctrl.fc.dis_default_rsp = 1;
    cmd.cmd_ctrl.dst_addr.addr_mode = EZB_ADDR_MODE_SHORT;
    cmd.cmd_ctrl.dst_addr.u.short_addr = 0x0000;
    cmd.cmd_ctrl.dst_ep = 1;
    cmd.cmd_ctrl.src_ep = this->endpoint_id_;
    cmd.cmd_ctrl.cluster_id = this->cluster_id_;
    cmd.payload.attr_id = this->attr_id_;

    ezb_zcl_report_attr_cmd_req(&cmd);
    this->report_requested_ = false;
    if (!has_lock) {
      esp_zigbee_lock_release();
    }
  }
}

ezb_zcl_reporting_info_t ZigBeeAttribute::get_reporting_info() {
  ezb_zcl_reporting_info_t reporting_info;  //= {
  //     .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
  //     .ep = this->endpoint_id_,
  //     .cluster_id = this->cluster_id_,
  //     .cluster_role = this->role_,
  //     .attr_id = this->attr_id_,
  //     .manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC,
  // };
  // reporting_info.dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  // reporting_info.u.send_info.min_interval = 10;     /*!< Actual minimum reporting interval */
  // reporting_info.u.send_info.max_interval = 0;      /*!< Actual maximum reporting interval */
  // reporting_info.u.send_info.def_min_interval = 10; /*!< Default minimum reporting interval */
  // reporting_info.u.send_info.def_max_interval = 0;  /*!< Default maximum reporting interval */
  // reporting_info.u.send_info.delta.s16 = 0;         /*!< Actual reportable change */

  return reporting_info;
}

void ZigBeeAttribute::set_report(bool force) {
  this->report_enabled = true;
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
