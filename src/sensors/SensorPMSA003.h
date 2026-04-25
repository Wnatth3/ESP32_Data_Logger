#pragma once
#include <PMserial.h>
#include "Debug.h"

class SensorPMSA003 {
public:
    uint16_t pm010 = 0;
    uint16_t pm025 = 0;
    uint16_t pm100 = 0;

    // rxPin = 18, txPin = 19 by default (matches original wiring)
    SensorPMSA003(uint8_t rxPin = 18, uint8_t txPin = 19)
        : _pms(PMSx003, rxPin, txPin) {}

    void begin() {
        _pms.init();
    }

    void read() {
        _pms.read();
        if (_pms) {
            pm010 = _pms.pm01;
            pm025 = _pms.pm25;
            pm100 = _pms.pm10;
        }
    }

    void print() {
        _deF("PMSA003A: PM1.0: "); _de(pm010);
        _deF(" ug/m3 | PM2.5: "); _de(pm025);
        _deF(" ug/m3 | PM10: ");  _de(pm100);
        _delnF(" ug/m3");
    }

    void addJson(JsonArray& doc) {
        JsonObject obj         = doc.add<JsonObject>();
        obj["measurement"]     = "pmsa003a";
        JsonObject fields      = obj["fields"].to<JsonObject>();
        fields["pm010"]        = pm010;
        fields["pm025"]        = pm025;
        fields["pm100"]        = pm100;
    }

private:
    SerialPM _pms;
};
