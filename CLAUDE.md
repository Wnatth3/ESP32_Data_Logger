# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Role

You are an expert embedded systems developer specializing in Arduino framework and Espressif32 platform.

## Constraints & Preferences:

- If the file already exists, use append or edit mode, not overwrite.
- Use non-blocking code, not `delay()`.
- Optimize for low memory usage.
- Avoid unnecessary libraries.
- Implement classes to creating templates to group related data and functions into categories.
- Use modular functions (separate logic clearly).
- Include serial debug output (baud rate: 115200).
- Compatible with both active low and high relay module.
- A payload sent to the MQTT Broker is in JSON format.
- Configurable pins, thresholds, and values are stored in the Config.h file.
- Synchronize across the ESP32 board, the dashboard, and the home assistant UI.
- Save statuses and configurable values using Preference library.
- In the event of a power outage, the device will be able to resume operation continuously.
- Add brief comments of one to three lines above the function in the main.cpp and *.h to explain what the function does.
- Add a summary commment above the code in the main.cpp to explain what this project does.

- When writing code, use this skill, `/andrej-karpathy`.
- When answering quesitons, use this skill, `/i-have-adhd`.

## Build & Flash

PlatformIO project, single env `esp32doit-devkit-v1` (ESP32 DoIt DevKit v1, Arduino framework).

```bash
pio run                      # build
pio run -t upload            # build + flash over USB
pio device monitor           # serial @115200, esp32_exception_decoder filter
pio run -t clean
```

No tests exist (`test/` holds only the PlatformIO README stub). Verification is done on hardware via serial output.

OTA flashing: the firmware serves an update page at `http://weatherst.local/` (mDNS name = `deviceName`). Login `admin`/`admin` → `/ud` → upload the `.pio/build/esp32doit-devkit-v1/firmware.bin`. `POST /reset` reboots the device.

Formatting: `.clang-format` (Google base, 2-space indent, 80 col, left pointer alignment).

## Architecture

Everything runs in `src/main.cpp` (~1100 lines, single translation unit). `src/Debug.h` and `src/Ota.h` are the only headers; `include/` and `lib/` are empty stubs.

### Timing model — this is the core of the design

The device does **not** poll on `millis()`. A DS3231 RTC alarm drives everything:

1. `setupAlarm()` computes the next wall-clock boundary via `setMinMatch()` and arms DS3231 Alarm 1. The interval is chosen at compile time by `#define _5Min` / `_10Min` / `_15Min` (or `_20SecTest` for a fast 20-second test loop).
2. The alarm pulls SQW (GPIO 33) low → ISR `onRtcTrigger()` sets the `rtcTrigger` flag.
3. `loop()` sees the flag and calls `fetchData()`, which re-arms the alarm, then `readData()` + `sendData()`.

`checkMinMatch()` gates the actual read, because the alarm also fires on the SGP41 preheat boundary. `preheatTime` is `setMin - 2` (or 58 when `setMin == 0`).

Changing the sampling interval means editing the `#define` block near the top of `main.cpp` — `setMinMatch()` and `checkMinMatch()` both branch on it and must stay consistent.

### Cooperative tasks

`TickTwo` timers updated from `loop()`, no RTOS tasks:

- `tSgp41HeatingOn` / `tSgp41HeatingOff` — duty-cycles the SGP41 hotplate. `HeatingOn` runs `readSht40Sgp41()` every 500 ms during the 2-minute preheat window before each sample so the VOC/NOx gas-index algorithms have converged data; the two timers hand off to each other by comparing `rtc.now().minute()` against `setMin` / `preheatTime`. Disabled entirely under `_20SecTest`.
- `tWifiDisconnectedDetect` — 60 s WiFi watchdog.
- `tConnectMqtt` / `tReconnectMqtt` — MQTT state machine. `tConnectMqtt` pumps `mqtt.loop()` while connected; on disconnect it hands to `tReconnectMqtt` (3 s retries). After 3 failures it backs off to a 60 s `tConnectMqtt` interval.

### Config & provisioning

WiFiManager captive portal (AP `WeatherSt` / password `password`) collects WiFi plus four custom MQTT fields. Those, along with the DHCP-assigned IP/gw/sn/dns, are persisted as JSON to `/config.txt` on LittleFS and reloaded by `loadConfiguration()` at boot. `mqttParameter` is the flag that says a broker was ever configured — `mqttInit()` skips MQTT entirely when it is false.

Long-press (5 s) on GPIO 0 (`resetWifiBtPressed`) deletes `/config.txt`, calls `wifiManager.resetSettings()`, and reboots.

The `WiFiManagerParameter` declarations in `wifiManagerSetup()` must stay in that function above `autoConnect()` — WiFiManager blocks inside `autoConnect()` and reads them by pointer.

### Sensors and data path

Sensor globals are declared as flat module-level variables grouped by device; `readData()` fills them, `sendData()` serializes them. Bus assignments:

- I2C (`Wire`, default pins): BME680 (BSEC, addr 0x77), VEML7700, SCD41, SHT40, SGP41, AHT21, ENS160 (0x53)
- UART2 (`HardwareSerial(2)`, GPIO 16/17): MH-Z19B CO2
- SoftwareSerial (GPIO 18/19): PMSA003A particulate
- One-wire GPIO 32: DHT22
- GPIO 33: DS3231 SQW, GPIO 0: reset button, `LED_BUILTIN`: ezLED status

`sendData()` builds a JSON **array**, one element per sensor, each `{"measurement": <name>, "fields": {...}}` — this is InfluxDB line-protocol shape, consumed by the Node-RED flow and rendered by Grafana. It is published to topic `esp32/sensors/json` with a 1024-byte PubSubClient buffer and a 1100-byte serialization buffer. Adding fields means checking both sizes.

`#undef NO_ERROR` before `#define NO_ERROR 0` near the SCD41 block is deliberate — it collides with a Windows/ESP32 SDK macro.

### Debug output

All serial printing goes through the macros in `src/Debug.h` (`_deln`, `_deF`, `_deVarln`, …). They compile to nothing unless `#define _DEBUG_` is uncommented at the top of `main.cpp` — which is the default state, so a stock build prints nothing. Uncomment it before debugging on hardware. Never add bare `Serial.print` calls.

## Companion configs

- `Node-RED/`, `node-red_flow/` — flows that subscribe to `esp32/sensors/json` and write to InfluxDB
- `Grafana/`, `grafana_dashboard/` — dashboard JSON
- `ArduinoJson_Example/Send_JSON_Format_to_MQTT.txt` — reference for the published payload shape

These are exported artifacts, not built by PlatformIO; keep them in sync when the JSON schema in `sendData()` changes.
