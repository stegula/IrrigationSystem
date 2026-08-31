# IrrigationSystem ESPHome components

This repository contains the custom ESPHome component used by the irrigation controller.

## Persistent web portal

`persistent_web_portal` keeps the ESP32 access point available while the device is also connected to a home Wi-Fi network. Its local interface provides Wi-Fi scanning and setup, four zone controls, automatic or manual time setup, and seven independent daily irrigation schedules. Every weekday has its own enable setting, start time, and four zone timers.

Import it directly from ESPHome:

```yaml
external_components:
  - source: github://stegula/IrrigationSystem@v0.4.2
    components:
      - persistent_web_portal
    refresh: 1h
```

The component requires a Wi-Fi component, an ESPHome real-time clock, and four switch IDs. For automatic internet time synchronization, configure SNTP:

```yaml
time:
  - platform: sntp
    id: irrigation_time
    timezone: Europe/Belgrade

persistent_web_portal:
  wifi_id: device_wifi
  time_id: irrigation_time
  relays:
    - relay_1
    - relay_2
    - relay_3
    - relay_4
```

Schedules are stored in ESP flash and run locally. When upgrading from `v0.3.0`, the shared schedule is copied into every weekday that was previously selected. The software clock continues without internet while the ESP32 remains powered; a full power loss requires SNTP or manual time setup unless external battery-backed RTC hardware is added.

The repository contains component source code only. Device passwords, API keys, and OTA credentials belong in the ESPHome device's local `secrets.yaml`.
