#include "zigbee_time.h"
#include "esphome/core/log.h"

namespace esphome {
namespace zigbee {

void ZigbeeTime::setup() {
  ESP_LOGCONFIG(TAG, "Using Zigbee network as time source");
  synced_ = false;
  requested_ = false;
  zc_->set_timeref(this);
}

void ZigbeeTime::update() {
  ESP_LOGCONFIG(TAG, "Updating time sync from Zigbee network...");
  synced_ = false;
  requested_ = false;
}

void ZigbeeTime::loop() {
  if (this->synced_) 
    return;
  if (this->requested_) 
    return;
  if(zc_->connected) {
    send_timesync_request();
    this->requested_ = true;
  }
} 

void ZigbeeTime::recieve_timesync_response(esp_zb_zcl_read_attr_resp_variable_t *variable) {
  while (variable) {
      ESP_LOGI(TAG, "Read attribute response: status(%d), attribute(0x%x), type(0x%x), value(%d)",
                  variable->status,
                  variable->attribute.id,
                  variable->attribute.data.type,
                  variable->attribute.data.value ? *(uint32_t *)variable->attribute.data.value : 0);
      switch (variable->attribute.id) {
        case ESP_ZB_ZCL_ATTR_TIME_TIME_ID:
          {
            uint32_t utc = *(uint32_t *)variable->attribute.data.value;
            utc = utc + ZIGBEE_TIME_OFFSET;
            ESP_LOGI(TAG, "Recieved UTC time: %d", utc);
              time::RealTimeClock::synchronize_epoch_(utc);
              this->synced_ = true;
            break;
          }
        default:
          ESP_LOGI(TAG, "Recieved other time property: not yet handled");
          break;
      }
      variable = variable->next;
  } 
}

void ZigbeeTime::send_timesync_request() {
	ESP_LOGI(TAG, "Requesting time from coordinator...");
  uint16_t attributes[] = 
  {
      ESP_ZB_ZCL_ATTR_TIME_TIME_ID,
  };
  esp_zb_zcl_read_attr_cmd_t read_req;
  read_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
  read_req.attr_field = attributes;
  read_req.attr_number = sizeof(attributes) / sizeof(uint16_t);
  read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TIME;
  read_req.zcl_basic_cmd.dst_endpoint = 1;
  read_req.zcl_basic_cmd.src_endpoint = 1;
  read_req.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000;  //coordinator
  esp_zb_zcl_read_attr_cmd_req(&read_req);
}

}  // namespace zigbee
}  // namespace esphome