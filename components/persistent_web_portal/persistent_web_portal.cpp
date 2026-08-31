#include "persistent_web_portal.h"

#include "esphome/core/log.h"

#include <algorithm>
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
    button:disabled { opacity: .55; cursor: wait; }
    label { display: block; margin: 12px 0 5px; }
    .row { display: grid; grid-template-columns: 1fr auto; gap: 10px; align-items: end; }
    .row button { width: auto; }
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
    <h2>Relays</h2>
    <div class="relays">
      <div class="relay"><h3>Relay 1</h3><button id="r1" onclick="toggleRelay(1)">Loading...</button></div>
      <div class="relay"><h3>Relay 2</h3><button id="r2" onclick="toggleRelay(2)">Loading...</button></div>
      <div class="relay"><h3>Relay 3</h3><button id="r3" onclick="toggleRelay(3)">Loading...</button></div>
      <div class="relay"><h3>Relay 4</h3><button id="r4" onclick="toggleRelay(4)">Loading...</button></div>
    </div>
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
  } catch (_) {}
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
  if (this->wifi_ == nullptr || this->relay_count_ != RELAY_COUNT) {
    ESP_LOGE(TAG, "Invalid configuration");
    this->mark_failed();
    return;
  }

  this->wifi_->add_scan_results_listener(this);
  this->wifi_->add_connect_state_listener(this);
  this->last_ap_check_ = millis();
  this->start_dns_();

  this->server_.on("/", HTTP_GET, [this]() { this->send_index_(); });
  this->server_.on("/api/state", HTTP_GET, [this]() { this->handle_state_(); });
  this->server_.on("/api/relay", HTTP_POST, [this]() { this->handle_relay_(); });
  this->server_.on("/api/wifi/scan", HTTP_GET, [this]() { this->handle_wifi_scan_(); });
  this->server_.on("/api/wifi/connect", HTTP_POST, [this]() { this->handle_wifi_connect_(); });
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
  response.reserve(220);
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
