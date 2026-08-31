# IrrigationSystem ESPHome components

This repository contains the custom ESPHome component used by the irrigation controller.

## Persistent web portal

`persistent_web_portal` keeps the ESP32 access point available while the device is also connected to a home Wi-Fi network. Its local interface provides Wi-Fi scanning and setup plus four relay controls.

Import it directly from ESPHome:

```yaml
external_components:
  - source: github://stegula/IrrigationSystem@main
    components:
      - persistent_web_portal
    refresh: 1h
```

The repository contains component source code only. Device passwords, API keys, and OTA credentials belong in the ESPHome device's local `secrets.yaml`.
