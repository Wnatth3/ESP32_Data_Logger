#pragma once
#include <bsec.h>
#include "Debug.h"

class SensorBME680 {
public:
    float temp     = 0;
    float humi     = 0;
    float pressure = 0;
    float gasRes   = 0;

    void begin() {
        _iaqSensor.begin(BME68X_I2C_ADDR_HIGH, Wire);
        _iaqSensor.updateSubscription(_sensorList, 13, BSEC_SAMPLE_RATE_LP);
    }

    void read() {
        if (_iaqSensor.run()) {
            temp     = _iaqSensor.temperature;
            humi     = _iaqSensor.humidity;
            pressure = _iaqSensor.pressure / 100.f;
            gasRes   = _iaqSensor.gasResistance;
        }
    }

    void print() {
        _deF("BME680: Temp: ");   _de(temp, 2);
        _deF(" C | Humi: ");      _de(humi, 2);
        _deF(" % | Gas Resist: "); _de(gasRes, 2);
        _delnF(" kohm");
    }

    void addJson(JsonArray& doc) {
        JsonObject obj         = doc.add<JsonObject>();
        obj["measurement"]     = "bme680";
        JsonObject fields      = obj["fields"].to<JsonObject>();
        fields["temp"]         = temp;
        fields["humi"]         = humi;
        fields["press"]        = pressure;
        fields["gasRes"]       = gasRes;
    }

private:
    Bsec _iaqSensor;

    bsec_virtual_sensor_t _sensorList[13] = {
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
        BSEC_OUTPUT_GAS_PERCENTAGE
    };
};
