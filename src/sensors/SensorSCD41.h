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
    static constexpr float    TEMP_OFFSET_C    = 3.5f;   // Calibrated offset
    static constexpr uint16_t SITE_ALTITUDE_M  = 310;    // Site elevation in meters

    float    temp = 0;
    float    humi = 0;
    uint16_t co2  = 0;

    // persistSettings() writes the SCD41 EEPROM, rated ~50k cycles. Under
    // deep sleep begin() runs on every wake, so pass persist=true only on a
    // cold boot; the offset/altitude registers themselves are volatile and
    // must still be re-applied each time.
    void begin(bool persist = true) {
        _scd41.begin(Wire, SCD41_I2C_ADDR_62);
        _scd41.wakeUp();
        _scd41.stopPeriodicMeasurement();
        _scd41.reinit();
        _scd41.setTemperatureOffset(TEMP_OFFSET_C);
        _scd41.setSensorAltitude(SITE_ALTITUDE_M);
        if (persist) _scd41.persistSettings();
        _scd41.startPeriodicMeasurement();
    }

    // Returns true when a fresh reading was captured.
    // Returns false if data is not yet ready or on error — caller should retry later.
    bool read() {
        bool dataReady = false;
        _error = _scd41.getDataReadyStatus(dataReady);
        if (_error != NO_ERROR) { _printError("getDataReadyStatus"); return false; }
        if (!dataReady) return false;

        _error = _scd41.readMeasurement(co2, temp, humi);
        if (_error != NO_ERROR) { _printError("readMeasurement"); return false; }
        return true;
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
