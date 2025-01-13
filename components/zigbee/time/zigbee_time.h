#pragma once

#include "esphome/core/component.h"
#include "esphome/components/time/real_time_clock.h"
#include "../zigbee.h"

namespace esphome {
namespace zigbee {

static const uint32_t zigbee_time_offset = 946684800; /* Zigbee time is based on counting seconds from 1 Jan 2000 (=946684800) */

class ZigBeeComponent;

class ZigbeeTime : public time::RealTimeClock {
  public:
    ZigbeeTime(ZigBeeComponent* zc) : zc_(zc) {}
    void setup() override;
    void loop() override;
    void update() override;
    void set_utc_time(uint32_t utc);
    void send_timesync_request();
    void recieve_timesync_response(esp_zb_zcl_read_attr_resp_variable_t *variable);

  protected:
    ZigBeeComponent* zc_;
    bool synced_;
    bool requested_;
};

}  // namespace zigbee
}  // namespace esphome
