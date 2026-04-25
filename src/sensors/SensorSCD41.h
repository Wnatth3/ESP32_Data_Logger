#pragma once
#include <SensirionI2cScd4x.h>
#include "Debug.h"

// Guard against library NO_ERROR conflict
#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

class SensorSCD41 {
public:
    float    temp = 0;
    float    humi = 0;
    uint16_t co2  = 0;

    void begin() {
        _scd41.begin(Wire, SCD41_I2C_ADDR_62);
        _scd41.wakeUp();
        _scd41.stopPeriodicMeasurement();
        _scd41.reinit();
        _scd41.setTemperatureOffset(3.5f);  // Sweet spot: 3.5 C
        _scd41.setSensorAltitude(310);
        _scd41.persistSettings();
        _scd41.startPeriodicMeasurement();
    }

    void read() {
        bool dataReady = false;
        _error = _scd41.getDataReadyStatus(dataReady);
        if (_error != NO_ERROR) { _printError("getDataReadyStatus"); return; }

        while (!dataReady) {
            delay(100);
            _error = _scd41.getDataReadyStatus(dataReady);
            if (_error != NO_ERROR) { _printError("getDataReadyStatus"); return; }
        }

        _error = _scd41.readMeasurement(co2, temp, humi);
        if (_error != NO_ERROR) { _printError("readMeasurement"); }
    }

    void print() {
        _deF("SCD41: Temp: ");  _de(temp, 2);
        _deF(" C | Humi: ");    _de(humi, 2);
        _deF(" % | CO2: ");     _de(co2);
        _delnF(" ppm");
    }

    void addJson(JsonArray& doc) {
        JsonObject obj     = doc.add<JsonObject>();
        obj["measurement"] = "scd41";
        JsonObject fields  = obj["fields"].to<JsonObject>();
        fields["temp"]     = temp;
        fields["humi"]     = humi;
        fields["co2"]      = co2;
    }

private:
    SensirionI2cScd4x _scd41;
    int16_t           _error = NO_ERROR;
    char              _errMsg[64];

    void _printError(const char* ctx) {
        _deF("SCD41 error in "); _de(ctx); _deF(": ");
        errorToString(_error, _errMsg, sizeof(_errMsg));
        _deln(_errMsg);
    }
};
