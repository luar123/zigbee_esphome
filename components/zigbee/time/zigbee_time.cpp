#include "zigbee_time.h"
#include "esphome/core/log.h"

namespace esphome {
namespace zigbee {

void ZigbeeTime::send_timesync_request() {
  ESP_LOGD(TAG, "Requesting time from coordinator...");
  uint16_t attributes[] = {ESP_ZB_ZCL_ATTR_TIME_TIME_ID, ESP_ZB_ZCL_ATTR_TIME_TIME_STATUS_ID};
  esp_zb_zcl_read_attr_cmd_t read_req;
  read_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
  read_req.attr_field = attributes;
  read_req.attr_number = sizeof(attributes) / sizeof(uint16_t);
  read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TIME;
  read_req.zcl_basic_cmd.dst_endpoint = 1;
  read_req.zcl_basic_cmd.src_endpoint = 1;
  read_req.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000;  // coordinator
  if (esp_zb_lock_acquire(30 / portTICK_PERIOD_MS)) {
    esp_zb_zcl_read_attr_cmd_req(&read_req);
    esp_zb_lock_release();
    this->requested_ = true;
    ESP_LOGD(TAG, "Sent request");
  }
}

void ZigbeeTime::recieve_timesync_response(esp_zb_zcl_read_attr_resp_variable_t *variable) {
  uint32_t utc = 0;
  uint8_t sync_status = 0;
  while (variable != nullptr) {
    ESP_LOGD(TAG, "Read attribute response: status(%d), attribute(0x%x), type(0x%x), value(%d)", variable->status,
             variable->attribute.id, variable->attribute.data.type,
             variable->attribute.data.value ? *(uint32_t *) variable->attribute.data.value : 0);
    switch (variable->attribute.id) {
      case ESP_ZB_ZCL_ATTR_TIME_TIME_ID:
        utc = *(uint32_t *) variable->attribute.data.value;
        utc = utc + zigbee_time_offset;
        ESP_LOGD(TAG, "Recieved UTC time: %d", utc);
        break;
      case ESP_ZB_ZCL_ATTR_TIME_TIME_STATUS_ID:
        sync_status = *(uint8_t *) variable->attribute.data.value;
        ESP_LOGD(TAG, "Recieved sync status time: 0x%x", sync_status);
        break;
      default:
        ESP_LOGD(TAG, "Recieved other time property: not yet handled");
        break;
    }
    variable = variable->next;
  }
  if ((utc != 0) && (sync_status & 0x3 != 0)) { /* 0x3 = either Master or Syncronized bits set */
    this->set_utc_time(utc);
  } else {
    ESP_LOGD(TAG, "Did not recieve both time and status; clock NOT updated");
  }
}

void ZigbeeTime::setup() {
  ESP_LOGD(TAG, "Using Zigbee network as time source");
  this->synced_ = false;
  this->requested_ = false;
  this->zc_->zt_ = this;
}

void ZigbeeTime::update() {
  ESP_LOGD(TAG, "Updating time sync from Zigbee network...");
  this->synced_ = false;
}

void ZigbeeTime::loop() {
  if (this->synced_)
    return;
  if (this->requested_)
    return;
  if (zc_->is_connected()) {
    this->send_timesync_request();
  }
}

void ZigbeeTime::set_utc_time(uint32_t utc) {
  this->synchronize_epoch_(utc);
  this->synced_ = true;
  this->requested_ = false;
}

}  // namespace zigbee
}  // namespace esphome