#pragma once
#include "MHZ19.h"
#include "Debug.h"

class SensorMHZ19B {
public:
    int co2 = 0;

    // rxPin = 16, txPin = 17 by default (matches original wiring)
    void begin(uint8_t rxPin = 16, uint8_t txPin = 17) {
        _serial.begin(9600, SERIAL_8N1, rxPin, txPin);
        _mhz19.begin(_serial);
        _mhz19.autoCalibration();
    }

    void read() {
        co2 = _mhz19.getCO2();
    }

    void print() {
        _deF("MHZ19B: CO2: "); _de(co2); _delnF(" ppm");
    }

    void addJson(JsonArray& doc) {
        JsonObject obj       = doc.add<JsonObject>();
        obj["measurement"]   = "mhz19b";
        obj["fields"]["co2"] = co2;
    }

private:
    HardwareSerial _serial{2};  // UART2 on ESP32
    MHZ19          _mhz19;
};
