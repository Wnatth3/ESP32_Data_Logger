#include <Arduino.h>
#include <FS.h>  //this needs to be first, or it all crashes and burns.../
#include <LittleFS.h>
#include <ArduinoJson.h>  //https://github.com/bblanchon/ArduinoJson
#include <WiFiManager.h>  //https://github.com/tzapu/WiFiManager
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
#include <SoftwareSerial.h>
#include <Wire.h>
#include <ezLED.h>
// Sensors
#include "bsec.h"                  // SD0 Pkn connect to GND // BME680 - BSEC Software library
#include <PMserial.h>              // PMSA003 - PMSerial - https://github.com/avaldebe/PMserial
#include "Adafruit_VEML7700.h"     // VEML7700 - Adafruit VEML7700
#include "MHZ19.h"                 // MH-Z19B - MH-Z19 - https://github.com/WifWaf/MH-Z19
#include <SensirionI2cScd4x.h>     // SCD4x - Sensirion I2C SCD4x
#include <SensirionI2CSgp41.h>     // SGP41 - Sensirion I2C SGP41
#include <SensirionI2cSht4x.h>     // SHT40 - Sensirion I2C SHT4x
#include <VOCGasIndexAlgorithm.h>  // SGP41 - Sensirian Gas Index Algorithm
#include <NOxGasIndexAlgorithm.h>  // SGP41 - Sensirian Gas Index Algorithm
#include <Adafruit_AHTX0.h>        // AHT21 - Adafruit AHTX0
#include <ScioSense_ENS160.h>      // ENS160 - ENS160 Adafruit Fork - https://github.com/adafruit/ENS160_driver
#include <DHT.h>                   // DHT22 - DHT Sensor library
#include <TickTwo.h>
#include <Button2.h>

//******************************** Configulation ****************************//
#define _DEBUG_  // Comment this line if you don't want to debug
#include "Debug.h"

// -- Read Sensores every 5, 10, or 15 minutes -- //
#define _20SecTest  // Uncomment this line if you want 20sec Sensors Test
// #define _5Min  // Uncomment this line if you want to read sensors every 5 minutes
// #define _10Min  // Uncomment this line if you want to read sensors every 10 minutes
// #define _15Min  // Uncomment this line if you want to read sensors every 15 minutes

//******************************** Variables & Objects **********************//
#define deviceName "WeatherSt"

//----------------- esLED ---------------------//
#define led LED_BUILTIN
ezLED statusLed(led);

//----------------- Reset WiFi Button ---------//
#define resetWifiBtPin 0
Button2 resetWifiBt;

//----------------- WiFi Manager --------------//
const char* filename = "/config.txt";  // Config file name

// default custom static IP
char static_ip[16]  = "192.168.0.191";
char static_gw[16]  = "192.168.0.1";
char static_sn[16]  = "255.255.255.0";
char static_dns[16] = "1.1.1.1";
// MQTT
char mqttBroker[16] = "192.168.0.10";
char mqttPort[6]    = "1883";
char mqttUser[16];  // = "admin";
char mqttPass[16];  // = "admin";

bool mqttParameter;

bool shouldSaveConfig = false;  // flag for saving data

WiFiManager wifiManager;

// //----------------- OTA Web Update ------------//
#include "Ota.h"

WebServer server(80);

//*---------------- PubSubClient -------------//
#define MQTT_PUB_JSON "esp32/sensors/json"

WiFiClient   mqttClient;
PubSubClient mqtt(mqttClient);

//----------------- Time Setup ----------------//
// Sync Time with NTP Server
WiFiUDP ntpUDP;
// NTPClient timeClient(ntpUDP, "NTP Server Name", offset time(ms), update Interval(ms));
NTPClient timeClient(ntpUDP, "time.google.com", 25200 /*GMT +7*/);
// NTPClient timeClient(ntpUDP, "time.facebook.com", 25200 /*GMT +7*/);
// NTPClient timeClient(ntpUDP, "time.apple.com", 25200 /*GMT +7*/);

//----------------- DS3231  Real Time Clock --//
#define SQW_PIN 33  // pin 33 for external Wake Up (ext1)
RTC_DS3231 rtc;

uint8_t tMin;
uint8_t setMin;
uint8_t preheatTime;
bool    rtcTrigger = false;

//----------------- Sensors -------------------//
// BME680 Temperature, Humidity, & Pressure Sensor
float pressBme680;
float gasResBme680;
float tempBme680;
float humiBme680;

bsec_virtual_sensor_t sensorList[13] = {
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_RAW_TEMPERATURE,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_HUMIDITY,
    BSEC_OUTPUT_RAW_GAS,
    BSEC_OUTPUT_STABILIZATION_STATUS,
    BSEC_OUTPUT_RUN_IN_STATUS,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    BSEC_OUTPUT_GAS_PERCENTAGE};

Bsec iaqSensor;

// VEML7700 Illumination Sensor
float lux;

Adafruit_VEML7700 veml = Adafruit_VEML7700();

// MH-Z19B CO2 Sensor
#define rxPin2 16
#define txPin2 17
int   co2;
MHZ19 myMHZ19;  // Constructor for library

HardwareSerial mySerial(2);  // On ESP32 we do not require the SoftwareSerial library, since we have 2 USARTS available

// PMSA003A
#define pmsRX 18
#define pmsTX 19
uint16_t pm010, pm025, pm100;
SerialPM pms(PMSx003, pmsRX, pmsTX);  // SoftwareSerial

// SCD41 CO2 Sensor
#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

static int16_t scd41Error;
static char    scd41ErrorMessage[64];

float    tempScd41;
float    humiScd41;
uint16_t co2Scd41;

SensirionI2cScd4x scd41;

// SHT40 Temperature & Humidity Sensor
float tempSht40;  // degreeC
float humiSht40;  // %RH

SensirionI2cSht4x sht40;

// SGP41 Air Quality Sensor, VOC & NOx Index
uint16_t conditioning_s = 10;
uint16_t sgp41Error;
char     sgp41ErrorMessage[256];

int32_t vocIdxSgp41;
int32_t noxIdxSgp41;

SensirionI2CSgp41 sgp41;

VOCGasIndexAlgorithm voc_algorithm;
NOxGasIndexAlgorithm nox_algorithm;

// AHT21
float tempAht21;
float humiAht21;

Adafruit_AHTX0 aht21;

// ENS160
uint8_t  aqiEns160;
uint8_t  tvocEns160;
uint16_t eco2Ens160;

ScioSense_ENS160 ens160(ENS160_I2CADDR_1);  // 0x53

// DHT22
#define DHTPIN  32
#define DHTTYPE DHT22

float tempDht22;
float humiDht22;

DHT dht(DHTPIN, DHTTYPE);

//******************************** Tasks ************************************//
void    sgp41HeatingOn();
void    sgp41HeatingOff();
TickTwo tSgp41HeatingOn(sgp41HeatingOn, 500, 0, MILLIS);  // (function, interval, iteration, interval unit)
TickTwo tSgp41HeatingOff(sgp41HeatingOff, 0, 0, MILLIS);  // (function, interval, iteration, interval unit)

void    wifiDisconnectedDetect();
TickTwo tWifiDisconnectedDetect(wifiDisconnectedDetect, 60000, 0, MILLIS);  // Every 1 minutes

void    connectMqtt();
void    reconnectMqtt();
TickTwo tConnectMqtt(connectMqtt, 0, 0, MILLIS);  // (function, interval, iteration, interval unit)
TickTwo tReconnectMqtt(reconnectMqtt, 3000, 0, MILLIS);

//******************************** Functions ********************************//
//----------------- WiFi Manager --------------//
// Loads the configuration from a file
void loadConfiguration(fs::FS& fs, const char* filename) {
    _delnF("Loading configuration...");
    File file = fs.open(filename, "r");
    if (!file) {
        _delnF("Failed to open data file");
        return;
    }

    // Allocate a temporary JsonDocument
    JsonDocument doc;
    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        _delnF("Failed to read file, using default configuration");
    }
    // Copy values from the JsonDocument to the Config
    // strlcpy(Destination_Variable, doc["Source_Variable"] /*| "Default_Value"*/, sizeof(Destination_Name));
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
        mqtt.setBufferSize(1024);  // Max buffer size = 1024 bytes (default: 256 bytes)
        mqtt.setServer(mqttBroker, atoi(mqttPort));
        tConnectMqtt.start();
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

    JsonDocument         doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        _delnF("Failed to read file");
    }

    char buffer[512];
    serializeJsonPretty(doc, buffer);
    _delnF(buffer);

    file.close();
}

void deleteFile(fs::FS& fs, const char* path) {
    _deVarln("Deleating file: ", path);
    if (fs.remove(path)) {
        _delnF("- file deleted");
    } else {
        _delnF("- delete failed");
    }
}

void wifiManagerSetup() {
    loadConfiguration(LittleFS, filename);
#ifdef _DEBUG_
    printFile(LittleFS, filename);
#endif
    // Don't move this block code, it is important for the blocking WiFiManager.
    WiFiManagerParameter customMqttBroker("broker", "mqtt server", mqttBroker, 16);
    WiFiManagerParameter customMqttPort("port", "mqtt port", mqttPort, 6);
    WiFiManagerParameter customMqttUser("user", "mqtt user", mqttUser, 10);
    WiFiManagerParameter customMqttPass("pass", "mqtt pass", mqttPass, 10);
    // end of block code

    wifiManager.setSaveConfigCallback(saveConfigCallback);

    // set static ip
    IPAddress _ip, _gw, _sn, _dns;
    _ip.fromString(static_ip);
    _gw.fromString(static_gw);
    _sn.fromString(static_sn);
    _dns.fromString(static_dns);
    wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn, _dns);
    // add all your parameters here
    wifiManager.addParameter(&customMqttBroker);
    wifiManager.addParameter(&customMqttPort);
    wifiManager.addParameter(&customMqttUser);
    wifiManager.addParameter(&customMqttPass);

    // reset settings - for testing
    // wifiManager.resetSettings();
    wifiManager.setDarkMode(true);
#ifndef _DEBUG_
    wifiManager.setDebugOutput(true, WM_DEBUG_SILENT);
#endif
    // wifiManager.setDebugOutput(true, WM_DEBUG_DEV);
    // wifiManager.setMinimumSignalQuality(20); // Default 8%
    // wifiManager.setConfigPortalTimeout(60);

    if (wifiManager.autoConnect(deviceName, "password")) {
        _delnF("WiFI is connected :D");
    } else {
        _delnF("Configportal running");
    }

    // read updated parameters
    strcpy(mqttBroker, customMqttBroker.getValue());
    strcpy(mqttPort, customMqttPort.getValue());
    strcpy(mqttUser, customMqttUser.getValue());
    strcpy(mqttPass, customMqttPass.getValue());

    // save the custom parameters to FS
    if (shouldSaveConfig) {
        File file = LittleFS.open(filename, "w");
        if (!file) {
            _delnF("Failed to open config file for writing");
            return;
        }

        // Allocate a temporary JsonDocument
        JsonDocument doc;
        // Set the values in the document
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

        // Serialize JSON to file
        if (serializeJson(doc, file) == 0) {
            _delnF("Failed to write to file");
        } else {
            _deVarln("The configuration has been saved to ", filename);
        }

        file.close();
    }

    _deVar("ip: ", WiFi.localIP());
    _deVar(" | gw: ", WiFi.gatewayIP());
    _deVar(" | sn: ", WiFi.subnetMask());
    _deVarln(" | dns: ", WiFi.dnsIP());
}

// ----------------- Reset WiFi Button ---------//
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

//----------------- OTA Web Updater -----------//
void otaWebUpdateSetup() {
    // const char* host = "ESP32";
    /*use mdns for host name resolution*/
    if (!MDNS.begin(deviceName)) {  // http://deviceName.local
        _delnF("Error setting up MDNS responder!");
        while (1) {
            delay(500);  // Default is 1000
        }
    }
    _delnF("mDNS responder started");
    /*return index page which is stored in ud */
    server.on("/", HTTP_GET, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", loginIndex);
    });
    server.on("/ud", HTTP_GET, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", ud);
    });
    /*handling uploading firmware file */
    server.on(
        "/update", HTTP_POST,
        []() {
            server.sendHeader("Connection", "close");
            server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
            ESP.restart();
        },
        []() {
            HTTPUpload& upload = server.upload();
            if (upload.status == UPLOAD_FILE_START) {
                _deVarln("Update: ", upload.filename);
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {  // start with max available size
                    Update.printError(Serial);
                }
            } else if (upload.status == UPLOAD_FILE_WRITE) {
                /* flashing firmware to ESP*/
                if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                    Update.printError(Serial);
                }
            } else if (upload.status == UPLOAD_FILE_END) {
                if (Update.end(true)) {  // true to set the size to the current progress
                    _deVarln("Update Success: ", upload.totalSize);
                    _delnF(" Rebooting...");
                } else {
                    Update.printError(Serial);
                }
            }
        });

    // Reset route handler
    server.on("/reset", HTTP_POST, []() {
        server.send(200, "text/plain", "ESP32 is restarting...");
        ESP.restart();  // Restart ESP32
    });

    server.begin();

    _delnF("\tOTA Web updater setting is done.");
}
//----------------- Time Setup ----------------//
String strTime(DateTime t) {
    char buff[] = "YYYY MMM DD (DDD) hh:mm:ss";
    return t.toString(buff);
}

// uint8_t preheatTime(uint8_t setMin) { return setMin == 0 ? 57 : setMin -3;}

// Set the time to 0, 15, 30, 45 min
uint8_t setMinMatch(uint8_t a) {
    if (a >= 0 && a <= 59) {
#ifdef _5Min
        return ((a / 5) + 1) * 5 % 60;
#endif
#ifdef _10Min
        return ((a / 10) + 1) * 10 % 60;
#endif
#ifdef _15Min
        return ((a / 15) + 1) * 15 % 60;
#endif
    }
    return 0;
}

uint8_t roundSec(uint8_t sec) { return sec > 60 ? sec - 60 : sec; }

void syncRtc() {
    if (!rtc.begin()) {
        _delnF("Couldn't find RTC!");
        // Serial.flush();
        // while (1) delay(10);
    }
    // Setup time with NTPClient library.
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
    //     if (!rtc.begin()) {
    //
    //         _deln("Couldn't find RTC!");
    // #endif
    // Serial.flush();
    // while (1) delay(10);
    // }

    if (rtc.lostPower()) {
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  // this will adjust to the date and time at compilation
    }

    // syncRtc();  // Sync RTC with NTP server. The internet connection is required.
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2023, 12, 9, 21, 59, 35));  // Manually set time

    _deln("\n\t" + strTime(rtc.now()));

    rtc.disable32K();  // we don't need the 32K Pin, so disable it
    rtc.clearAlarm(1);
    rtc.clearAlarm(2);
    rtc.writeSqwPinMode(DS3231_OFF);  // stop oscillating signals at SQW Pin, otherwise setAlarm1 will fail
    rtc.disableAlarm(2);

#ifdef _20SecTest
    // Set alarm time
    rtc.setAlarm1(rtc.now() + TimeSpan(0, 0, 0, 20), DS3231_A1_Second);  // Test
    _deF("Trigger next time: ");
    _deln(String(roundSec(rtc.now().second() + 20)) + "th sec.");

#else

    tMin = rtc.now().minute();

    _deln("tMin: " + String(tMin));
    // uint8_t setMin = setMinMatch(tMin);
    setMin      = setMinMatch(tMin);
    preheatTime = setMin == 0 ? 58 : setMin - 2;
    rtc.setAlarm1(DateTime(2023, 2, 18, 0, setMin, 0), DS3231_A1_Minute);
    // rtc.setAlarm1(DateTime(2023, 2, 18, 0, 0, 0), DS3231_A1_Minute);
    // rtc.setAlarm2(DateTime(2023, 2, 18, 0, 30, 0), DS3231_A2_Minute);
    // rtc.setAlarm1(DateTime(2023, 2, 18, 0, 57, 0), DS3231_A1_Minute);
    // rtc.setAlarm2(DateTime(2023, 2, 18, 0, 27, 0), DS3231_A2_Minute);

    _de("Preheat Time: " + String(preheatTime) + "th min.");
    _deln(" | Tringger next time: " + String(setMin) + "th min.");

#endif

    _delnF("\tThe alarm setting is done.");
}

void IRAM_ATTR onRtcTrigger() { rtcTrigger = true; }

// 15 minute match 0, 15, 30, and 45
bool checkMinMatch(int tMin) {
    // #ifdef _5Min
    //     return tMin >= 0 && tMin % 5 == 0 && tMin < 60;
    // #endif
    // #ifdef _10Min
    //     return tMin >= 0 && tMin % 10 == 0 && tMin < 60;
    // #endif
    // #ifdef _15Min
    //     return tMin >= 0 && tMin % 15 == 0 && tMin < 60;
    // #endif
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

//----------------- Collect Data --------------//

void readScd41() {
    bool dataReady = false;

    scd41Error = scd41.getDataReadyStatus(dataReady);
    if (scd41Error != NO_ERROR) {
        _deF("Error trying to execute getDataReadyStatus(): ");
        errorToString(scd41Error, scd41ErrorMessage, sizeof scd41ErrorMessage);
        _deln(scd41ErrorMessage);
        return;
    }
    while (!dataReady) {
        delay(100);
        scd41Error = scd41.getDataReadyStatus(dataReady);
        if (scd41Error != NO_ERROR) {
            _deF("Error trying to execute getDataReadyStatus(): ");
            errorToString(scd41Error, scd41ErrorMessage, sizeof scd41ErrorMessage);
            _deln(scd41ErrorMessage);
            return;
        }
    }
    // If ambient pressure compenstation during measurement
    // is required, you should call the respective functions here.
    // Check out the header file for the function definition.
    scd41Error = scd41.readMeasurement(co2Scd41, tempScd41, humiScd41);
    if (scd41Error != NO_ERROR) {
        _deF("Error trying to execute readMeasurement(): ");
        errorToString(scd41Error, scd41ErrorMessage, sizeof scd41ErrorMessage);
        _deln(scd41ErrorMessage);
        return;
    }
}

void readSht40Sgp41() {
    humiSht40                      = 0;  // %RH
    tempSht40                      = 0;  // degreeC
    uint16_t srawVoc               = 0;
    uint16_t srawNox               = 0;
    uint16_t defaultCompenstaionRh = 0x8000;  // in ticks as defined by SGP41
    uint16_t defaultCompenstaionT  = 0x6666;  // in ticks as defined by SGP41
    uint16_t compensationRh        = 0;       // in ticks as defined by SGP41
    uint16_t compensationT         = 0;       // in ticks as defined by SGP41

    // 1. Sleep: Measure every second (1Hz), as defined by the Gas Index
    // Algorithm
    //    prerequisite
    // delay(1000);

    // 2. Measure temperature and humidity for SGP internal compensation
    sgp41Error = sht40.measureHighPrecision(tempSht40, humiSht40);
    if (sgp41Error) {
        errorToString(sgp41Error, sgp41ErrorMessage, 256);
        _deVarln("SHT4x - Error trying to execute measureHighPrecision(): ", sgp41ErrorMessage);
        _delnF("Fallback to use default values for humidity and temperature compensation for SGP41");
        compensationRh = defaultCompenstaionRh;
        compensationT  = defaultCompenstaionT;
    } else {
        // convert temperature and humidity to ticks as defined by SGP41
        // interface
        // NOTE: in case you read RH and T raw signals check out the
        // ticks specification in the datasheet, as they can be different for
        // different sensors
        compensationT  = static_cast<uint16_t>((tempSht40 + 45) * 65535 / 175);
        compensationRh = static_cast<uint16_t>(humiSht40 * 65535 / 100);
    }

    // 3. Measure SGP4x signals
    if (conditioning_s > 0) {
        // During NOx conditioning (10s) SRAW NOx will remain 0
        sgp41Error = sgp41.executeConditioning(compensationRh, compensationT, srawVoc);
        conditioning_s--;
    } else {
        sgp41Error = sgp41.measureRawSignals(compensationRh, compensationT, srawVoc, srawNox);
    }

    // 4. Process raw signals by Gas Index Algorithm to get the VOC and NOx
    // index
    //    values
    if (sgp41Error) {
        errorToString(sgp41Error, sgp41ErrorMessage, 256);
        _deF("SGP41 - Error trying to execute measureRawSignals(): ");
        _deln(sgp41ErrorMessage);
    } else {
        vocIdxSgp41 = voc_algorithm.process(srawVoc);
        noxIdxSgp41 = nox_algorithm.process(srawNox);
    }
}

void sgp41HeatingOn() {
    if (tSgp41HeatingOn.counter() == 1) {
        _delnF("sgp41HeatingOn: First Time");
    }

    readSht40Sgp41();
    uint8_t now = rtc.now().minute();
    if (now == setMin) {
        tSgp41HeatingOn.stop();
        tSgp41HeatingOff.start();
    }
}

void sgp41HeatingOff() {
    if (tSgp41HeatingOff.counter() == 1) {
        _delnF("sgp41HeatingOff: First Time");
    }

    uint8_t now = rtc.now().minute();
    if (now == preheatTime) {
        tSgp41HeatingOff.stop();
        tSgp41HeatingOn.start();
    }
}

void readEns160Aht21() {
    // AHT21
    sensors_event_t humiEvent, tempEvent;
    aht21.getEvent(&humiEvent, &tempEvent);  // populate temp and humidity objects with fresh data

    tempAht21 = tempEvent.temperature;
    humiAht21 = humiEvent.relative_humidity;

    // ENS160
    ens160.measure(true);
    ens160.measureRaw(true);

    aqiEns160  = ens160.getAQI();
    tvocEns160 = ens160.getTVOC();
    eco2Ens160 = ens160.geteCO2();
}

void readData() {
    if (iaqSensor.run()) {
        tempBme680   = iaqSensor.temperature;
        humiBme680   = iaqSensor.humidity;
        pressBme680  = iaqSensor.pressure / 100.f;
        gasResBme680 = iaqSensor.gasResistance;
    }

    // VEML7700-------------/
    lux = veml.readLux(VEML_LUX_AUTO);

    // MH-Z19B-------------/
    co2 = myMHZ19.getCO2();  // Request CO2 (as ppm)

    // PMSA003A-------------/
    pms.read();
    if (pms) {  // successfull read
        pm010 = pms.pm01;
        pm025 = pms.pm25;
        pm100 = pms.pm10;
    }

    readScd41();
    readSht40Sgp41();
    readEns160Aht21();

    // DHT22
    tempDht22 = dht.readTemperature();
    humiDht22 = dht.readHumidity();

    _deF("AHT21: Temp: ");
    _de(tempAht21, 2);
    _deF(" C | Humi: ");
    _de(humiAht21, 2);
    _delnF(" %");

    _deF("BME680: Temp: ");
    _de(tempBme680, 2);
    _deF(" C | Humi: ");
    _de(humiBme680, 2);
    _deF(" % | Gas Resist: ");
    _de(gasResBme680, 2);
    _delnF(" kohm");

    _deF("DHT22: Temp: ");
    _de(tempDht22, 2);
    _deF(" C | Humi: ");
    _de(humiDht22, 2);
    _delnF(" %");

    _deF("ENS160: AQI: ");
    _de(aqiEns160);
    _deF(" | TVOC: ");
    _de(tvocEns160);
    _deF(" ppb | eCO2: ");
    _de(eco2Ens160);
    _delnF(" ppm");

    _deF("MHZ19B: CO2: ");
    _de(co2);
    _delnF(" ppm");

    _deF("PMSA003A: PM1.0: ");
    _de(pm010);
    _deF(" ug/m3 | PM2.5: ");
    _de(pm025);
    _deF(" ug/m3 | PM10: ");
    _de(pm100);
    _delnF(" ug/m3");

    _deF("SDC41: Temp: ");
    _de(tempScd41, 2);
    _deF(" C | Humi: ");
    _de(humiScd41, 2);
    _deF(" % | CO2: ");
    _de(co2Scd41);
    _delnF(" ppm");

    _deF("SGP41: VOC Idx: ");
    _de(vocIdxSgp41);
    _deF(" | NOx Idx: ");
    _deln(noxIdxSgp41);

    _deF("SHT40: Temp: ");
    _de(tempSht40, 2);
    _deF(" C | Humi: ");
    _de(humiSht40, 2);
    _delnF(" %");

    _deF("VEML7700: Lux: ");
    _de(lux, 2);
    _deln();

    _delnF("\tdata reading is done.");
}

void sendData() {
    JsonDocument doc;
    doc.clear();

    JsonObject doc_0         = doc.add<JsonObject>();
    doc_0["measurement"]     = "aht21";
    JsonObject root_0_fields = doc_0["fields"].to<JsonObject>();
    root_0_fields["temp"]    = tempAht21;
    root_0_fields["humi"]    = humiAht21;

    JsonObject doc_1         = doc.add<JsonObject>();
    doc_1["measurement"]     = "bme680";
    JsonObject root_1_fields = doc_1["fields"].to<JsonObject>();
    root_1_fields["temp"]    = tempBme680;
    root_1_fields["humi"]    = humiBme680;
    root_1_fields["press"]   = pressBme680;
    root_1_fields["gasRes"]  = gasResBme680;

    JsonObject doc_2         = doc.add<JsonObject>();
    doc_2["measurement"]     = "dht22";
    JsonObject root_2_fields = doc_2["fields"].to<JsonObject>();
    root_2_fields["temp"]    = tempDht22;
    root_2_fields["humi"]    = humiDht22;

    JsonObject doc_3         = doc.add<JsonObject>();
    doc_3["measurement"]     = "ens160";
    JsonObject root_3_fields = doc_3["fields"].to<JsonObject>();
    root_3_fields["aqi"]     = aqiEns160;
    root_3_fields["tVoc"]    = tvocEns160;
    root_3_fields["eCo2"]    = eco2Ens160;

    JsonObject doc_4       = doc.add<JsonObject>();
    doc_4["measurement"]   = "mhz19b";
    doc_4["fields"]["co2"] = co2;

    JsonObject doc_5         = doc.add<JsonObject>();
    doc_5["measurement"]     = "pmsa003a";
    JsonObject root_5_fields = doc_5["fields"].to<JsonObject>();
    root_5_fields["pm010"]   = pm010;
    root_5_fields["pm025"]   = pm025;
    root_5_fields["pm100"]   = pm100;

    JsonObject doc_6         = doc.add<JsonObject>();
    doc_6["measurement"]     = "scd41";
    JsonObject root_6_fields = doc_6["fields"].to<JsonObject>();
    root_6_fields["temp"]    = tempScd41;
    root_6_fields["humi"]    = humiScd41;
    root_6_fields["co2"]     = co2Scd41;

    JsonObject doc_7         = doc.add<JsonObject>();
    doc_7["measurement"]     = "sgp41";
    JsonObject root_7_fields = doc_7["fields"].to<JsonObject>();
    root_7_fields["vocIdx"]  = vocIdxSgp41;
    root_7_fields["noxIdx"]  = noxIdxSgp41;

    JsonObject doc_8         = doc.add<JsonObject>();
    doc_8["measurement"]     = "sht40";
    JsonObject root_8_fields = doc_8["fields"].to<JsonObject>();
    root_8_fields["temp"]    = tempSht40;
    root_8_fields["humi"]    = humiSht40;

    JsonObject doc_9       = doc.add<JsonObject>();
    doc_9["measurement"]   = "veml7700";
    doc_9["fields"]["lux"] = lux;

    doc.shrinkToFit();  // optional
    char jsonBuffer[1100];
    serializeJson(doc, jsonBuffer);

    mqtt.publish(MQTT_PUB_JSON, jsonBuffer);

    // Print the JSON document
    // memset(jsonBuffer, 0, sizeof(jsonBuffer));  // Clear the buffer
    // serializeJsonPretty(doc, jsonBuffer);
    // _delnF(jsonBuffer);

    _delnF("\nData sending is done.");
}

void IRAM_ATTR fetchData() {
    setupAlarm();

#ifdef _20SecTest
    readData();
    sendData();

#else

#ifdef _5Min
    _deF("5min Match: ");
#endif
#ifdef _10Min
    _deF("10min Match: ");
#endif
#ifdef _15Min
    _deF("15min Match: ");
#endif

    _deln(checkMinMatch(tMin) ? "true" : "false");
    if (checkMinMatch(tMin)) {
        readData();
        sendData();
    } else {
        _delnF("\tread data next time.");
    }

#endif
}

//******************************** Task Functions ***************************//
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
            tReconnectMqtt.stop();
            _delnF("connected");
            tConnectMqtt.interval(0);
            tConnectMqtt.start();
            statusLed.blinkNumberOfTimes(200, 200, 3);  // 250ms ON, 750ms OFF, repeat 3 times, blink immediately
        } else {
            _deVar("failed state: ", mqtt.state());
            _deVarln(" | counter: ", tReconnectMqtt.counter());
            if (tReconnectMqtt.counter() >= 3) {
                tReconnectMqtt.stop();
                tConnectMqtt.interval(60 * 1000);  // 300 sec. = 5 min.Wait 5 minute before reconnecting.
                tConnectMqtt.start();
            }
        }
    } else {
        if (tReconnectMqtt.counter() <= 1) {
            _delnF("WiFi is not connected");
        }
    }
}

void connectMqtt() {
    if (!mqtt.connected()) {
        tConnectMqtt.stop();
        tReconnectMqtt.start();
    } else {
        mqtt.loop();
    }
}

void checkSensor(bool condition, String sensorName) {
    if (condition) {
        _deln(sensorName + ": Available");
    } else {
        _deln(sensorName + ": Failed!");
    }
}

void printScd41Config(String prefix) {
    float    tempOffset    = 0.0f;
    uint16_t tempOffsetRaw = 0, altitude = 0;

    _de(prefix);
    scd41.getTemperatureOffset(tempOffset);
    _deF(" tempOffset: ");
    _de(tempOffset, 2);
    _deF(" C");
    scd41.getTemperatureOffsetRaw(tempOffsetRaw);
    _deF(" | ");
    _deF(" TempOffsetRaw: ");
    _de(175 * tempOffsetRaw / 65535);
    _deF(" C");
    scd41.getSensorAltitude(altitude);
    _deF(" | ");
    _deF(" Altitude: ");
    _de(altitude);
    _delnF(" m");
}

//******************************** Setup  ***********************************//
void setup() {
    _serialBegin(115200);
    statusLed.turnOFF();
    Wire.begin();
    pinMode(SQW_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(SQW_PIN), onRtcTrigger, FALLING);
    resetWifiBt.begin(resetWifiBtPin);
    resetWifiBt.setLongClickTime(5000);
    resetWifiBt.setLongClickDetectedHandler(resetWifiBtPressed);

    while (!LittleFS.begin(true)) {  // true = format if mount failed
        _delnF("Failed to initialize LittleFS library");
        delay(1000);
    }
    // BME680
    iaqSensor.begin(BME68X_I2C_ADDR_HIGH, Wire); // BME68X_I2C_ADDR_HIGH(default) = 0x77, BME68X_I2C_ADDR_LOW = 0x76
    iaqSensor.updateSubscription(sensorList, 13, BSEC_SAMPLE_RATE_LP);
    // VEML7700
    checkSensor(veml.begin(), "VEML7700");
    // MH-Z19B
    mySerial.begin(9600, SERIAL_8N1, rxPin2, txPin2);  // (Uno example) device to MH-Z19 serial start
    myMHZ19.begin(mySerial);                           // *Serial(Stream) reference must be passed to library begin().
    myMHZ19.autoCalibration();                         // Turn auto calibration ON (OFF autoCalibration(false))
    // PMSA003
    pms.init();
    // SCD41
    scd41.begin(Wire, SCD41_I2C_ADDR_62);
    scd41.wakeUp();
    scd41.stopPeriodicMeasurement();
    scd41.reinit();  // Reload the configuration from EEPROM
    // uint64_t serialNumber = 0;
    // scd41.getSerialNumber(serialNumber);

    printScd41Config("");
    scd41.setTemperatureOffset(3.5f);  // Default: 4 C, Sweet spot: 3.5 C
    scd41.setSensorAltitude(310);      // Set altitude to 0m (default)
    scd41.persistSettings();           // Save settings to EEPROM
    printScd41Config("After");
    scd41.startPeriodicMeasurement();

    // SHT40
    sht40.begin(Wire, SHT40_I2C_ADDR_44);
    // SGP41
    sgp41.begin(Wire);
    // AHT21
    checkSensor(aht21.begin(), "AHT21");
    // ENS160
    ens160.begin();
    checkSensor(ens160.available(), "ENS160");
    ens160.setMode(ENS160_OPMODE_STD);
    // DHT22
    dht.begin();

    wifiManagerSetup();
    otaWebUpdateSetup();
    syncRtc();
    setupAlarm();

    mqttInit();
    tWifiDisconnectedDetect.start();
#ifndef _20SecTest
    tSgp41HeatingOff.start();
#endif
}

//******************************** Loop *************************************//
void loop() {
    tWifiDisconnectedDetect.update();
    statusLed.loop();  // MUST call the led.loop() function in loop()
    resetWifiBt.loop();
    server.handleClient();  // OTA Web Update
    tConnectMqtt.update();
    tReconnectMqtt.update();

#ifndef _20SecTest
    tSgp41HeatingOn.update();
    tSgp41HeatingOff.update();
#endif

    if (rtcTrigger) {
        rtcTrigger = false;
        fetchData();
    }
}