#pragma once
#include <DHT.h>
#include "Debug.h"

class SensorDHT22 {
public:
    float temp = 0;
    float humi = 0;

    SensorDHT22(uint8_t pin = 32) : _dht(pin, DHT22) {}

    void begin() {
        _dht.begin();
    }

    void read() {
        temp = _dht.readTemperature();
        humi = _dht.readHumidity();
    }

    void print() {
        _deF("DHT22: Temp: "); _de(temp, 2);
        _deF(" C | Humi: ");   _de(humi, 2);
        _delnF(" %");
    }

    void addJson(JsonArray& doc) {
        JsonObject obj     = doc.add<JsonObject>();
        obj["measurement"] = "dht22";
        JsonObject fields  = obj["fields"].to<JsonObject>();
        fields["temp"]     = temp;
        fields["humi"]     = humi;
    }

private:
    DHT _dht;
};
