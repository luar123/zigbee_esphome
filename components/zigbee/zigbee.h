#pragma once

#include <map>
#include <tuple>

#include "esphome/core/defines.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/lock_free_queue.h"
#include "esphome/core/event_pool.h"

#include "esp_zb_event.h"

#include "esp_zigbee.h"
#include "ezbee/zha.h"
#include "zigbee_helpers.h"

#ifdef USE_ZIGBEE_TIME
#include "time/zigbee_time.h"
#endif

namespace esphome {
namespace zigbee {

static const char *const TAG = "zigbee";
static constexpr uint8_t MAX_ZB_QUEUE_SIZE = 32;

using zb_device_params_t = struct DeviceParamsS {
  ezb_extaddr_t ieee_addr;
  uint8_t endpoint;
  uint16_t short_addr;
};

using zdo_info_user_ctx_t = struct ZdoInfoCtxS {
  uint8_t endpoint;
  uint16_t short_addr;
};

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE false /* enable the install code policy for security */
#define ED_AGING_TIMEOUT EZB_NWK_ED_TIMEOUT_64MIN
#define ED_KEEP_ALIVE 3000 /* 3000 millisecond */
#define MAX_CHILDREN 10
#define EZB_PRIMARY_CHANNEL_MASK (0x07FFF800U) /* Zigbee primary channel mask use in the example */
#define ESP_ZIGBEE_STORAGE_PARTITION_NAME "zb_storage"

#define EZB_DEFAULT_RADIO_CONFIG() \
  { .radio_mode = ESP_ZIGBEE_RADIO_MODE_NATIVE, }

template<class T> T get_value_by_type(uint8_t attr_type, void *data);
uint8_t *get_zcl_string(const char *str, uint8_t max_size, bool use_max_size = false);

class ZigBeeAttribute;
class ZigbeeTime;

class ZigBeeComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_basic_cluster(std::string model, std::string manufacturer, std::string date, uint8_t power,
                         uint8_t app_version, uint8_t stack_version, uint8_t hw_version, std::string area,
                         uint8_t physical_env);
  void set_trust_center_key(const char *trust_center_key);
  void set_device_version(uint8_t version) { this->device_version_ = version; }
  void add_cluster(uint8_t endpoint_id, uint16_t cluster_id, uint8_t role);
  void create_default_cluster(uint8_t endpoint_id, uint16_t device_id);

  template<typename T>
  void add_attr(ZigBeeAttribute *attr, uint8_t endpoint_id, uint16_t cluster_id, uint8_t role, uint16_t attr_id,
                uint8_t attr_type, uint8_t attr_access, uint8_t max_size, T value);

  template<typename T>
  void add_attr(uint8_t endpoint_id, uint16_t cluster_id, uint8_t role, uint16_t attr_id, uint8_t attr_type,
                uint8_t attr_access, uint8_t max_size, T value);

  static bool app_signal_handler(const ezb_app_signal_t *app_signal);
  void handle_attribute(ezb_zcl_message_info_t info, ezb_zcl_attribute_t attribute, uint8_t *current_level);
  void handle_report_attribute(uint8_t dst_endpoint, uint16_t cluster, ezb_zcl_report_attr_variable_t variables,
                               ezb_address_t src_address, uint8_t src_endpoint);
  void handle_read_attribute_response(ezb_zcl_message_info_t info, ezb_zcl_read_attr_rsp_variable_t *variables);
  void searchBindings();
  static void bindingTableCb(const ezb_zdo_nwk_mgmt_bind_req_result_t *table_info, void *user_ctx);
  static void esp_zigbee_alarm_bdb_commissioning(ezb_bdb_comm_mode_mask_t mode);

  void reset() {
    esp_zigbee_lock_acquire(portMAX_DELAY);
    esp_zigbee_factory_reset();
    esp_zigbee_lock_release();
  }
  void report();

#ifdef USE_ZIGBEE_TIME
  ZigbeeTime *zt_{nullptr};
#endif

  template<typename F> void add_on_join_callback(F &&callback) {
    this->on_join_callback_.add(std::forward<F>(callback));
  }

  bool is_started() { return this->started_; }
  bool is_connected() { return this->connected_; }
  bool connected_ = false;
  bool started_ = false;
  bool joined_ = false;

  CallbackManager<void()> on_join_callback_{};
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

 protected:
  template<typename... Args> friend void enqueue_zb_event(Args... args);
  esphome::LockFreeQueue<ZBEvent, MAX_ZB_QUEUE_SIZE> zb_events_;
  esphome::EventPool<ZBEvent, MAX_ZB_QUEUE_SIZE> zb_event_pool_;
  ezb_zcl_cluster_desc_t create_basic_cluster_();
  template<typename T>
  void add_attr_(ZigBeeAttribute *attr, uint8_t endpoint_id, uint16_t cluster_id, uint8_t role, uint16_t attr_id,
                 uint8_t attr_type, uint8_t attr_access, T *value_p);
  std::map<uint8_t, std::tuple<uint16_t, ezb_af_ep_desc_t>> endpoint_list_;
  // std::map<std::tuple<uint8_t, uint16_t, uint8_t>, ezb_zcl_cluster_desc_t> attribute_list_;
  std::map<std::tuple<uint8_t, uint16_t, uint8_t, uint16_t>, ZigBeeAttribute *> attributes_;
  ezb_af_device_desc_t dev_desc_ = ezb_af_create_device_desc();
  uint8_t ident_time_;
  bool custom_trust_center_key_ = false;
  uint8_t trustkey_[16] = {0};
  uint8_t device_version_ = 0;
#ifdef ZB_ED_ROLE
  ezb_nwk_device_type_t device_role_ = EZB_NWK_DEVICE_TYPE_END_DEVICE;
#else
  ezb_nwk_device_type_t device_role_ = EZB_NWK_DEVICE_TYPE_ROUTER;
#endif
};

template<typename T>
void ZigBeeComponent::add_attr(uint8_t endpoint_id, uint16_t cluster_id, uint8_t role, uint16_t attr_id,
                               uint8_t attr_type, uint8_t attr_access, uint8_t max_size, T value) {
  this->add_attr<T>(nullptr, endpoint_id, cluster_id, role, attr_id, attr_type, attr_access, max_size, value);
}

template<typename T>
void ZigBeeComponent::add_attr(ZigBeeAttribute *attr, uint8_t endpoint_id, uint16_t cluster_id, uint8_t role,
                               uint16_t attr_id, uint8_t attr_type, uint8_t attr_access, uint8_t max_size, T value) {
  // The size byte of the zcl_str must be set to the maximum value,
  // even though the initial string may be shorter.
  if constexpr (std::is_same<T, std::string>::value) {
    auto zcl_str = get_zcl_string(value.c_str(), max_size, true);
    add_attr_(attr, endpoint_id, cluster_id, role, attr_id, attr_type, attr_access, zcl_str);
    delete[] zcl_str;
  } else if constexpr (std::is_convertible<T, const char *>::value) {
    auto zcl_str = get_zcl_string(value, max_size, true);
    add_attr_(attr, endpoint_id, cluster_id, role, attr_id, attr_type, attr_access, zcl_str);
    delete[] zcl_str;
  } else {
    add_attr_(attr, endpoint_id, cluster_id, role, attr_id, attr_type, attr_access, &value);
  }
}

template<typename T>
void ZigBeeComponent::add_attr_(ZigBeeAttribute *attr, uint8_t endpoint_id, uint16_t cluster_id, uint8_t role,
                                uint16_t attr_id, uint8_t attr_type, uint8_t attr_access, T *value_p) {
  ezb_zcl_cluster_desc_t cluster_desc =
      ezb_af_endpoint_get_cluster_desc(std::get<1>(this->endpoint_list_[endpoint_id]), cluster_id, role);
  esp_err_t ret =
      esphome_zb_cluster_add_or_update_attr(cluster_id, cluster_desc, attr_id, attr_type, attr_access, value_p);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Could not add attribute 0x%04X to cluster 0x%04X in endpoint %u: %u", attr_id, cluster_id,
             endpoint_id, ret);
  }
  if (attr != nullptr) {
    this->attributes_[{endpoint_id, cluster_id, role, attr_id}] = attr;
  }
}

}  // namespace zigbee
}  // namespace esphome
