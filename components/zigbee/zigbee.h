#pragma once

#include <map>
#include <tuple>
#include <deque>

#include "esp_zigbee_core.h"
#include "zboss_api.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "zigbee_helpers.h"
#include "zigbee_time.h"

namespace esphome {
namespace zigbee {

static const char *const TAG = "zigbee";

using device_params_t = struct DeviceParamsS {
  esp_zb_ieee_addr_t ieee_addr;
  uint8_t endpoint;
  uint16_t short_addr;
};

using zdo_info_user_ctx_t = struct ZdoInfoCtxS {
  uint8_t endpoint;
  uint16_t short_addr;
};

using zb_device_params_t = struct zb_device_params_s {
  esp_zb_ieee_addr_t ieee_addr;
  uint8_t endpoint;
  uint16_t short_addr;
};

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE false /* enable the install code policy for security */
#define ED_AGING_TIMEOUT ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE 3000 /* 3000 millisecond */
#define ESP_ZB_PRIMARY_CHANNEL_MASK \
  ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK /* Zigbee primary channel mask use in the example */

#define ESP_ZB_DEFAULT_RADIO_CONFIG() \
  { .radio_mode = ZB_RADIO_MODE_NATIVE, }

#define ESP_ZB_DEFAULT_HOST_CONFIG() \
  { .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE, }

template<class T> T get_value_by_type(uint8_t attr_type, void *data);

class ZigBeeAttribute;
class ZigbeeTime;

class ZigBeeComponent : public Component {
 public:
  void setup() override;
  void dump_config() override;
  esp_err_t create_endpoint(uint8_t endpoint_id, esp_zb_ha_standard_devices_t device_id);
  void set_ident_time(uint8_t ident_time);
  void set_basic_cluster(std::string model, std::string manufacturer, std::string date, uint8_t power,
                         uint8_t app_version, uint8_t stack_version, uint8_t hw_version, std::string area,
                         uint8_t physical_env);
  void add_cluster(uint8_t endpoint_id, uint16_t cluster_id, uint8_t role);
  void create_default_cluster(uint8_t endpoint_id, esp_zb_ha_standard_devices_t device_id);

  template<typename T>
  void add_attr(ZigBeeAttribute *attr, uint8_t endpoint_id, uint16_t cluster_id, uint8_t role, uint16_t attr_id,
                uint8_t attr_type, uint8_t attr_access, T value_p);

  void set_report(uint8_t endpoint_id, uint16_t cluster_id, uint8_t role, uint16_t attr_id);
  void handle_attribute(esp_zb_device_cb_common_info_t info, esp_zb_zcl_attribute_t attribute);
  void searchBindings();
  static void bindingTableCb(const esp_zb_zdo_binding_table_info_t *table_info, void *user_ctx);

  void reset() {
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_factory_reset();
    esp_zb_lock_release();
  }
  void report();
  void set_timeref(ZigbeeTime* zt);

  void add_on_join_callback(std::function<void()> &&callback) { this->on_join_callback_.add(std::move(callback)); }

  bool is_started() { return this->started_; }
  bool connected = false;
  bool started_ = false;

  CallbackManager<void()> on_join_callback_{};
  std::deque<esp_zb_zcl_reporting_info_t> reporting_list;

 protected:
  esp_zb_attribute_list_t *create_ident_cluster_();
  esp_zb_attribute_list_t *create_basic_cluster_();
  std::map<uint8_t, esp_zb_ha_standard_devices_t> endpoint_list_;
  std::map<uint8_t, esp_zb_cluster_list_t *> cluster_list_;
  std::map<std::tuple<uint8_t, uint16_t, uint8_t>, esp_zb_attribute_list_t *> attribute_list_;
  std::map<std::tuple<uint8_t, uint16_t, uint8_t, uint16_t>, ZigBeeAttribute *> attributes_;
  esp_zb_nwk_device_type_t device_role_ = ESP_ZB_DEVICE_TYPE_ED;
  esp_zb_ep_list_t *esp_zb_ep_list_ = esp_zb_ep_list_create();
  struct {
    std::string model;
    std::string manufacturer;
    std::string date;
    uint8_t power;
    uint8_t app_version;
    uint8_t stack_version;
    uint8_t hw_version;
    std::string area;
    uint8_t physical_env;
  } basic_cluster_data_;
  uint8_t ident_time_;
};

extern "C" void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct);

template<typename T>
void ZigBeeComponent::add_attr(ZigBeeAttribute *attr, uint8_t endpoint_id, uint16_t cluster_id, uint8_t role,
                               uint16_t attr_id, uint8_t attr_type, uint8_t attr_access, T value_p) {
  esp_zb_attribute_list_t *attr_list = this->attribute_list_[{endpoint_id, cluster_id, role}];
  esp_err_t ret =
      esphome_zb_cluster_add_or_update_attr(cluster_id, attr_list, attr_id, attr_type, attr_access, &value_p);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Could not add attribute 0x%04X to cluster 0x%04X in endpoint %u: %s", attr_id, cluster_id,
             endpoint_id, esp_err_to_name(ret));
  }
  this->attributes_[{endpoint_id, cluster_id, role, attr_id}] = attr;
}

}  // namespace zigbee
}  // namespace esphome
