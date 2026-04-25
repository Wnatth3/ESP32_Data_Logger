#pragma once
#include <SensirionI2CSgp41.h>
#include <SensirionI2cSht4x.h>
#include <VOCGasIndexAlgorithm.h>
#include <NOxGasIndexAlgorithm.h>
#include "Debug.h"

class SensorSHT40SGP41 {
public:
    float   tempSht40 = 0;
    float   humiSht40 = 0;
    int32_t vocIdx    = 0;
    int32_t noxIdx    = 0;

    void begin() {
        _sht40.begin(Wire, SHT40_I2C_ADDR_44);
        _sgp41.begin(Wire);
    }

    // Call this every ~1 second during the SGP41 conditioning/preheat phase.
    void read() {
        uint16_t srawVoc = 0, srawNox = 0;
        uint16_t compRh  = 0x8000;  // SGP41 default ticks
        uint16_t compT   = 0x6666;

        // SHT40: measure for SGP41 T/RH compensation
        _error = _sht40.measureHighPrecision(tempSht40, humiSht40);
        if (_error) {
            _printError("SHT40 measureHighPrecision");
            _delnF("SGP41: using default T/RH compensation");
            compRh = 0x8000;
            compT  = 0x6666;
        } else {
            compT  = static_cast<uint16_t>((tempSht40 + 45) * 65535 / 175);
            compRh = static_cast<uint16_t>(humiSht40 * 65535 / 100);
        }

        // SGP41: conditioning for first 10 calls, then measure
        if (_conditioning_s > 0) {
            _error = _sgp41.executeConditioning(compRh, compT, srawVoc);
            _conditioning_s--;
        } else {
            _error = _sgp41.measureRawSignals(compRh, compT, srawVoc, srawNox);
        }

        if (_error) {
            _printError("SGP41 measureRawSignals");
        } else {
            vocIdx = _voc.process(srawVoc);
            noxIdx = _nox.process(srawNox);
        }
    }

    bool isConditioned() const { return _conditioning_s == 0; }

    void print() {
        _deF("SHT40: Temp: ");   _de(tempSht40, 2);
        _deF(" C | Humi: ");     _de(humiSht40, 2); _delnF(" %");
        _deF("SGP41: VOC Idx: "); _de(vocIdx);
        _deF(" | NOx Idx: ");     _deln(noxIdx);
    }

    void addJsonSht40(JsonArray& doc) {
        JsonObject obj     = doc.add<JsonObject>();
        obj["measurement"] = "sht40";
        JsonObject fields  = obj["fields"].to<JsonObject>();
        fields["temp"]     = tempSht40;
        fields["humi"]     = humiSht40;
    }

    void addJsonSgp41(JsonArray& doc) {
        JsonObject obj     = doc.add<JsonObject>();
        obj["measurement"] = "sgp41";
        JsonObject fields  = obj["fields"].to<JsonObject>();
        fields["vocIdx"]   = vocIdx;
        fields["noxIdx"]   = noxIdx;
    }

private:
    SensirionI2cSht4x    _sht40;
    SensirionI2CSgp41    _sgp41;
    VOCGasIndexAlgorithm _voc;
    NOxGasIndexAlgorithm _nox;

    uint16_t _conditioning_s = 10;
    uint16_t _error          = 0;
    char     _errMsg[256];

    void _printError(const char* ctx) {
        errorToString(_error, _errMsg, 256);
        _deF(ctx); _deF(" error: "); _deln(_errMsg);
    }
};
