#pragma once

#include <Arduino.h>

#define _LOG_  // Comment out to disable debug Serial prints across all files
#ifdef _LOG_
#define _serialBegin(...) Serial.begin(__VA_ARGS__)
#define _logf(...) Serial.printf(__VA_ARGS__)
#define _logF(...) Serial.print(F(__VA_ARGS__))
#else
#define _serialBegin(...)
#define _logf(...)
#define _logF(...)
#endif