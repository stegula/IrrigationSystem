# IrrigationSystem ESPHome components

This repository contains the custom ESPHome component used by the irrigation controller.

## Persistent web portal

`persistent_web_portal` keeps the ESP32 access point available while the device is also connected to a home Wi-Fi network. Its bilingual English/Serbian Latin interface provides Wi-Fi scanning and setup, a control to forget the saved home network, four zone controls, a live rain-sensor status, automatic or manual time setup, and seven independent daily irrigation schedules. Every weekday has its own enable setting, start time, and four zone timers. The flag buttons at the top switch language immediately and remember the selection in the browser.

Import it directly from ESPHome:

```yaml
external_components:
  - source: github://stegula/IrrigationSystem@v0.7.0
    components:
      - persistent_web_portal
    refresh: 1h
```

The component requires a Wi-Fi component, an ESPHome real-time clock, a rain binary sensor, and four switch IDs. For a dry-contact rain sensor that closes to ground, configure GPIO27 with its internal pull-up and invert the input:

```yaml
time:
  - platform: sntp
    id: irrigation_time
    timezone: Europe/Belgrade

binary_sensor:
  - platform: gpio
    id: rain_sensor
    name: Rain Sensor
    device_class: moisture
    pin:
      number: GPIO27
      mode:
        input: true
        pullup: true
      inverted: true
    filters:
      - delayed_on: 250ms
      - delayed_off: 250ms

persistent_web_portal:
  wifi_id: device_wifi
  time_id: irrigation_time
  rain_sensor_id: rain_sensor
  relays:
    - relay_1
    - relay_2
    - relay_3
    - relay_4
```

The rain input is informational in `v0.7.0`; it is displayed on both the AP and home-network portal but does not alter irrigation operation.

Schedules are stored in ESP flash and run locally. When upgrading from `v0.3.0`, the shared schedule is copied into every weekday that was previously selected. The software clock continues without internet while the ESP32 remains powered; a full power loss requires SNTP or manual time setup unless external battery-backed RTC hardware is added.

The repository contains component source code only. Device passwords, API keys, and OTA credentials belong in the ESPHome device's local `secrets.yaml`.

## User manual

- [Korisničko uputstvo - srpski, latinica](docs/Korisnicko_uputstvo_Kontroler_navodnjavanja.docx)
