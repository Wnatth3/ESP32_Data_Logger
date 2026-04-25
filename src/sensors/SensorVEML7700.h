#pragma once
#include "Adafruit_VEML7700.h"
#include "Debug.h"

class SensorVEML7700 {
public:
    float lux = 0;

    void begin() {
        if (!_veml.begin()) _delnF("VEML7700 is not found");
    }

    void read() {
        lux = _veml.readLux(VEML_LUX_AUTO);
    }

    void print() {
        _deF("VEML7700: Lux: "); _de(lux, 2); _deln();
    }

    void addJson(JsonArray& doc) {
        JsonObject obj     = doc.add<JsonObject>();
        obj["measurement"] = "veml7700";
        obj["fields"]["lux"] = lux;
    }

private:
    Adafruit_VEML7700 _veml;
};
