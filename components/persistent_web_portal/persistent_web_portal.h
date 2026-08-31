#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

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
  void set_time(time::RealTimeClock *time) { this->time_ = time; }
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
  static constexpr uint32_t SCHEDULER_CHECK_INTERVAL_MS = 250;
  static constexpr uint32_t ZONE_GAP_MS = 10000;
  static constexpr uint32_t DNS_PORT = 53;
  static constexpr uint16_t MAX_ZONE_MINUTES = 1440;
  static constexpr uint8_t SETTINGS_VERSION = 2;
  static constexpr uint8_t DAYS_COUNT = 7;
  static constexpr uint8_t NO_ZONE = 0xFF;
  static constexpr uint32_t LEGACY_SETTINGS_KEY = 0x49525247UL;
  static constexpr uint32_t SETTINGS_KEY = 0x49525248UL;

  enum class SequencePhase : uint8_t { IDLE, RUNNING_ZONE, WAITING_GAP };
  enum class TimeSource : uint8_t { NONE, SYSTEM, MANUAL, NETWORK };

  struct __attribute__((packed)) DaySchedule {
    uint8_t enabled;
    uint8_t start_hour;
    uint8_t start_minute;
    uint16_t zone_minutes[RELAY_COUNT];
  };

  struct __attribute__((packed)) ScheduleSettings {
    uint8_t version;
    DaySchedule days[DAYS_COUNT];
    uint32_t last_run_date;
  };

  struct __attribute__((packed)) LegacyScheduleSettings {
    uint8_t version;
    uint8_t days_mask;
    uint8_t start_hour;
    uint8_t start_minute;
    uint16_t zone_minutes[RELAY_COUNT];
    uint32_t last_run_date;
  };

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
  void handle_time_set_();
  void handle_schedule_save_();
  void handle_schedule_stop_();
  void handle_not_found_();
  void load_settings_();
  bool save_settings_();
  void check_schedule_(uint32_t now_ms);
  void update_sequence_(uint32_t now_ms);
  void start_sequence_(uint32_t now_ms);
  void start_next_zone_(uint32_t now_ms);
  void finish_sequence_();
  void stop_sequence_(bool turn_off_relays);
  void turn_all_zones_off_();
  bool has_pending_zone_() const;
  static uint8_t weekday_index_(uint8_t day_of_week);
  static bool deadline_reached_(uint32_t now, uint32_t deadline);
  String build_state_json_() const;
  static void append_json_string_(String &output, const String &value);
  static bool is_valid_station_password_(const String &password);

  wifi::WiFiComponent *wifi_{nullptr};
  time::RealTimeClock *time_{nullptr};
  std::array<switch_::Switch *, RELAY_COUNT> relays_{};
  uint8_t relay_count_{0};
  ESPPreferenceObject schedule_pref_;
  ScheduleSettings settings_{};

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
  SequencePhase sequence_phase_{SequencePhase::IDLE};
  TimeSource time_source_{TimeSource::NONE};
  uint8_t active_zone_{NO_ZONE};
  uint8_t next_zone_index_{0};
  std::array<uint16_t, RELAY_COUNT> active_zone_minutes_{};
  uint32_t sequence_deadline_{0};
  uint32_t last_ap_check_{0};
  uint32_t last_scan_started_{0};
  uint32_t last_scheduler_check_{0};
};

}  // namespace esphome::persistent_web_portal
