// =============================================================================
// CatFeeder ESP32 – Webserver & REST-API
// =============================================================================
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "sensors.h"
#include "motors.h"

class Web {
public:
    void begin(Config &c, Status &st, Sensors &se, Motors &mo, CfgManager &cm, FeedLog &fl);
    void loop();
    bool apMode() { return _ap; }
    String ip();

    // Flag: Asynchrone Fütterung angefordert
    bool     feedRequested = false;
    uint16_t feedGrams     = 0;
    uint8_t  feedServo     = 0;

private:
    AsyncWebServer   _srv{80};
    AsyncEventSource _sse{"/events"};

    Config     *_c  = nullptr;
    Status     *_st = nullptr;
    Sensors    *_se = nullptr;
    Motors     *_mo = nullptr;
    CfgManager *_cm = nullptr;
    FeedLog    *_fl = nullptr;

    bool     _ap = false;
    uint32_t _lastSSE = 0;

    void _wifi();
    void _routes();
    void _sendSSE();
    String _html();
};
