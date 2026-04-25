#pragma once
#include <Adafruit_AHTX0.h>
#include <ScioSense_ENS160.h>
#include "Debug.h"

class SensorENS160AHT21 {
public:
    float   tempAht21 = 0;
    float   humiAht21 = 0;
    uint8_t aqi       = 0;
    uint8_t tvoc      = 0;
    uint16_t eco2     = 0;

    void begin() {
        if (!_aht21.begin()) _delnF("AHT21 is not found");
        _ens160.begin();
        if (!_ens160.available()) _delnF("ENS160 is not found");
        _ens160.setMode(ENS160_OPMODE_STD);
    }

    void read() {
        sensors_event_t humiEvent, tempEvent;
        _aht21.getEvent(&humiEvent, &tempEvent);
        tempAht21 = tempEvent.temperature;
        humiAht21 = humiEvent.relative_humidity;

        _ens160.measure(true);
        _ens160.measureRaw(true);
        aqi  = _ens160.getAQI();
        tvoc = _ens160.getTVOC();
        eco2 = _ens160.geteCO2();
    }

    void print() {
        _deF("AHT21: Temp: "); _de(tempAht21, 2);
        _deF(" C | Humi: ");   _de(humiAht21, 2); _delnF(" %");
        _deF("ENS160: AQI: "); _de(aqi);
        _deF(" | TVOC: ");     _de(tvoc);
        _deF(" ppb | eCO2: "); _de(eco2); _delnF(" ppm");
    }

    void addJsonAht21(JsonArray& doc) {
        JsonObject obj     = doc.add<JsonObject>();
        obj["measurement"] = "aht21";
        JsonObject fields  = obj["fields"].to<JsonObject>();
        fields["temp"]     = tempAht21;
        fields["humi"]     = humiAht21;
    }

    void addJsonEns160(JsonArray& doc) {
        JsonObject obj     = doc.add<JsonObject>();
        obj["measurement"] = "ens160";
        JsonObject fields  = obj["fields"].to<JsonObject>();
        fields["aqi"]      = aqi;
        fields["tVoc"]     = tvoc;
        fields["eCo2"]     = eco2;
    }

private:
    Adafruit_AHTX0      _aht21;
    ScioSense_ENS160    _ens160{ENS160_I2CADDR_1};  // 0x53
};
