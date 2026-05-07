// =============================================================================
// CatFeeder ESP32 – Sensor-Manager
// INA219 · VL53L0X · AS5600 · DS3231 · IR (2×analog + 2×digital)
// =============================================================================
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <Adafruit_VL53L0X.h>
#include <Adafruit_AS5600.h>
#include <RTClib.h>
#include "config.h"
#include "pins.h"

class Sensors {
public:
    void begin();
    void update();                                // Alle Sensoren lesen
    void fillStatus(Status &st, const Config &c); // Status-Struct befüllen

    // RTC
    String   timeString();
    uint8_t  hour();
    uint8_t  minute();
    bool     setTime(uint16_t y, uint8_t mo, uint8_t d,
                     uint8_t h, uint8_t mi, uint8_t s);

    // Health-Flags
    bool ok_ina  = false;
    bool ok_vl   = false;
    bool ok_as   = false;
    bool ok_rtc  = false;

private:
    Adafruit_INA219  _ina;
    Adafruit_VL53L0X _vl;
    Adafruit_AS5600  _as;
    RTC_DS3231       _rtc;

    // Cached
    float    _busV      = 0;
    float    _curMA     = 0;
    float    _powMW     = 0;
    uint16_t _distMM    = 0;
    float    _angle     = 0;
    uint16_t _ir1a      = 0;
    uint16_t _ir2a      = 0;
    bool     _ir1d      = false;
    bool     _ir2d      = false;

    uint32_t _lastRead  = 0;
    static constexpr uint32_t READ_INTERVAL = 500; // ms
};
