# agent.md — ESP32 IoT Sensor Project

> This file provides context for AI coding agents (e.g., Claude, Copilot) working in this repository. Read it before making any changes.

---

## Project Overview & Architecture

This is an **ESP32-based IoT sensor project** built with the **Arduino framework**. The firmware reads data from one or more sensors, processes it locally, and transmits it to a remote endpoint (MQTT broker, HTTP API).

### High-Level Architecture

```
[Sensors] ──► [ESP32 Firmware] ──► [WiFi] ──► [Cloud / Broker / Server]
                    │
              [Local Storage]
                (littleFS)
```

### Key Responsibilities

- **Sensor acquisition** — periodic or interrupt-driven reading of physical sensors
- **Data processing** — unit conversion, filtering, threshold checks
- **Connectivity** — WiFi management, reconnection logic, OTA updates
- **Transmission** — publish sensor payloads via MQTT or HTTP POST
- **Power management** — deep sleep cycles where applicable

### Project Path

D:\Fluke\Google_Drive\Smart_Farm\IOT\VS_Code\ESP32_Data_Logger

### Directory Structure

```
project-root/
│   └── .pio/
│       └── lipdeps/     # Libraries
├── src/
│   ├── Debug.h          
│   └── main.cpp         # Entry point: setup() and loop()
├── platformio.ini       # Board and lib config
├── README.md
└── AGENTS.md            # ← You are here
```

---

## Pin & Hardware Configuration

All pin assignments live in **`include/config.h`**. Never hard-code GPIO numbers in `.cpp` files — always reference the named constants.

### Default Pin Map (edit to match your hardware)

| Constant              | GPIO | Purpose                        |
|-----------------------|------|--------------------------------|
| `PIN_SENSOR_DATA`     | 4    | 1-Wire / single-wire sensor bus |
| `PIN_I2C_SDA`         | 21   | I²C data                       |
| `PIN_I2C_SCL`         | 22   | I²C clock                      |
| `PIN_SPI_MOSI`        | 23   | SPI MOSI                       |
| `PIN_SPI_MISO`        | 19   | SPI MISO                       |
| `PIN_SPI_CLK`         | 18   | SPI clock                      |
| `PIN_SPI_CS`          | 5    | SPI chip select                |
| `PIN_LED_STATUS`      | 2    | Onboard / status LED           |
| `PIN_WAKEUP`          | 33   | External wakeup (deep sleep)   |
| `PIN_ANALOG_IN`       | 34   | ADC input (input-only GPIO)    |

### Hardware Notes

- **ADC caveats** — ADC2 pins (GPIO 0, 2, 4, 12–15, 25–27) are unavailable while WiFi is active. Use ADC1 pins (32–39) for sensor reads that overlap with network activity.
- **3.3 V logic only** — all GPIO are 3.3 V. Use level shifters for 5 V sensors.
- **GPIO 34–39 are input-only** — do not configure them as outputs.
- **Boot strapping pins** — avoid GPIO 0, 2, 12 for general I/O; they affect boot mode.
- **Pull-ups** — enable internal pull-ups via `pinMode(PIN, INPUT_PULLUP)` where the sensor requires it (e.g., 1-Wire, I²C without external resistors).

---

## Code Conventions & Style

### General Rules

- **Language standard:** C++17 (`-std=gnu++17` in `platformio.ini`)
- **File names:** `snake_case.cpp` / `snake_case.h`
- **Class names:** `PascalCase`
- **Functions & variables:** `camelCase`
- **Constants & macros:** `UPPER_SNAKE_CASE`
- **Indentation:** 2 spaces (no tabs)
- **Line length:** 100 characters max

### Arduino-Specific Conventions

```cpp
// ✅ Good — non-blocking loop using millis()
void loop() {
  unsigned long now = millis();
  if (now - lastReadMs >= READ_INTERVAL_MS) {
    lastReadMs = now;
    readSensors();
  }
}

// ❌ Bad — blocking delay() stalls all tasks
void loop() {
  delay(5000);
  readSensors();
}
```

- **Never use `delay()` in `loop()`** — use `millis()` timers or FreeRTOS tasks.
- **Prefer FreeRTOS tasks** for concurrent operations (sensor reads, network I/O).
- **Wrap Serial output** in a `DEBUG_PRINT` macro gated by a `DEBUG` build flag so logs are stripped in production builds.

```cpp
// include/config.h
#ifdef DEBUG
  #define LOG(msg)   Serial.println(msg)
  #define LOGF(...)  Serial.printf(__VA_ARGS__)
#else
  #define LOG(msg)
  #define LOGF(...)
#endif
```

### Sensor Driver Pattern

Each sensor gets its own class with a consistent interface:

```cpp
class MySensor {
public:
  bool begin();                  // Init hardware, return false on failure
  bool read(SensorData& out);    // Populate struct, return false on error
  const char* name() const;      // Human-readable sensor name
private:
  // hardware handle, calibration state, etc.
};
```

### Configuration

Runtime config (WiFi credentials, broker URL, device ID) is stored in **NVS** via `Preferences`. Compile-time defaults live in `config.h`. Do not commit real credentials — use a `secrets.h` (git-ignored) or environment variables injected at build time.

---

## Debugging & Testing Guide

### Serial Monitor

Default baud rate: **115200**

Connect with:
```bash
# PlatformIO
pio device monitor --baud 115200
```

### Common Issues & Fixes

| Symptom | Likely Cause | Fix |
|---|---|---|
| Reboot loop / `Guru Meditation` | Stack overflow or null pointer | Increase task stack size; add null checks |
| WiFi connects then drops | Weak signal or IP conflict | Add reconnect logic with exponential backoff |
| Sensor reads always `0` or `-1` | Wrong pin, missing pull-up, bad wiring | Verify `config.h` pin map; check with multimeter |
| ADC reads are noisy | ADC2 conflict with WiFi | Switch to ADC1 pin; add 100 nF decoupling cap |
| OTA update fails | Insufficient flash partition | Set partition scheme to `min_spiffs` or `huge_app` |
| `WDT reset` in loop | Blocking call or infinite loop | Yield with `vTaskDelay(1)` or `yield()` |

### Logging Levels

Use these prefixes in log messages for easy filtering:

```
[INFO]  Normal operational messages
[WARN]  Recoverable issues
[ERROR] Failures requiring attention
[DEBUG] Verbose trace (stripped in release builds)
```

Filter in terminal:
```bash
pio device monitor | grep "\[ERROR\]"
```

### Unit Testing (optional)

Use **Unity** via PlatformIO's native test runner for pure logic (parsers, unit converters, data models). Hardware-dependent code must be tested on-device.

```bash
pio test -e native        # run host-side unit tests
pio test -e esp32dev      # run on-device tests over serial
```

---

