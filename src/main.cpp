#include <Arduino.h>
#include <FS.h>  // Must be first
#include <LittleFS.h>
#include <ArduinoJson.h>
// NTP
#include <NTPClient.h>
#include <WiFiUdp.h>
//
#include <RTClib.h>
#include <Wire.h>
#include <ezLED.h>
#include <esp_sleep.h>
#include <TaskScheduler.h>

//******************************** Configuration ****************************//
#define _DEBUG_  // Comment this line to disable debug output
#include "Debug.h"

#define BATTERY_MODE  // Maximize battery life by sleeping between readings

// -- Read Sensors every 20s (test), 5, 10, or 15 minutes --
#define _20SecTest
// #define _5Min
// #define _10Min
// #define _15Min

//******************************** Sensor Classes ***************************//
#include "sensors/SensorBME680.h"
#include "sensors/SensorVEML7700.h"
#include "sensors/SensorMHZ19B.h"
#include "sensors/SensorPMSA003.h"
#include "sensors/SensorSCD41.h"
#include "sensors/SensorSHT40SGP41.h"
#include "sensors/SensorENS160AHT21.h"
#include "sensors/SensorDHT22.h"

//******************************** WiFi Manager Class ***********************//
#include "WifiManagerHandler.h"

//******************************** Defines **********************************//
#define DEVICE_NAME "WeatherSt"
#define MQTT_PUB_JSON "esp32/sensors/json"
#define SQW_PIN 33
#define LED_PIN LED_BUILTIN

static constexpr long     NTP_UTC_OFFSET_SEC        = 25200;   // GMT+7
static constexpr uint16_t JSON_BUFFER_SIZE          = 1100;
static constexpr uint32_t WIFI_CHECK_INTERVAL_MS    = 600000;  // 10 minutes
static constexpr uint32_t MQTT_RECONNECT_INTERVAL_MS = 3000;
static constexpr uint32_t LOW_POWER_CPU_MHZ         = 80;

//******************************** Sensor Objects ***************************//
SensorBME680 bme680;
SensorVEML7700 veml7700;
SensorMHZ19B mhz19b;    // rxPin=16, txPin=17 (defaults)
SensorPMSA003 pmsa003;  // rxPin=18, txPin=19 (defaults)
SensorSCD41 scd41;
SensorSHT40SGP41 sht40sgp41;
SensorENS160AHT21 ens160aht21;
SensorDHT22 dht22;  // pin=32 (default)

//******************************** WiFi / MQTT Object ***********************//
WifiManagerHandler wifiHandler(DEVICE_NAME);

//******************************** LED *************************************//
ezLED statusLed(LED_PIN);

//******************************** NTP / RTC ********************************//
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.google.com", NTP_UTC_OFFSET_SEC);
RTC_DS3231 rtc;

uint8_t tMin;
uint8_t setMin;
volatile bool rtcTrigger = false;

//******************************** Tasks ************************************//
Scheduler ts;

void wifiDisconnectedDetect();
Task tWifiDisconnected(WIFI_CHECK_INTERVAL_MS, TASK_FOREVER, &wifiDisconnectedDetect, &ts, false);

void taskConnectMqtt();
void taskReconnectMqtt();
Task tConnectMqtt(0, TASK_FOREVER, &taskConnectMqtt, &ts, false);
Task tReconnectMqtt(MQTT_RECONNECT_INTERVAL_MS, TASK_FOREVER, &taskReconnectMqtt, &ts, false);

//******************************** RTC / Time *******************************//
String strTime(DateTime t) {
  char buf[] = "YYYY MMM DD (DDD) hh:mm:ss";
  return t.toString(buf);
}

uint8_t setMinMatch(uint8_t a) {
  if (a >= 0 && a <= 59) {
#if defined(_5Min)
    return ((a / 5) + 1) * 5 % 60;
#elif defined(_10Min)
    return ((a / 10) + 1) * 10 % 60;
#elif defined(_15Min)
    return ((a / 15) + 1) * 15 % 60;
#endif
  }
  return 0;
}

uint8_t roundSec(uint8_t sec) {
  return sec > 60 ? sec - 60 : sec;
}

void syncRtc() {
  if (!rtc.begin()) _delnF("Couldn't find RTC!");
  timeClient.begin();
  timeClient.forceUpdate();
  if (timeClient.isTimeSet()) {
    rtc.adjust(DateTime(timeClient.getEpochTime()));
    _delnF("\nNTP sync succeeded.");
  } else {
    _delnF("\nNTP sync failed.");
  }
}

void setupAlarm() {
  if (rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  _deln("\n\t" + strTime(rtc.now()));

  rtc.disable32K();
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
  rtc.writeSqwPinMode(DS3231_OFF);
  rtc.disableAlarm(2);

#ifdef _20SecTest
  rtc.setAlarm1(rtc.now() + TimeSpan(0, 0, 0, 20), DS3231_A1_Second);
  _deF("Trigger next time: ");
  _deln(String(roundSec(rtc.now().second() + 20)) + "th sec.");
#else
  tMin   = rtc.now().minute();
  setMin = setMinMatch(tMin);
  rtc.setAlarm1(DateTime(2023, 2, 18, 0, setMin, 0), DS3231_A1_Minute);
  _deln("Trigger next time: " + String(setMin) + "th min.");
#endif
  _delnF("\tAlarm setting done.");
}

void IRAM_ATTR onRtcTrigger() {
  rtcTrigger = true;
}

#ifndef _20SecTest
bool checkMinMatch(int m) {
#if defined(_5Min)
  return m >= 0 && m % 5 == 0 && m < 60;
#elif defined(_10Min)
  return m >= 0 && m % 10 == 0 && m < 60;
#elif defined(_15Min)
  return m >= 0 && m % 15 == 0 && m < 60;
#else
  return false;
#endif
}
#endif

//******************************** Sleep ************************************//
void enterLowPowerSleep() {
#ifdef BATTERY_MODE
  _delnF("Entering deep sleep until RTC alarm...");
  wifiHandler.disconnect();
  statusLed.turnOFF();
  esp_sleep_enable_ext0_wakeup((gpio_num_t)SQW_PIN, 0);
  esp_deep_sleep_start();
#endif
}

//******************************** Task Functions ***************************//
void wifiDisconnectedDetect() {
  wifiHandler.reconnectIfNeeded();
}

void taskReconnectMqtt() {
  wifiHandler.mqttReconnect(tReconnectMqtt.getIterations(), statusLed, tReconnectMqtt, tConnectMqtt);
}

void taskConnectMqtt() {
  if (!wifiHandler.mqttConnected()) {
    tConnectMqtt.disable();
    tReconnectMqtt.enable();
  } else {
    wifiHandler.mqttLoop();
  }
}

//******************************** Read / Send Data *************************//
void readData() {
  bme680.read();
  veml7700.read();
  mhz19b.read();
  pmsa003.read();
  scd41.read();
  sht40sgp41.read();
  ens160aht21.read();
  dht22.read();

  ens160aht21.print();
  bme680.print();
  dht22.print();
  mhz19b.print();
  pmsa003.print();
  scd41.print();
  sht40sgp41.print();
  veml7700.print();

  _delnF("\tData reading done.");
}

void sendData() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();

  ens160aht21.addJsonAht21(arr);
  bme680.addJson(arr);
  dht22.addJson(arr);
  ens160aht21.addJsonEns160(arr);
  mhz19b.addJson(arr);
  pmsa003.addJson(arr);
  scd41.addJson(arr);
  sht40sgp41.addJsonSgp41(arr);
  sht40sgp41.addJsonSht40(arr);
  veml7700.addJson(arr);

  doc.shrinkToFit();
  char jsonBuffer[JSON_BUFFER_SIZE];
  serializeJson(doc, jsonBuffer);

  if (!wifiHandler.mqttConnectOnce()) {
    _delnF("MQTT publish skipped");
    return;
  }
  wifiHandler.mqttPublish(MQTT_PUB_JSON, jsonBuffer);
  _delnF("\nData sending done.");
}

void fetchData() {
  setupAlarm();

#ifdef _20SecTest
  readData();
  if (wifiHandler.connect()) {
    sendData();
    wifiHandler.disconnect();
  }
#else
  uint8_t nowMin = rtc.now().minute();
#if defined(_5Min)
  _deF("5min Match: ");
#elif defined(_10Min)
  _deF("10min Match: ");
#elif defined(_15Min)
  _deF("15min Match: ");
#endif
  _deln(checkMinMatch(nowMin) ? "true" : "false");
  if (checkMinMatch(nowMin)) {
    readData();
    if (wifiHandler.connect()) {
      sendData();
      wifiHandler.disconnect();
    }
  } else {
    _delnF("\tread data next time.");
  }
#endif
}

//******************************** Setup ************************************//
void setup() {
#ifdef BATTERY_MODE
  setCpuFrequencyMhz(LOW_POWER_CPU_MHZ);
#endif
  _serialBegin(115200);

  statusLed.turnOFF();
  Wire.begin();
  pinMode(SQW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SQW_PIN), onRtcTrigger, FALLING);

  while (!LittleFS.begin(true)) {
    _delnF("Failed to initialize LittleFS");
    delay(1000);
  }

  // ── Sensors ──────────────────────────────────────────────────────────────
  bme680.begin();
  veml7700.begin();
  mhz19b.begin();
  pmsa003.begin();
  scd41.begin();
  sht40sgp41.begin();
  ens160aht21.begin();
  dht22.begin();

  // ── WiFi / MQTT ───────────────────────────────────────────────────────────
  wifiHandler.begin(LittleFS);  // load config, run WiFiManager portal
  syncRtc();
  setupAlarm();
  wifiHandler.mqttInit(tConnectMqtt);  // start MQTT task if credentials exist

#ifndef BATTERY_MODE
  tWifiDisconnected.enable();
  tConnectMqtt.enable();
#endif
}

//******************************** Loop *************************************//
void loop() {
  ts.execute();
  statusLed.loop();

  if (rtcTrigger) {
    rtcTrigger = false;
    fetchData();
  }
#ifdef BATTERY_MODE
  else {
    enterLowPowerSleep();
  }
#endif
}
