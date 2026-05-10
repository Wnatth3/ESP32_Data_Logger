#pragma once
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <TaskScheduler.h>
#include <ezLED.h>
#include "Debug.h"

// ─────────────────────────────────────────────────────────────────────────────
//  WifiManagerHandler
//
//  Owns:
//    • LittleFS config file (load / save / print / delete)
//    • WiFiManager setup  (static IP, custom MQTT parameters, portal)
//    • WiFi connect / disconnect helpers
//    • PubSubClient wrapper (connectOnce, loop)
//
//  Usage in main:
//    WifiManagerHandler wifiHandler(DEVICE_NAME);
//    wifiHandler.begin(LittleFS);        // setup() – loads config, runs portal
//    wifiHandler.mqttInit(tConnectMqtt); // setup() – arms the reconnect task
//    wifiHandler.connect();              // before sendData()
//    wifiHandler.disconnect();           // after  sendData()
//    wifiHandler.mqttPublish(topic, buf);
//    wifiHandler.mqttLoop();             // inside task
// ─────────────────────────────────────────────────────────────────────────────

class WifiManagerHandler {
public:
  // ── Config fields (read-only after begin()) ──────────────────────────────
  char mqttBroker[16] = "192.168.0.10";
  char mqttPort[6]    = "1883";
  char mqttUser[16]   = "";
  char mqttPass[16]   = "";
  bool mqttParameter  = false;

  // ── Static-IP fields (read-only after begin()) ───────────────────────────
  char staticIp[16]  = "192.168.0.191";
  char staticGw[16]  = "192.168.0.1";
  char staticSn[16]  = "255.255.255.0";
  char staticDns[16] = "1.1.1.1";

  // ── Constructor ──────────────────────────────────────────────────────────
  WifiManagerHandler(const char* deviceName,
                     const char* apPassword = "password",
                     const char* configFile = "/config.txt",
                     uint16_t mqttBufSize   = 1024)
      : _deviceName(deviceName)
      , _apPassword(apPassword)
      , _configFile(configFile)
      , _mqttBufSize(mqttBufSize)
      , _mqtt(_mqttClient) {}

  // ── begin() – call once in setup() after LittleFS.begin() ────────────────
  void begin(fs::FS& fs) {
    _fs = &fs;
    _loadConfig();
#ifdef _DEBUG_
    _printConfigFile();
#endif
    _runWifiManager();
  }

  // ── mqttInit() – call after begin(); pass the "connectMqtt" Task ─────────
  //    The task is enabled here only if MQTT credentials are present.
  void mqttInit(Task& connectTask) {
    _connectTask = &connectTask;
    _deF("MQTT parameters are ");
    if (mqttParameter) {
      _delnF(" available");
      _mqtt.setBufferSize(_mqttBufSize);
      _mqtt.setServer(mqttBroker, atoi(mqttPort));
      _connectTask->enable();
    } else {
      _delnF(" not available.");
    }
  }

  // ── WiFi helpers ─────────────────────────────────────────────────────────
  bool connect() {
    if (WiFi.status() == WL_CONNECTED) return true;
    _delnF("Connecting to WiFi...");
    if (!_wifiManager.autoConnect(_deviceName, _apPassword)) {
      _delnF("WiFi connect failed");
      return false;
    }
    _delnF("WiFi connected");
    return true;
  }

  void disconnect(bool mqttToo = true) {
    if (WiFi.status() == WL_CONNECTED) {
      if (mqttToo && _mqtt.connected()) _mqtt.disconnect();
      WiFi.disconnect(true, true);
    }
    WiFi.mode(WIFI_OFF);
    _delnF("WiFi powered off");
  }

  void reconnectIfNeeded() {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.disconnect();
      WiFi.reconnect();
    }
  }

  // ── MQTT helpers ─────────────────────────────────────────────────────────

  // One-shot connect used inside sendData(); returns false if unavailable.
  bool mqttConnectOnce() {
    if (!mqttParameter) {
      _delnF("MQTT parameter not available");
      return false;
    }
    if (_mqtt.connected()) return true;
    _deF("Connecting MQTT...");
    if (_mqtt.connect(_deviceName, mqttUser, mqttPass)) {
      _delnF("connected");
      return true;
    }
    _deVar("MQTT connect failed state: ", _mqtt.state());
    _delnF("");
    return false;
  }

  // Persistent reconnect used inside the reconnectMqtt Task.
  // Returns true when connected, false while still retrying.
  bool mqttReconnect(uint32_t iterations, ezLED& statusLed, Task& reconnectTask,
                     Task& connectTask) {
    if (WiFi.status() != WL_CONNECTED) {
      if (iterations <= 1) _delnF("WiFi is not connected");
      return false;
    }
    _deVar("MQTT Broker: ", mqttBroker);
    _deVar(" | Port: ", mqttPort);
    _deVar(" | User: ", mqttUser);
    _deVarln(" | Pass: ", mqttPass);
    _deF("Connecting MQTT... ");
    if (_mqtt.connect(_deviceName, mqttUser, mqttPass)) {
      reconnectTask.disable();
      _delnF("connected");
      connectTask.setInterval(0);
      connectTask.enable();
      statusLed.blinkNumberOfTimes(300, 300, 3);
      return true;
    }
    _deVar("failed state: ", _mqtt.state());
    _deVarln(" | counter: ", iterations);
    if (iterations >= 3) {
      reconnectTask.disable();
      connectTask.setInterval(60 * 1000);
      connectTask.enable();
    }
    return false;
  }

  bool mqttConnected() { return _mqtt.connected(); }
  void mqttLoop() { _mqtt.loop(); }
  bool mqttPublish(const char* topic, const char* payload) {
    return _mqtt.publish(topic, payload);
  }

private:
  // ── Injected / stored state ───────────────────────────────────────────────
  const char* _deviceName;
  const char* _apPassword;
  const char* _configFile;
  uint16_t _mqttBufSize;
  fs::FS* _fs        = nullptr;
  Task* _connectTask = nullptr;
  bool _shouldSave   = false;

  WiFiManager _wifiManager;
  WiFiClient _mqttClient;
  PubSubClient _mqtt;

  // ── LittleFS helpers ─────────────────────────────────────────────────────
  void _loadConfig() {
    _delnF("Loading configuration...");
    File file = _fs->open(_configFile, "r");
    if (!file) {
      _delnF("Failed to open config file");
      return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, file))
      _delnF("Failed to read file, using defaults");

    strlcpy(mqttBroker, doc["mqttBroker"] | mqttBroker, sizeof(mqttBroker));
    strlcpy(mqttPort, doc["mqttPort"] | mqttPort, sizeof(mqttPort));
    strlcpy(mqttUser, doc["mqttUser"] | "", sizeof(mqttUser));
    strlcpy(mqttPass, doc["mqttPass"] | "", sizeof(mqttPass));
    mqttParameter = doc["mqttParameter"] | false;

    if (doc["ip"]) {
      strlcpy(staticIp, doc["ip"], sizeof(staticIp));
      strlcpy(staticGw, doc["gateway"], sizeof(staticGw));
      strlcpy(staticSn, doc["subnet"], sizeof(staticSn));
      strlcpy(staticDns, doc["dns"], sizeof(staticDns));
    } else {
      _delnF("No custom IP in config, using defaults");
    }
    file.close();
  }

  void _saveConfig() {
    File file = _fs->open(_configFile, "w");
    if (!file) {
      _delnF("Failed to open config file for writing");
      return;
    }

    JsonDocument doc;
    doc["mqttBroker"] = mqttBroker;
    doc["mqttPort"]   = mqttPort;
    doc["mqttUser"]   = mqttUser;
    doc["mqttPass"]   = mqttPass;

    if (strlen(mqttBroker) > 0) {
      doc["mqttParameter"] = true;
      mqttParameter        = true;
    }
    doc["ip"]      = WiFi.localIP().toString();
    doc["gateway"] = WiFi.gatewayIP().toString();
    doc["subnet"]  = WiFi.subnetMask().toString();
    doc["dns"]     = WiFi.dnsIP().toString();

    if (serializeJson(doc, file) == 0) _delnF("Failed to write config");
    else _deVarln("Config saved to ", _configFile);
    file.close();
  }

  void _printConfigFile() {
    _delnF("Print config file...");
    File file = _fs->open(_configFile, "r");
    if (!file) {
      _delnF("Failed to open config file");
      return;
    }
    JsonDocument doc;
    if (!deserializeJson(doc, file)) {
      char buf[512];
      serializeJsonPretty(doc, buf);
      _deln(buf);
    }
    file.close();
  }

  void _deleteConfigFile() {
    _deVarln("Deleting file: ", _configFile);
    if (_fs->remove(_configFile)) _delnF("- file deleted");
    else _delnF("- delete failed");
  }

  // ── WiFiManager portal setup ─────────────────────────────────────────────
  void _runWifiManager() {
    // Custom portal parameters (must stay in scope until autoConnect returns)
    WiFiManagerParameter paramBroker("broker", "mqtt server", mqttBroker, 16);
    WiFiManagerParameter paramPort("port", "mqtt port", mqttPort, 6);
    WiFiManagerParameter paramUser("user", "mqtt user", mqttUser, 10);
    WiFiManagerParameter paramPass("pass", "mqtt pass", mqttPass, 10);

    // Save-config callback (lambda captures this)
    _wifiManager.setSaveConfigCallback([this]() {
      _delnF("Should save config");
      _shouldSave = true;
    });

    // Static IP
    IPAddress ip, gw, sn, dns;
    ip.fromString(staticIp);
    gw.fromString(staticGw);
    sn.fromString(staticSn);
    dns.fromString(staticDns);
    _wifiManager.setSTAStaticIPConfig(ip, gw, sn, dns);

    _wifiManager.addParameter(&paramBroker);
    _wifiManager.addParameter(&paramPort);
    _wifiManager.addParameter(&paramUser);
    _wifiManager.addParameter(&paramPass);

    _wifiManager.setDarkMode(true);
#ifndef _DEBUG_
    _wifiManager.setDebugOutput(true, WM_DEBUG_SILENT);
#endif

#ifndef BATTERY_MODE
    if (_wifiManager.autoConnect(_deviceName, _apPassword))
      _delnF("WiFi is connected :D");
    else _delnF("Config portal running");
#endif

    // Read back updated values from portal
    strlcpy(mqttBroker, paramBroker.getValue(), sizeof(mqttBroker));
    strlcpy(mqttPort, paramPort.getValue(), sizeof(mqttPort));
    strlcpy(mqttUser, paramUser.getValue(), sizeof(mqttUser));
    strlcpy(mqttPass, paramPass.getValue(), sizeof(mqttPass));

    if (_shouldSave) _saveConfig();

    _deVar("ip: ", WiFi.localIP());
    _deVar(" | gw: ", WiFi.gatewayIP());
    _deVar(" | sn: ", WiFi.subnetMask());
    _deVarln(" | dns: ", WiFi.dnsIP());
  }
};
