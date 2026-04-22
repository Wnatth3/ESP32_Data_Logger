# ESP32_Data_Logger

## Prerequisites:
- Hardware: ESP32, DS3231 RTC, BME680, VEML7700, MH-Z19B, PMSA003A, SCD41, SHT40, SGP41, AHT21, ENS160, DHT22, status LED, reset button.
- Libraries: Arduino core for ESP32, LittleFS, ArduinoJson, WiFiManager, PubSubClient, ESPmDNS, Update, WebServer, NTPClient, RTClib, ezLED, PMserial, Adafruit_VEML7700, MHZ19, SensirionI2cScd4x, SensirionI2CSgp41, SensirionI2cSht4x, VOCGasIndexAlgorithm, NOxGasIndexAlgorithm, Adafruit_AHTX0, ScioSense_ENS160, DHT, TickTwo, Button2.
- MQTT broker and WiFi network available.
- Proper wiring for all sensors and RTC, with correct I2C/SPI/UART pins.

## Features:
- Connects to WiFi using WiFiManager with optional static IP and MQTT credentials stored in LittleFS.
- Supports OTA firmware updates via web interface.
- Synchronizes time with NTP and maintains time with DS3231 RTC.
- Periodically reads environmental data from multiple sensors (temperature, humidity, pressure, gas, CO2, PM, light, VOC, NOx, AQI).
- Publishes sensor data as JSON to MQTT broker.
- Handles WiFi and MQTT reconnection automatically.
- Allows resetting WiFi/MQTT configuration via long-press button.
- Status LED indicates device state.
- Modular sensor reading and error handling.



