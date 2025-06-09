# ESP32_Data_Logger

## Prerequisites:
- Hardware: ESP32, DS3231 RTC, BME680, VEML7700, MH-Z19B, PMSA003A, SCD41, SHT40, SGP41, AHT21, ENS160, DHT22, status LED, reset button.
- Libraries:
    * Arduino core for ESP32
    * FS, SPIFFS, Wire, ArduinoJson
    * WiFiManager (https://github.com/tzapu/WiFiManager)
    * PubSubClient (MQTT)
    * ESPmDNS, Update, WebServer, WiFiClient
    * RTClib
    * ezLED, EasyButton, TickTwo
    * BSEC (BME680), PMserial, Adafruit_VEML7700, MHZ19, SensirionI2cScd4x, SensirionI2CSgp41, SensirionI2cSht4x, VOCGasIndexAlgorithm, NOxGasIndexAlgorithm, Adafruit_AHTX0, ScioSense_ENS160, DHT
    * NTPClient, WiFiUdp

## Features:
- WiFi configuration via captive portal (WiFiManager) with persistent MQTT settings.
- MQTT publishing of sensor data (BME680, VEML7700, MH-Z19B, PMSA003A, SCD41, SHT40, SGP41, AHT21, ENS160, DHT22).
- OTA firmware update via web interface with authentication.
- RTC (DS3231) based periodic data acquisition and alarm scheduling.
- NTP time synchronization for RTC.
- Sensor initialization, error checking, and data acquisition.
- Status LED and WiFi reset button support.
- Automatic reconnection for WiFi and MQTT.
- Modular sensor reading and data publishing.
- Debug output via serial (optional).
