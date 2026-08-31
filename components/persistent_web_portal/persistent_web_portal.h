#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/component.h"

#include <DNSServer.h>
#include <WebServer.h>
#include <esp_wifi.h>

#include <array>
#include <span>
#include <string>
#include <vector>

namespace esphome::persistent_web_portal {

class PersistentWebPortal final : public Component,
                                  public wifi::WiFiScanResultsListener,
                                  public wifi::WiFiConnectStateListener {
 public:
  void set_wifi(wifi::WiFiComponent *wifi) { this->wifi_ = wifi; }
  void add_relay(switch_::Switch *relay);

  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_wifi_scan_results(const wifi::wifi_scan_vector_t<wifi::WiFiScanResult> &results) override;
  void on_wifi_connect_state(StringRef ssid, std::span<const uint8_t, 6> bssid) override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  static constexpr uint8_t RELAY_COUNT = 4;
  static constexpr uint32_t AP_CHECK_INTERVAL_MS = 1000;
  static constexpr uint32_t SCAN_RETRY_INTERVAL_MS = 5000;
  static constexpr uint32_t DNS_PORT = 53;

  struct ScannedNetwork {
    std::string ssid;
    int8_t rssi;
    bool secure;
  };

  void ensure_ap_();
  void start_dns_();
  void send_index_();
  void send_json_error_(int status, const char *message);
  void handle_state_();
  void handle_relay_();
  void handle_wifi_scan_();
  void handle_wifi_connect_();
  void handle_not_found_();
  String build_state_json_() const;
  static void append_json_string_(String &output, const String &value);
  static bool is_valid_station_password_(const String &password);

  wifi::WiFiComponent *wifi_{nullptr};
  std::array<switch_::Switch *, RELAY_COUNT> relays_{};
  uint8_t relay_count_{0};

  ::WebServer server_{80};
  DNSServer dns_server_;
  std::string pending_ssid_;
  std::string pending_password_;
  bool pending_credentials_{false};
  bool connect_requested_{false};
  bool restore_ap_requested_{false};
  bool scan_in_progress_{false};
  bool scan_results_ready_{false};
  std::vector<ScannedNetwork> scan_results_;
  uint32_t last_ap_check_{0};
  uint32_t last_scan_started_{0};
};

}  // namespace esphome::persistent_web_portal
