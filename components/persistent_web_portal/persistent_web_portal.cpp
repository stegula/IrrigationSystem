#include "persistent_web_portal.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <cstdio>
#include <sys/time.h>
#include <vector>

namespace esphome::persistent_web_portal {

static const char *const TAG = "persistent_web_portal";

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Irrigation Controller</title>
  <style>
    :root { color-scheme: light dark; font-family: system-ui, sans-serif; }
    body { margin: 0; background: #111827; color: #f9fafb; }
    main { width: min(720px, calc(100% - 32px)); margin: 28px auto; }
    h1 { margin-bottom: 4px; }
    .muted { color: #9ca3af; }
    section { background: #1f2937; border: 1px solid #374151; border-radius: 14px; padding: 18px; margin: 16px 0; }
    .status { display: grid; grid-template-columns: repeat(auto-fit,minmax(180px,1fr)); gap: 8px; }
    .relays { display: grid; grid-template-columns: repeat(auto-fit,minmax(140px,1fr)); gap: 12px; }
    .relay { border: 1px solid #4b5563; border-radius: 12px; padding: 14px; text-align: center; }
    button, select, input { box-sizing: border-box; width: 100%; min-height: 44px; border-radius: 9px; border: 1px solid #6b7280; padding: 9px 12px; font: inherit; }
    button { cursor: pointer; color: white; background: #2563eb; border-color: #2563eb; font-weight: 650; }
    button.off { background: #4b5563; border-color: #4b5563; }
    button.stop { background: #b91c1c; border-color: #b91c1c; }
    button:disabled { opacity: .55; cursor: wait; }
    label { display: block; margin: 12px 0 5px; }
    .row { display: grid; grid-template-columns: 1fr auto; gap: 10px; align-items: end; }
    .row button { width: auto; }
    .day-card { border: 1px solid #4b5563; border-radius: 10px; margin-top: 10px; }
    .day-card summary { cursor: pointer; padding: 12px; font-weight: 700; }
    .day-content { padding: 0 12px 12px; }
    .enable-row { display: flex; align-items: center; gap: 9px; }
    .enable-row input { width: auto; min-height: 0; }
    .timers { display: grid; grid-template-columns: repeat(auto-fit,minmax(210px,1fr)); gap: 0 14px; }
    .actions { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-top: 16px; }
    .message { min-height: 1.4em; margin-top: 10px; }
    .ok { color: #6ee7b7; }
    .error { color: #fca5a5; }
  </style>
</head>
<body>
<main>
  <h1>Irrigation Controller</h1>
  <p class="muted">Works directly through this device access point. Home Wi-Fi is optional.</p>

  <section>
    <h2>Zones</h2>
    <div class="relays">
      <div class="relay"><h3>Zone 1</h3><button id="r1" onclick="toggleRelay(1)">Loading...</button></div>
      <div class="relay"><h3>Zone 2</h3><button id="r2" onclick="toggleRelay(2)">Loading...</button></div>
      <div class="relay"><h3>Zone 3</h3><button id="r3" onclick="toggleRelay(3)">Loading...</button></div>
      <div class="relay"><h3>Zone 4</h3><button id="r4" onclick="toggleRelay(4)">Loading...</button></div>
    </div>
  </section>

  <section>
    <h2>Date and Time</h2>
    <div class="status">
      <div><strong>Current:</strong> <span id="current-time">Not set</span></div>
      <div><strong>Source:</strong> <span id="time-source">Unavailable</span></div>
    </div>
    <p class="muted">Time synchronizes automatically over the internet when home Wi-Fi is available. It can also be set manually for offline use.</p>
    <form id="time-form">
      <label for="manual-datetime">Manual current date and time</label>
      <div class="row">
        <input id="manual-datetime" type="datetime-local" step="1" required>
        <button type="submit" id="set-time">Set Time</button>
      </div>
      <div id="time-message" class="message muted"></div>
    </form>
  </section>

  <section>
    <h2>Irrigation Schedule</h2>
    <div><strong>Sequence:</strong> <span id="sequence-status">Idle</span></div>
    <form id="schedule-form">
      <p class="muted">Each day has its own start time and zone timers. Disable a day to prevent its schedule from running.</p>
      <div id="day-schedules"></div>
      <div class="actions">
        <button type="submit" id="save-schedule">Save Schedule</button>
        <button type="button" class="stop" id="stop-sequence" onclick="stopSequence()">Stop Irrigation</button>
      </div>
      <div id="schedule-message" class="message muted"></div>
    </form>
  </section>

  <section>
    <h2>Home Wi-Fi (optional)</h2>
    <div class="status">
      <div><strong>Status:</strong> <span id="wifi-status">Loading...</span></div>
      <div><strong>Home IP:</strong> <span id="sta-ip">—</span></div>
      <div><strong>Direct IP:</strong> <span id="ap-ip">192.168.4.1</span></div>
    </div>
    <form id="wifi-form">
      <label for="ssid">Network</label>
      <div class="row">
        <select id="ssid" required><option value="">Press Scan</option></select>
        <button type="button" id="scan" onclick="scanWifi()">Scan</button>
      </div>
      <label for="password">Password</label>
      <input id="password" type="password" maxlength="64" autocomplete="current-password" placeholder="Leave empty for an open network">
      <p><button type="submit" id="connect">Connect</button></p>
      <div id="wifi-message" class="message muted"></div>
    </form>
  </section>
</main>
<script>
let relayStates = [false, false, false, false];
let scheduleLoaded = false;
let timeInputInitialized = false;
const DAY_NAMES = ['Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday', 'Sunday'];

function buildDaySchedules() {
  const container = document.getElementById('day-schedules');
  DAY_NAMES.forEach((name, day) => {
    const details = document.createElement('details');
    details.className = 'day-card';
    details.open = day === 0;
    details.innerHTML = `<summary>${name}</summary>
      <div class="day-content">
        <label class="enable-row"><input id="day-${day}-enabled" type="checkbox">Enable ${name}</label>
        <label>Irrigation Start Time<input id="day-${day}-start" type="time" required></label>
        <div class="timers">
          ${[1, 2, 3, 4].map(zone => `<label>Irigation Timer Zone ${zone} (minutes)<input id="day-${day}-zone-${zone}" type="number" min="0" max="1440" step="1" value="0" required></label>`).join('')}
        </div>
      </div>`;
    container.appendChild(details);
  });
}

async function request(url, options) {
  const response = await fetch(url, options);
  const data = await response.json().catch(() => ({}));
  if (!response.ok) throw new Error(data.error || `Request failed (${response.status})`);
  return data;
}

async function refreshState() {
  try {
    const state = await request('/api/state');
    relayStates = state.relays;
    state.relays.forEach((on, index) => {
      const button = document.getElementById(`r${index + 1}`);
      button.textContent = on ? 'ON' : 'OFF';
      button.classList.toggle('off', !on);
    });
    document.getElementById('wifi-status').textContent = state.wifi.connected ? `Connected to ${state.wifi.ssid}` : 'Not connected';
    document.getElementById('sta-ip').textContent = state.wifi.sta_ip || '—';
    document.getElementById('ap-ip').textContent = state.wifi.ap_ip || '192.168.4.1';

    document.getElementById('current-time').textContent = state.time.valid ? state.time.datetime.replace('T', ' ') : 'Not set';
    document.getElementById('time-source').textContent = {
      network: 'Internet (SNTP)', manual: 'Manual', system: 'System clock', unavailable: 'Unavailable'
    }[state.time.source] || state.time.source;
    if (!timeInputInitialized) {
      document.getElementById('manual-datetime').value = state.time.valid ? state.time.datetime : browserDateTime();
      timeInputInitialized = true;
    }

    if (!scheduleLoaded) {
      state.schedule.days.forEach((profile, day) => {
        document.getElementById(`day-${day}-enabled`).checked = profile.enabled;
        document.getElementById(`day-${day}-start`).value = profile.start_time;
        profile.timers.forEach((minutes, zone) => {
          document.getElementById(`day-${day}-zone-${zone + 1}`).value = minutes;
        });
      });
      scheduleLoaded = true;
    }

    let sequenceText = 'Idle';
    if (state.schedule.phase === 'watering')
      sequenceText = `Zone ${state.schedule.active_zone} running — ${state.schedule.remaining_seconds}s remaining`;
    else if (state.schedule.phase === 'delay')
      sequenceText = `10-second delay — ${state.schedule.remaining_seconds}s remaining`;
    document.getElementById('sequence-status').textContent = sequenceText;
    document.getElementById('stop-sequence').disabled = state.schedule.phase === 'idle';
  } catch (_) {}
}

function browserDateTime() {
  const date = new Date();
  const pad = value => String(value).padStart(2, '0');
  return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}T${pad(date.getHours())}:${pad(date.getMinutes())}:${pad(date.getSeconds())}`;
}

async function toggleRelay(channel) {
  const button = document.getElementById(`r${channel}`);
  button.disabled = true;
  try {
    await request('/api/relay', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: new URLSearchParams({channel, state: relayStates[channel - 1] ? 'off' : 'on'})
    });
    await refreshState();
  } finally {
    button.disabled = false;
  }
}

async function scanWifi() {
  const button = document.getElementById('scan');
  const select = document.getElementById('ssid');
  const message = document.getElementById('wifi-message');
  button.disabled = true;
  select.innerHTML = '<option value="">Scanning...</option>';
  message.textContent = 'Scanning can briefly pause the direct connection.';
  message.className = 'message muted';
  try {
    let networks;
    for (;;) {
      const response = await fetch('/api/wifi/scan');
      const data = await response.json().catch(() => ({}));
      if (response.status === 202) {
        await new Promise(resolve => setTimeout(resolve, 500));
        continue;
      }
      if (!response.ok) throw new Error(data.error || `Scan failed (${response.status})`);
      networks = data;
      break;
    }
    select.innerHTML = '';
    if (!networks.length) select.innerHTML = '<option value="">No networks found</option>';
    networks.forEach(network => {
      const option = document.createElement('option');
      option.value = network.ssid;
      option.textContent = `${network.ssid} (${network.rssi} dBm${network.secure ? ', secured' : ', open'})`;
      select.appendChild(option);
    });
    message.textContent = `Found ${networks.length} network${networks.length === 1 ? '' : 's'}.`;
    message.className = 'message ok';
  } catch (error) {
    select.innerHTML = '<option value="">Scan failed — try again</option>';
    message.textContent = error.message;
    message.className = 'message error';
  } finally {
    button.disabled = false;
  }
}

document.getElementById('time-form').addEventListener('submit', async event => {
  event.preventDefault();
  const button = document.getElementById('set-time');
  const message = document.getElementById('time-message');
  button.disabled = true;
  try {
    await request('/api/time', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: new URLSearchParams({datetime: document.getElementById('manual-datetime').value})
    });
    message.textContent = 'Device date and time updated.';
    message.className = 'message ok';
    await refreshState();
  } catch (error) {
    message.textContent = error.message;
    message.className = 'message error';
  } finally {
    button.disabled = false;
  }
});

document.getElementById('schedule-form').addEventListener('submit', async event => {
  event.preventDefault();
  const button = document.getElementById('save-schedule');
  const message = document.getElementById('schedule-message');
  const scheduleData = {};
  let enabledDays = 0;
  DAY_NAMES.forEach((_, day) => {
    const enabled = document.getElementById(`day-${day}-enabled`).checked;
    if (enabled) enabledDays++;
    scheduleData[`day_${day}_enabled`] = enabled ? '1' : '0';
    scheduleData[`day_${day}_start`] = document.getElementById(`day-${day}-start`).value;
    for (let zone = 1; zone <= 4; zone++)
      scheduleData[`day_${day}_zone_${zone}`] = document.getElementById(`day-${day}-zone-${zone}`).value;
  });
  button.disabled = true;
  try {
    await request('/api/schedule', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: new URLSearchParams(scheduleData)
    });
    message.textContent = enabledDays ? 'Daily schedules saved.' : 'Schedules saved with no enabled days.';
    message.className = 'message ok';
    scheduleLoaded = false;
    await refreshState();
  } catch (error) {
    message.textContent = error.message;
    message.className = 'message error';
  } finally {
    button.disabled = false;
  }
});

async function stopSequence() {
  const button = document.getElementById('stop-sequence');
  const message = document.getElementById('schedule-message');
  button.disabled = true;
  try {
    await request('/api/schedule/stop', {method: 'POST'});
    message.textContent = 'Irrigation stopped.';
    message.className = 'message ok';
    await refreshState();
  } catch (error) {
    message.textContent = error.message;
    message.className = 'message error';
  } finally {
    button.disabled = false;
  }
}

document.getElementById('wifi-form').addEventListener('submit', async event => {
  event.preventDefault();
  const button = document.getElementById('connect');
  const message = document.getElementById('wifi-message');
  button.disabled = true;
  try {
    await request('/api/wifi/connect', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: new URLSearchParams({
        ssid: document.getElementById('ssid').value,
        password: document.getElementById('password').value
      })
    });
    message.textContent = 'Connecting. This page may pause briefly; the direct access point will return automatically.';
    message.className = 'message ok';
  } catch (error) {
    message.textContent = error.message;
    message.className = 'message error';
  } finally {
    button.disabled = false;
  }
});

buildDaySchedules();
refreshState();
setInterval(refreshState, 1500);
</script>
</body>
</html>
)HTML";

void PersistentWebPortal::add_relay(switch_::Switch *relay) {
  if (this->relay_count_ < RELAY_COUNT) {
    this->relays_[this->relay_count_++] = relay;
  }
}

void PersistentWebPortal::setup() {
  if (this->wifi_ == nullptr || this->time_ == nullptr || this->relay_count_ != RELAY_COUNT) {
    ESP_LOGE(TAG, "Invalid configuration");
    this->mark_failed();
    return;
  }

  this->schedule_pref_ = global_preferences->make_preference<ScheduleSettings>(SETTINGS_KEY, true);
  this->load_settings_();
  if (this->time_->now().is_valid())
    this->time_source_ = TimeSource::SYSTEM;
  this->time_->add_on_time_sync_callback([this]() {
    this->time_source_ = TimeSource::NETWORK;
    ESP_LOGI(TAG, "Clock synchronized from the network");
  });

  this->wifi_->add_scan_results_listener(this);
  this->wifi_->add_connect_state_listener(this);
  this->last_ap_check_ = millis();
  this->start_dns_();

  this->server_.on("/", HTTP_GET, [this]() { this->send_index_(); });
  this->server_.on("/api/state", HTTP_GET, [this]() { this->handle_state_(); });
  this->server_.on("/api/relay", HTTP_POST, [this]() { this->handle_relay_(); });
  this->server_.on("/api/wifi/scan", HTTP_GET, [this]() { this->handle_wifi_scan_(); });
  this->server_.on("/api/wifi/connect", HTTP_POST, [this]() { this->handle_wifi_connect_(); });
  this->server_.on("/api/time", HTTP_POST, [this]() { this->handle_time_set_(); });
  this->server_.on("/api/schedule", HTTP_POST, [this]() { this->handle_schedule_save_(); });
  this->server_.on("/api/schedule/stop", HTTP_POST, [this]() { this->handle_schedule_stop_(); });
  this->server_.onNotFound([this]() { this->handle_not_found_(); });
  this->server_.begin();

  ESP_LOGI(TAG, "Web portal started at http://192.168.4.1/");
}

void PersistentWebPortal::loop() {
  this->dns_server_.processNextRequest();
  this->server_.handleClient();

  const uint32_t now = millis();
  if (this->restore_ap_requested_ || now - this->last_ap_check_ >= AP_CHECK_INTERVAL_MS) {
    this->restore_ap_requested_ = false;
    this->last_ap_check_ = now;
    this->ensure_ap_();
  }

  if (this->connect_requested_) {
    this->connect_requested_ = false;
    wifi::WiFiAP station;
    station.set_ssid(this->pending_ssid_);
    station.set_password(this->pending_password_);
    this->wifi_->set_sta(station);
    this->wifi_->start_connecting(station);
  }

  if (this->pending_credentials_ && this->wifi_->is_connected()) {
    char ssid_buffer[wifi::SSID_BUFFER_SIZE];
    String connected_ssid(this->wifi_->wifi_ssid_to(ssid_buffer));
    if (connected_ssid == this->pending_ssid_.c_str()) {
      this->wifi_->save_wifi_sta(this->pending_ssid_, this->pending_password_);
      this->pending_password_.clear();
      this->pending_credentials_ = false;
      ESP_LOGI(TAG, "New Wi-Fi credentials connected and saved");
    }
  }

  this->update_sequence_(now);
  if (now - this->last_scheduler_check_ >= SCHEDULER_CHECK_INTERVAL_MS) {
    this->last_scheduler_check_ = now;
    this->check_schedule_(now);
  }
}

void PersistentWebPortal::on_wifi_scan_results(const wifi::wifi_scan_vector_t<wifi::WiFiScanResult> &results) {
  std::vector<ScannedNetwork> networks;
  networks.reserve(results.size());

  for (const auto &result : results) {
    const StringRef ssid_ref = result.get_ssid();
    if (ssid_ref.empty() || result.get_is_hidden())
      continue;

    const std::string ssid(ssid_ref.c_str(), ssid_ref.size());
    auto duplicate = std::find_if(networks.begin(), networks.end(), [&ssid](const ScannedNetwork &network) {
      return network.ssid == ssid;
    });
    if (duplicate == networks.end()) {
      networks.push_back({ssid, result.get_rssi(), result.get_with_auth()});
    } else if (result.get_rssi() > duplicate->rssi) {
      duplicate->rssi = result.get_rssi();
      duplicate->secure = result.get_with_auth();
    }
  }

  std::sort(networks.begin(), networks.end(), [](const ScannedNetwork &left, const ScannedNetwork &right) {
    return left.rssi > right.rssi;
  });
  this->scan_results_ = std::move(networks);
  this->scan_in_progress_ = false;
  this->scan_results_ready_ = true;
}

void PersistentWebPortal::on_wifi_connect_state(StringRef ssid, std::span<const uint8_t, 6> bssid) {
  (void) ssid;
  (void) bssid;
  // ESPHome normally tears down its fallback AP after a station connection.
  // Restore AP+STA mode from the main loop after every connection transition.
  this->restore_ap_requested_ = true;
}

void PersistentWebPortal::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Persistent Web Portal:\n"
                "  URL: http://192.168.4.1/\n"
                "  Relays: %u",
                this->relay_count_);
}

void PersistentWebPortal::ensure_ap_() {
  wifi_mode_t mode;
  const esp_err_t get_mode_result = esp_wifi_get_mode(&mode);
  if (get_mode_result != ESP_OK) {
    ESP_LOGW(TAG, "Could not read Wi-Fi mode (%d)", get_mode_result);
    return;
  }
  // Scanning requires the station interface. A device without saved home-Wi-Fi
  // credentials normally starts in AP-only mode, so promote every mode except
  // AP+STA to AP+STA while preserving the configured access point.
  if (mode == WIFI_MODE_APSTA)
    return;

  const auto ap = this->wifi_->get_ap();
  if (ap.get_ssid().empty()) {
    ESP_LOGE(TAG, "Cannot restore AP without an SSID");
    return;
  }

  const esp_err_t set_mode_result = esp_wifi_set_mode(WIFI_MODE_APSTA);
  if (set_mode_result != ESP_OK) {
    ESP_LOGW(TAG, "Could not enable persistent AP+STA mode (%d)", set_mode_result);
    return;
  }

  this->start_dns_();
  // ESPHome disables the fallback AP after STA connects. Although changing the
  // radio back to AP+STA brings back its SSID and DHCP interface, an HTTP socket
  // created before that interface cycle can remain unreachable from AP clients.
  // Reopen the wildcard listener after the AP_START event has propagated.
  this->set_timeout("restart_web_services", 500, [this]() {
    this->server_.begin();
    this->start_dns_();
    ESP_LOGI(TAG, "Portal listeners restarted for the persistent access point");
  });
  ESP_LOGI(TAG, "Persistent access point restored");
}

void PersistentWebPortal::start_dns_() {
  this->dns_server_.stop();
  this->dns_server_.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));
}

void PersistentWebPortal::send_index_() {
  this->server_.sendHeader("Cache-Control", "no-store");
  this->server_.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void PersistentWebPortal::send_json_error_(int status, const char *message) {
  String response = F("{\"error\":");
  append_json_string_(response, String(message));
  response += '}';
  this->server_.send(status, "application/json", response);
}

void PersistentWebPortal::handle_state_() {
  this->server_.sendHeader("Cache-Control", "no-store");
  this->server_.send(200, "application/json", this->build_state_json_());
}

void PersistentWebPortal::handle_relay_() {
  if (!this->server_.hasArg("channel") || !this->server_.hasArg("state")) {
    this->send_json_error_(400, "channel and state are required");
    return;
  }

  const int channel = this->server_.arg("channel").toInt();
  if (channel < 1 || channel > RELAY_COUNT) {
    this->send_json_error_(400, "channel must be between 1 and 4");
    return;
  }

  const String requested_state = this->server_.arg("state");
  if (this->sequence_phase_ != SequencePhase::IDLE) {
    ESP_LOGI(TAG, "Manual zone control stopped the irrigation sequence");
    this->stop_sequence_(true);
  }
  auto *relay = this->relays_[channel - 1];
  if (requested_state == "on") {
    relay->turn_on();
  } else if (requested_state == "off") {
    relay->turn_off();
  } else {
    this->send_json_error_(400, "state must be on or off");
    return;
  }

  this->server_.send(200, "application/json", F("{\"ok\":true}"));
}

void PersistentWebPortal::handle_wifi_scan_() {
  if (this->scan_results_ready_) {
    this->scan_results_ready_ = false;

    String response;
    response.reserve(32 + this->scan_results_.size() * 64);
    response += '[';
    for (size_t i = 0; i < this->scan_results_.size(); i++) {
      if (i != 0)
        response += ',';
      response += F("{\"ssid\":");
      append_json_string_(response, String(this->scan_results_[i].ssid.c_str()));
      response += F(",\"rssi\":");
      response += this->scan_results_[i].rssi;
      response += F(",\"secure\":");
      response += this->scan_results_[i].secure ? F("true") : F("false");
      response += '}';
    }
    response += ']';

    this->server_.sendHeader("Cache-Control", "no-store");
    this->server_.send(200, "application/json", response);
    return;
  }

  if (this->scan_in_progress_) {
    this->server_.send(202, "application/json", F("{\"status\":\"scanning\"}"));
    return;
  }

  const uint32_t now = millis();
  if (this->last_scan_started_ != 0 && now - this->last_scan_started_ < SCAN_RETRY_INTERVAL_MS) {
    this->send_json_error_(429, "Please wait before starting another Wi-Fi scan");
    return;
  }

  this->ensure_ap_();
  wifi_scan_config_t scan_config{};
  scan_config.show_hidden = true;
  const esp_err_t scan_result = esp_wifi_scan_start(&scan_config, false);
  if (scan_result != ESP_OK) {
    ESP_LOGW(TAG, "Could not start Wi-Fi scan (%d)", scan_result);
    this->send_json_error_(503, "Wi-Fi scan could not start; try again shortly");
    return;
  }

  this->last_scan_started_ = now;
  this->scan_in_progress_ = true;
  this->server_.send(202, "application/json", F("{\"status\":\"scanning\"}"));
}

void PersistentWebPortal::handle_wifi_connect_() {
  if (!this->server_.hasArg("ssid") || !this->server_.hasArg("password")) {
    this->send_json_error_(400, "ssid and password are required");
    return;
  }

  const String ssid = this->server_.arg("ssid");
  const String password = this->server_.arg("password");
  if (ssid.isEmpty() || ssid.length() > 32) {
    this->send_json_error_(400, "SSID must contain 1 to 32 characters");
    return;
  }
  if (!is_valid_station_password_(password)) {
    this->send_json_error_(400, "Password must be empty, 8 to 63 characters, or a 64-digit hexadecimal key");
    return;
  }

  this->pending_ssid_ = ssid.c_str();
  this->pending_password_ = password.c_str();
  this->pending_credentials_ = true;
  this->connect_requested_ = true;

  this->server_.send(202, "application/json", F("{\"ok\":true,\"status\":\"connecting\"}"));
}

void PersistentWebPortal::handle_time_set_() {
  if (!this->server_.hasArg("datetime")) {
    this->send_json_error_(400, "datetime is required");
    return;
  }

  const String value = this->server_.arg("datetime");
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  const int parsed = sscanf(value.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second);
  if (parsed < 5) {
    this->send_json_error_(400, "Use a valid date and time");
    return;
  }

  ESPTime manual_time{};
  manual_time.year = year;
  manual_time.month = month;
  manual_time.day_of_month = day;
  manual_time.hour = hour;
  manual_time.minute = minute;
  manual_time.second = parsed >= 6 ? second : 0;
  if (!manual_time.fields_in_range(false, false) || year < 2019 || year > 2099) {
    this->send_json_error_(400, "Date or time is outside the supported range");
    return;
  }

  manual_time.recalc_timestamp_local();
  if (manual_time.timestamp < 1546300800) {
    this->send_json_error_(400, "Date or time is invalid");
    return;
  }

  struct timeval time_value {
    .tv_sec = manual_time.timestamp, .tv_usec = 0
  };
  if (settimeofday(&time_value, nullptr) != 0) {
    ESP_LOGW(TAG, "Could not set the manual clock");
    this->send_json_error_(500, "Could not set the device clock");
    return;
  }

  this->time_source_ = TimeSource::MANUAL;
  ESP_LOGI(TAG, "Clock set manually");
  this->server_.send(200, "application/json", F("{\"ok\":true}"));
}

void PersistentWebPortal::handle_schedule_save_() {
  ScheduleSettings updated{};
  updated.version = SETTINGS_VERSION;
  for (uint8_t day = 0; day < DAYS_COUNT; day++) {
    String prefix = F("day_");
    prefix += static_cast<unsigned int>(day);
    const String enabled_arg = prefix + F("_enabled");
    const String start_arg = prefix + F("_start");
    if (!this->server_.hasArg(enabled_arg) || !this->server_.hasArg(start_arg)) {
      this->send_json_error_(400, "Complete daily schedule data is required");
      return;
    }

    const String enabled_value = this->server_.arg(enabled_arg);
    if (enabled_value != "0" && enabled_value != "1") {
      this->send_json_error_(400, "Invalid daily enable setting");
      return;
    }
    updated.days[day].enabled = enabled_value == "1";

    int start_hour = -1;
    int start_minute = -1;
    const String start_time = this->server_.arg(start_arg);
    if (sscanf(start_time.c_str(), "%d:%d", &start_hour, &start_minute) != 2 || start_hour < 0 || start_hour > 23 ||
        start_minute < 0 || start_minute > 59) {
      this->send_json_error_(400, "Invalid irrigation start time");
      return;
    }
    updated.days[day].start_hour = start_hour;
    updated.days[day].start_minute = start_minute;

    for (uint8_t zone = 0; zone < RELAY_COUNT; zone++) {
      String timer_arg = prefix + F("_zone_");
      timer_arg += static_cast<unsigned int>(zone + 1);
      if (!this->server_.hasArg(timer_arg)) {
        this->send_json_error_(400, "Complete daily zone timers are required");
        return;
      }
      const long minutes = this->server_.arg(timer_arg).toInt();
      if (minutes < 0 || minutes > MAX_ZONE_MINUTES) {
        this->send_json_error_(400, "Zone timers must be between 0 and 1440 minutes");
        return;
      }
      updated.days[day].zone_minutes[zone] = minutes;
    }
  }

  this->stop_sequence_(true);
  this->settings_ = updated;

  if (!this->save_settings_()) {
    this->send_json_error_(500, "Could not save the schedule");
    return;
  }

  ESP_LOGI(TAG, "Irrigation schedule saved");
  this->server_.send(200, "application/json", F("{\"ok\":true}"));
}

void PersistentWebPortal::handle_schedule_stop_() {
  this->stop_sequence_(true);
  this->server_.send(200, "application/json", F("{\"ok\":true}"));
}

void PersistentWebPortal::load_settings_() {
  ScheduleSettings saved{};
  bool valid = this->schedule_pref_.load(&saved) && saved.version == SETTINGS_VERSION;
  if (valid) {
    for (uint8_t day = 0; day < DAYS_COUNT && valid; day++) {
      if (saved.days[day].enabled > 1 || saved.days[day].start_hour >= 24 || saved.days[day].start_minute >= 60) {
        valid = false;
        break;
      }
      for (uint8_t zone = 0; zone < RELAY_COUNT; zone++)
        if (saved.days[day].zone_minutes[zone] > MAX_ZONE_MINUTES) {
          valid = false;
          break;
        }
    }
  }

  if (valid) {
    this->settings_ = saved;
    return;
  }

  this->settings_ = {};
  this->settings_.version = SETTINGS_VERSION;
  for (uint8_t day = 0; day < DAYS_COUNT; day++) {
    this->settings_.days[day].start_hour = 6;
    this->settings_.days[day].start_minute = 0;
  }

  auto legacy_pref = global_preferences->make_preference<LegacyScheduleSettings>(LEGACY_SETTINGS_KEY, true);
  LegacyScheduleSettings legacy{};
  bool legacy_valid = legacy_pref.load(&legacy) && legacy.version == 1 && legacy.days_mask <= 0x7F &&
                      legacy.start_hour < 24 && legacy.start_minute < 60;
  for (uint8_t zone = 0; zone < RELAY_COUNT && legacy_valid; zone++)
    legacy_valid = legacy.zone_minutes[zone] <= MAX_ZONE_MINUTES;
  if (!legacy_valid)
    return;

  for (uint8_t day = 0; day < DAYS_COUNT; day++) {
    this->settings_.days[day].enabled = (legacy.days_mask & (1U << day)) != 0;
    this->settings_.days[day].start_hour = legacy.start_hour;
    this->settings_.days[day].start_minute = legacy.start_minute;
    for (uint8_t zone = 0; zone < RELAY_COUNT; zone++)
      this->settings_.days[day].zone_minutes[zone] = legacy.zone_minutes[zone];
  }
  this->settings_.last_run_date = legacy.last_run_date;
  if (this->save_settings_())
    ESP_LOGI(TAG, "Migrated the shared schedule to independent daily schedules");
  else
    ESP_LOGW(TAG, "Could not persist the migrated daily schedules");
}

bool PersistentWebPortal::save_settings_() { return this->schedule_pref_.save(&this->settings_); }

void PersistentWebPortal::check_schedule_(uint32_t now_ms) {
  if (this->sequence_phase_ != SequencePhase::IDLE)
    return;

  const ESPTime current = this->time_->now();
  if (!current.is_valid())
    return;
  const uint8_t day_index = weekday_index_(current.day_of_week);
  if (day_index >= DAYS_COUNT)
    return;
  const DaySchedule &today = this->settings_.days[day_index];
  if (!today.enabled || current.hour != today.start_hour || current.minute != today.start_minute)
    return;

  bool has_runtime = false;
  for (uint8_t i = 0; i < RELAY_COUNT; i++)
    has_runtime |= today.zone_minutes[i] != 0;
  if (!has_runtime)
    return;

  const uint32_t date_key = static_cast<uint32_t>(current.year) * 10000UL + current.month * 100UL + current.day_of_month;
  if (this->settings_.last_run_date == date_key)
    return;

  this->settings_.last_run_date = date_key;
  for (uint8_t zone = 0; zone < RELAY_COUNT; zone++)
    this->active_zone_minutes_[zone] = today.zone_minutes[zone];
  if (!this->save_settings_())
    ESP_LOGW(TAG, "Could not persist the last irrigation run date");
  this->start_sequence_(now_ms);
}

void PersistentWebPortal::update_sequence_(uint32_t now_ms) {
  if (this->sequence_phase_ == SequencePhase::IDLE || !deadline_reached_(now_ms, this->sequence_deadline_))
    return;

  if (this->sequence_phase_ == SequencePhase::RUNNING_ZONE) {
    if (this->active_zone_ < RELAY_COUNT)
      this->relays_[this->active_zone_]->turn_off();
    this->active_zone_ = NO_ZONE;

    if (this->has_pending_zone_()) {
      this->sequence_phase_ = SequencePhase::WAITING_GAP;
      this->sequence_deadline_ = now_ms + ZONE_GAP_MS;
      ESP_LOGI(TAG, "Waiting 10 seconds before the next zone");
    } else {
      this->finish_sequence_();
    }
    return;
  }

  if (this->sequence_phase_ == SequencePhase::WAITING_GAP)
    this->start_next_zone_(now_ms);
}

void PersistentWebPortal::start_sequence_(uint32_t now_ms) {
  this->turn_all_zones_off_();
  this->sequence_phase_ = SequencePhase::WAITING_GAP;
  this->active_zone_ = NO_ZONE;
  this->next_zone_index_ = 0;
  ESP_LOGI(TAG, "Starting scheduled irrigation sequence");
  this->start_next_zone_(now_ms);
}

void PersistentWebPortal::start_next_zone_(uint32_t now_ms) {
  while (this->next_zone_index_ < RELAY_COUNT && this->active_zone_minutes_[this->next_zone_index_] == 0)
    this->next_zone_index_++;

  if (this->next_zone_index_ >= RELAY_COUNT) {
    this->finish_sequence_();
    return;
  }

  const uint8_t zone = this->next_zone_index_++;
  const uint16_t minutes = this->active_zone_minutes_[zone];
  this->relays_[zone]->turn_on();
  this->active_zone_ = zone;
  this->sequence_phase_ = SequencePhase::RUNNING_ZONE;
  this->sequence_deadline_ = now_ms + static_cast<uint32_t>(minutes) * 60000UL;
  ESP_LOGI(TAG, "Zone %u started for %u minute(s)", zone + 1, minutes);
}

void PersistentWebPortal::finish_sequence_() {
  this->turn_all_zones_off_();
  this->sequence_phase_ = SequencePhase::IDLE;
  this->active_zone_ = NO_ZONE;
  this->next_zone_index_ = 0;
  this->sequence_deadline_ = 0;
  this->active_zone_minutes_.fill(0);
  ESP_LOGI(TAG, "Irrigation sequence finished");
}

void PersistentWebPortal::stop_sequence_(bool turn_off_relays) {
  const bool was_active = this->sequence_phase_ != SequencePhase::IDLE;
  if (turn_off_relays)
    this->turn_all_zones_off_();
  this->sequence_phase_ = SequencePhase::IDLE;
  this->active_zone_ = NO_ZONE;
  this->next_zone_index_ = 0;
  this->sequence_deadline_ = 0;
  this->active_zone_minutes_.fill(0);
  if (was_active)
    ESP_LOGI(TAG, "Irrigation sequence stopped");
}

void PersistentWebPortal::turn_all_zones_off_() {
  for (auto *relay : this->relays_)
    relay->turn_off();
}

bool PersistentWebPortal::has_pending_zone_() const {
  for (uint8_t i = this->next_zone_index_; i < RELAY_COUNT; i++) {
    if (this->active_zone_minutes_[i] != 0)
      return true;
  }
  return false;
}

uint8_t PersistentWebPortal::weekday_index_(uint8_t day_of_week) {
  if (day_of_week < 1 || day_of_week > 7)
    return DAYS_COUNT;
  return day_of_week == 1 ? 6 : day_of_week - 2;
}

bool PersistentWebPortal::deadline_reached_(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

void PersistentWebPortal::handle_not_found_() {
  if (this->server_.method() == HTTP_GET) {
    this->server_.sendHeader("Location", "http://192.168.4.1/", true);
    this->server_.send(302, "text/plain", "");
    return;
  }
  this->send_json_error_(404, "not found");
}

String PersistentWebPortal::build_state_json_() const {
  String response;
  response.reserve(1400);
  response += F("{\"relays\":[");
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    if (i != 0)
      response += ',';
    response += this->relays_[i]->state ? F("true") : F("false");
  }
  response += F("],\"wifi\":{\"connected\":");
  const bool connected = this->wifi_->is_connected();
  response += connected ? F("true") : F("false");
  response += F(",\"ssid\":");
  char ssid_buffer[wifi::SSID_BUFFER_SIZE];
  append_json_string_(response, connected ? String(this->wifi_->wifi_ssid_to(ssid_buffer)) : String());
  response += F(",\"sta_ip\":");
  String station_ip;
  if (connected) {
    const auto addresses = this->wifi_->wifi_sta_ip_addresses();
    for (const auto &address : addresses) {
      if (address.is_set() && address.is_ip4()) {
        char address_buffer[network::IP_ADDRESS_BUFFER_SIZE];
        station_ip = address.str_to(address_buffer);
        break;
      }
    }
  }
  append_json_string_(response, station_ip);
  response += F(",\"ap_ip\":");
  append_json_string_(response, String(F("192.168.4.1")));
  response += F("},\"time\":{");
  ESPTime current = this->time_->now();
  response += F("\"valid\":");
  response += current.is_valid() ? F("true") : F("false");
  response += F(",\"datetime\":");
  char datetime_buffer[ESPTime::STRFTIME_BUFFER_SIZE]{};
  if (current.is_valid())
    current.strftime(datetime_buffer, sizeof(datetime_buffer), "%Y-%m-%dT%H:%M:%S");
  append_json_string_(response, String(datetime_buffer));
  response += F(",\"source\":");
  const char *source = "unavailable";
  switch (this->time_source_) {
    case TimeSource::NETWORK:
      source = "network";
      break;
    case TimeSource::MANUAL:
      source = "manual";
      break;
    case TimeSource::SYSTEM:
      source = "system";
      break;
    case TimeSource::NONE:
      break;
  }
  append_json_string_(response, String(source));
  response += F("},\"schedule\":{\"days\":[");
  for (uint8_t day = 0; day < DAYS_COUNT; day++) {
    if (day != 0)
      response += ',';
    response += F("{\"enabled\":");
    response += this->settings_.days[day].enabled ? F("true") : F("false");
    response += F(",\"start_time\":");
    char start_time_buffer[8];
    snprintf(start_time_buffer, sizeof(start_time_buffer), "%02u:%02u", this->settings_.days[day].start_hour,
             this->settings_.days[day].start_minute);
    append_json_string_(response, String(start_time_buffer));
    response += F(",\"timers\":[");
    for (uint8_t zone = 0; zone < RELAY_COUNT; zone++) {
      if (zone != 0)
        response += ',';
      response += this->settings_.days[day].zone_minutes[zone];
    }
    response += F("]}");
  }
  response += F("],\"phase\":");
  const char *phase = "idle";
  if (this->sequence_phase_ == SequencePhase::RUNNING_ZONE)
    phase = "watering";
  else if (this->sequence_phase_ == SequencePhase::WAITING_GAP)
    phase = "delay";
  append_json_string_(response, String(phase));
  response += F(",\"active_zone\":");
  response += this->active_zone_ < RELAY_COUNT ? this->active_zone_ + 1 : 0;
  response += F(",\"remaining_seconds\":");
  uint32_t remaining_seconds = 0;
  if (this->sequence_phase_ != SequencePhase::IDLE) {
    const int32_t remaining_ms = static_cast<int32_t>(this->sequence_deadline_ - millis());
    if (remaining_ms > 0)
      remaining_seconds = (static_cast<uint32_t>(remaining_ms) + 999) / 1000;
  }
  response += remaining_seconds;
  response += F("}}");
  return response;
}

bool PersistentWebPortal::is_valid_station_password_(const String &password) {
  const size_t length = password.length();
  if (length == 0 || (length >= 8 && length <= 63))
    return true;
  if (length != 64)
    return false;

  for (size_t i = 0; i < length; i++) {
    const char character = password[i];
    if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f') ||
          (character >= 'A' && character <= 'F')))
      return false;
  }
  return true;
}

void PersistentWebPortal::append_json_string_(String &output, const String &value) {
  output += '"';
  for (size_t i = 0; i < value.length(); i++) {
    const char character = value[i];
    switch (character) {
      case '"':
        output += F("\\\"");
        break;
      case '\\':
        output += F("\\\\");
        break;
      case '\n':
        output += F("\\n");
        break;
      case '\r':
        output += F("\\r");
        break;
      case '\t':
        output += F("\\t");
        break;
      default:
        if (static_cast<uint8_t>(character) >= 0x20)
          output += character;
        break;
    }
  }
  output += '"';
}

}  // namespace esphome::persistent_web_portal
