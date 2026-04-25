#include <Arduino.h>
#include <FS.h>  // Must be first
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
// OTA
#include <ESPmDNS.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFiClient.h>
// NTP
#include <NTPClient.h>
#include <WiFiUdp.h>
//
#include <RTClib.h>
#include <Wire.h>
#include <ezLED.h>
#include <esp_sleep.h>
#include <TaskScheduler.h>
#include <Button2.h>

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

//******************************** Sensor Objects ***************************//
SensorBME680 bme680;
SensorVEML7700 veml7700;
SensorMHZ19B mhz19b;    // rxPin=16, txPin=17 (defaults match wiring)
SensorPMSA003 pmsa003;  // rxPin=18, txPin=19 (defaults match wiring)
SensorSCD41 scd41;
SensorSHT40SGP41 sht40sgp41;
SensorENS160AHT21 ens160aht21;
SensorDHT22 dht22;  // pin=32 (default matches wiring)

//******************************** Variables & Objects **********************//
#define deviceName "WeatherSt"

//----------------- ezLED -------------------------//
#define led LED_BUILTIN
ezLED statusLed(led);

//----------------- Reset WiFi Button -------------//
#define resetWifiBtPin 0
Button2 resetWifiBt;

//----------------- WiFi Manager ------------------//
const char* filename = "/config.txt";

char static_ip[16]  = "192.168.0.191";
char static_gw[16]  = "192.168.0.1";
char static_sn[16]  = "255.255.255.0";
char static_dns[16] = "1.1.1.1";
// MQTT
char mqttBroker[16] = "192.168.0.10";
char mqttPort[6]    = "1883";
char mqttUser[16];
char mqttPass[16];

bool mqttParameter    = false;
bool shouldSaveConfig = false;

WiFiManager wifiManager;

//----------------- PubSubClient -----------------//
#define MQTT_PUB_JSON "esp32/sensors/json"

WiFiClient mqttClient;
PubSubClient mqtt(mqttClient);

//----------------- NTP Time ---------------------//
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.google.com", 25200 /*GMT+7*/);

//----------------- DS3231 RTC -------------------//
#define SQW_PIN 33
RTC_DS3231 rtc;

uint8_t tMin;
uint8_t setMin;
uint8_t preheatTime;
volatile bool rtcTrigger = false;

//******************************** Tasks ************************************//
Scheduler ts;

void sgp41HeatingOn();
void sgp41HeatingOff();
Task tSgp41HeatingOn(500, TASK_FOREVER, &sgp41HeatingOn, &ts, false);
Task tSgp41HeatingOff(0, TASK_FOREVER, &sgp41HeatingOff, &ts, false);

void wifiDisconnectedDetect();
Task tWifiDisconnectedDetect(600000, TASK_FOREVER, &wifiDisconnectedDetect, &ts, false);

void connectMqtt();
void reconnectMqtt();
Task tConnectMqtt(0, TASK_FOREVER, &connectMqtt, &ts, false);
Task tReconnectMqtt(3000, TASK_FOREVER, &reconnectMqtt, &ts, false);

//******************************** Functions ********************************//
//----------------- WiFi Manager --------------//
void loadConfiguration(fs::FS& fs, const char* filename) {
  _delnF("Loading configuration...");
  File file = fs.open(filename, "r");
  if (!file) {
    _delnF("Failed to open data file");
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, file))
    _delnF("Failed to read file, using default configuration");

  strlcpy(mqttBroker, doc["mqttBroker"], sizeof(mqttBroker));
  strlcpy(mqttPort, doc["mqttPort"], sizeof(mqttPort));
  strlcpy(mqttUser, doc["mqttUser"], sizeof(mqttUser));
  strlcpy(mqttPass, doc["mqttPass"], sizeof(mqttPass));
  mqttParameter = doc["mqttParameter"];

  if (doc["ip"]) {
    strlcpy(static_ip, doc["ip"], sizeof(static_ip));
    strlcpy(static_gw, doc["gateway"], sizeof(static_gw));
    strlcpy(static_sn, doc["subnet"], sizeof(static_sn));
    strlcpy(static_dns, doc["dns"], sizeof(static_dns));
  } else {
    _delnF("No custom IP in config file");
  }
  file.close();
}

void mqttInit() {
  _deF("MQTT parameters are ");
  if (mqttParameter) {
    _delnF(" available");
    mqtt.setBufferSize(1024);
    mqtt.setServer(mqttBroker, atoi(mqttPort));
    tConnectMqtt.enable();
  } else {
    _delnF(" not available.");
  }
}

void saveConfigCallback() {
  _delnF("Should save config");
  shouldSaveConfig = true;
}

void printFile(fs::FS& fs, const char* filename) {
  _delnF("Print config file...");
  File file = fs.open(filename, "r");
  if (!file) {
    _delnF("Failed to open data file");
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, file)) _delnF("Failed to read file");

  char buffer[512];
  serializeJsonPretty(doc, buffer);
  _deln(buffer);
  file.close();
}

void deleteFile(fs::FS& fs, const char* path) {
  _deVarln("Deleting file: ", path);
  if (fs.remove(path)) _delnF("- file deleted");
  else _delnF("- delete failed");
}

bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  _delnF("Connecting to WiFi...");
  if (!wifiManager.autoConnect(deviceName, "password")) {
    _delnF("WiFi connect failed");
    return false;
  }
  _delnF("WiFi connected");
  return true;
}

void disconnectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (mqtt.connected()) mqtt.disconnect();
    WiFi.disconnect(true, true);
  }
  WiFi.mode(WIFI_OFF);
  _delnF("WiFi powered off");
}

bool connectMqttOnce() {
  if (!mqttParameter) {
    _delnF("MQTT parameter not available");
    return false;
  }
  if (mqtt.connected()) return true;
  _deF("Connecting MQTT...");
  if (mqtt.connect(deviceName, mqttUser, mqttPass)) {
    _delnF("connected");
    return true;
  }
  _deVar("MQTT connect failed state: ", mqtt.state());
  _delnF("");
  return false;
}

void enterLowPowerSleep() {
#ifdef BATTERY_MODE
  _delnF("Entering light sleep until RTC alarm...");
  disconnectWiFi();
  statusLed.turnOFF();
  esp_sleep_enable_ext0_wakeup((gpio_num_t)SQW_PIN, 0);
  esp_deep_sleep_start();
  _delnF("Woke from sleep");
#endif
}

void wifiManagerSetup() {
  loadConfiguration(LittleFS, filename);
#ifdef _DEBUG_
  printFile(LittleFS, filename);
#endif
  WiFiManagerParameter customMqttBroker("broker", "mqtt server", mqttBroker, 16);
  WiFiManagerParameter customMqttPort("port", "mqtt port", mqttPort, 6);
  WiFiManagerParameter customMqttUser("user", "mqtt user", mqttUser, 10);
  WiFiManagerParameter customMqttPass("pass", "mqtt pass", mqttPass, 10);

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  IPAddress _ip, _gw, _sn, _dns;
  _ip.fromString(static_ip);
  _gw.fromString(static_gw);
  _sn.fromString(static_sn);
  _dns.fromString(static_dns);
  wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn, _dns);
  wifiManager.addParameter(&customMqttBroker);
  wifiManager.addParameter(&customMqttPort);
  wifiManager.addParameter(&customMqttUser);
  wifiManager.addParameter(&customMqttPass);

  wifiManager.setDarkMode(true);
#ifndef _DEBUG_
  wifiManager.setDebugOutput(true, WM_DEBUG_SILENT);
#endif

#ifndef BATTERY_MODE
  if (wifiManager.autoConnect(deviceName, "password"))
    _delnF("WiFI is connected :D");
  else
    _delnF("Configportal running");
#endif

  strcpy(mqttBroker, customMqttBroker.getValue());
  strcpy(mqttPort, customMqttPort.getValue());
  strcpy(mqttUser, customMqttUser.getValue());
  strcpy(mqttPass, customMqttPass.getValue());

  if (shouldSaveConfig) {
    File file = LittleFS.open(filename, "w");
    if (!file) {
      _delnF("Failed to open config file for writing");
      return;
    }

    JsonDocument doc;
    doc["mqttBroker"] = mqttBroker;
    doc["mqttPort"]   = mqttPort;
    doc["mqttUser"]   = mqttUser;
    doc["mqttPass"]   = mqttPass;

    if (doc["mqttBroker"] != "") {
      doc["mqttParameter"] = true;
      mqttParameter        = doc["mqttParameter"];
    }
    doc["ip"]      = WiFi.localIP().toString();
    doc["gateway"] = WiFi.gatewayIP().toString();
    doc["subnet"]  = WiFi.subnetMask().toString();
    doc["dns"]     = WiFi.dnsIP().toString();

    if (serializeJson(doc, file) == 0) _delnF("Failed to write to file");
    else _deVarln("Config saved to ", filename);
    file.close();
  }

  _deVar("ip: ", WiFi.localIP());
  _deVar(" | gw: ", WiFi.gatewayIP());
  _deVar(" | sn: ", WiFi.subnetMask());
  _deVarln(" | dns: ", WiFi.dnsIP());
}

void resetWifiBtPressed(Button2& btn) {
  statusLed.turnON();
  _delnF("Deleting the config file and resetting WiFi.");
  deleteFile(LittleFS, filename);
  wifiManager.resetSettings();
  _deF(deviceName);
  _delnF(" is restarting.");
  delay(3000);
  ESP.restart();
}

//----------------- RTC / Time ----------------//
String strTime(DateTime t) {
  char buff[] = "YYYY MMM DD (DDD) hh:mm:ss";
  return t.toString(buff);
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
    _delnF("\nSetup time from NTP server succeeded.");
  } else {
    _delnF("\nSetup time from NTP server failed.");
  }
}

void setupAlarm() {
  if (rtc.lostPower())
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

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
  tMin        = rtc.now().minute();
  setMin      = setMinMatch(tMin);
  preheatTime = (setMin == 0) ? 58 : setMin - 2;
  rtc.setAlarm1(DateTime(2023, 2, 18, 0, setMin, 0), DS3231_A1_Minute);
  _de("Preheat Time: " + String(preheatTime) + "th min.");
  _deln(" | Trigger next time: " + String(setMin) + "th min.");
#endif
  _delnF("\tAlarm setting done.");
}

void IRAM_ATTR onRtcTrigger() {
  rtcTrigger = true;
}

#ifndef _20SecTest
bool checkMinMatch(int tMin) {
#if defined(_5Min)
  return tMin >= 0 && tMin % 5 == 0 && tMin < 60;
#elif defined(_10Min)
  return tMin >= 0 && tMin % 10 == 0 && tMin < 60;
#elif defined(_15Min)
  return tMin >= 0 && tMin % 15 == 0 && tMin < 60;
#else
  return false;
#endif
}
#endif

//----------------- SGP41 Preheat Tasks -------//
void sgp41HeatingOn() {
  if (tSgp41HeatingOn.getIterations() == 1) _delnF("sgp41HeatingOn: First Time");
  sht40sgp41.read();
  if (rtc.now().minute() == setMin) {
    tSgp41HeatingOn.disable();
    tSgp41HeatingOff.enable();
  }
}

void sgp41HeatingOff() {
  if (tSgp41HeatingOff.getIterations() == 1) _delnF("sgp41HeatingOff: First Time");
  if (rtc.now().minute() == preheatTime) {
    tSgp41HeatingOff.disable();
    tSgp41HeatingOn.enable();
  }
}

//----------------- MQTT Tasks ----------------//
void wifiDisconnectedDetect() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    WiFi.reconnect();
  }
}

void reconnectMqtt() {
  if (WiFi.status() == WL_CONNECTED) {
    _deVar("MQTT Broker: ", mqttBroker);
    _deVar(" | Port: ", mqttPort);
    _deVar(" | User: ", mqttUser);
    _deVarln(" | Pass: ", mqttPass);
    _deF("Connecting MQTT... ");
    if (mqtt.connect(deviceName, mqttUser, mqttPass)) {
      tReconnectMqtt.disable();
      _delnF("connected");
      tConnectMqtt.setInterval(0);
      tConnectMqtt.enable();
      statusLed.blinkNumberOfTimes(300, 300, 3);
    } else {
      _deVar("failed state: ", mqtt.state());
      _deVarln(" | counter: ", tReconnectMqtt.getIterations());
      if (tReconnectMqtt.getIterations() >= 3) {
        tReconnectMqtt.disable();
        tConnectMqtt.setInterval(60 * 1000);
        tConnectMqtt.enable();
      }
    }
  } else {
    if (tReconnectMqtt.getIterations() <= 1) _delnF("WiFi is not connected");
  }
}

void connectMqtt() {
  if (!mqtt.connected()) {
    tConnectMqtt.disable();
    tReconnectMqtt.enable();
  } else {
    mqtt.loop();
  }
}

//----------------- Read & Send Data ----------//
void readData() {
  bme680.read();
  veml7700.read();
  mhz19b.read();
  pmsa003.read();
  scd41.read();
  sht40sgp41.read();
  ens160aht21.read();
  dht22.read();

  // Debug print all sensors
  ens160aht21.print();
  bme680.print();
  dht22.print();
  ens160aht21.print();  // ENS160 part already covered inside ens160aht21.print()
  mhz19b.print();
  pmsa003.print();
  scd41.print();
  sht40sgp41.print();
  veml7700.print();

  _delnF("\tData reading done.");
}

void sendData() {
  JsonDocument doc;
  doc.clear();
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
  char jsonBuffer[1100];
  serializeJson(doc, jsonBuffer);

  if (!connectMqttOnce()) {
    _delnF("MQTT publish skipped");
    return;
  }
  mqtt.publish(MQTT_PUB_JSON, jsonBuffer);
  _delnF("\nData sending done.");
}

void fetchData() {
  setupAlarm();

#ifdef _20SecTest
  readData();
  if (connectWiFi()) {
    sendData();
    disconnectWiFi();
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
    if (connectWiFi()) {
      sendData();
      disconnectWiFi();
    }
  } else {
    _delnF("\tread data next time.");
  }
#endif
}

//******************************** Setup ************************************//
void setup() {
#ifdef BATTERY_MODE
  setCpuFrequencyMhz(80);
#endif
  _serialBegin(115200);

  statusLed.turnOFF();
  Wire.begin();
  pinMode(SQW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SQW_PIN), onRtcTrigger, FALLING);

  resetWifiBt.begin(resetWifiBtPin);
  resetWifiBt.setLongClickTime(5000);
  resetWifiBt.setLongClickDetectedHandler(resetWifiBtPressed);

  while (!LittleFS.begin(true)) {
    _delnF("Failed to initialize LittleFS");
    delay(1000);
  }

  // Initialize all sensors
  bme680.begin();
  veml7700.begin();
  mhz19b.begin();
  pmsa003.begin();
  scd41.begin();
  sht40sgp41.begin();
  ens160aht21.begin();
  dht22.begin();

  wifiManagerSetup();
#ifndef BATTERY_MODE
  otaWebUpdateSetup();
#endif
  syncRtc();
  setupAlarm();
  mqttInit();

#ifndef BATTERY_MODE
  tWifiDisconnectedDetect.enable();
  tConnectMqtt.enable();
#endif
#ifndef _20SecTest
  tSgp41HeatingOff.enable();
#endif
}

//******************************** Loop *************************************//
void loop() {
  ts.execute();
  statusLed.loop();
  resetWifiBt.loop();

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
