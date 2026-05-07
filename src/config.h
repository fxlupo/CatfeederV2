// =============================================================================
// CatFeeder ESP32 – Konfiguration & Datenstrukturen
// =============================================================================
#pragma once
#include <Arduino.h>
#include <Preferences.h>

// ─── Firmware ───────────────────────────────────────────────────────────────
#define FW_VERSION             "1.0.0"

// ─── WLAN ───────────────────────────────────────────────────────────────────
#define WIFI_AP_SSID           "CatFeeder-Setup"
#define WIFI_AP_PASS           "katze1234"
#define WIFI_CONNECT_TIMEOUT   15000   // ms

#if __has_include("wifi_credentials.h")
#include "wifi_credentials.h"
#endif

#ifndef WIFI_DEFAULT_SSID
#define WIFI_DEFAULT_SSID      ""
#endif

#ifndef WIFI_DEFAULT_PASS
#define WIFI_DEFAULT_PASS      ""
#endif

// ─── Stepper ────────────────────────────────────────────────────────────────
#define STEPPER_STEPS_REV      200     // NEMA17 Standard (1,8° / Schritt)
#define STEPPER_DEFAULT_SPEED  600     // Steps/s
#define STEPPER_PULSE_US       5       // Mindest-Pulsdauer µs

// ─── Servo ──────────────────────────────────────────────────────────────────
#define SERVO_MIN_US           500
#define SERVO_MAX_US           2500
#define SERVO_DEFAULT_OPEN     90
#define SERVO_DEFAULT_CLOSE    0

// ─── Sensoren ───────────────────────────────────────────────────────────────
#define FILL_EMPTY_MM          300     // VL53L0X Abstand = Behälter leer
#define FILL_FULL_MM           30      // VL53L0X Abstand = Behälter voll
#define IR_THRESHOLD           2048    // Analog-Schwelle IR
#define INA_OVERCURRENT_MA     2000    // Überstrom-Grenze mA

// ─── Fütterung ──────────────────────────────────────────────────────────────
#define MAX_SLOTS              8       // Fütterungszeiten pro Tag
#define DEFAULT_STEPS_PER_GRAM 10      // Stepper-Schritte pro Gramm

// ═════════════════════════════════════════════════════════════════════════════
// Datenstrukturen
// ═════════════════════════════════════════════════════════════════════════════

struct FeedSlot {
    bool     active;
    uint8_t  hour;           // 0-23
    uint8_t  minute;         // 0-59
    uint16_t grams;          // Portionsgröße
    uint8_t  servo;          // 0 = beide, 1 = S1, 2 = S2
    bool     doneToday;      // Bereits ausgeführt?
};

struct Config {
    // WLAN
    char     ssid[33];
    char     pass[65];

    // Fütterungszeiten
    FeedSlot slots[MAX_SLOTS];

    // Stepper
    uint16_t stepsPerGram;
    uint16_t stepperSpeed;   // Steps/s

    // Servo-Winkel
    uint8_t  s1Open;
    uint8_t  s1Close;
    uint8_t  s2Open;
    uint8_t  s2Close;

    // VL53L0X
    uint16_t fillEmptyMM;
    uint16_t fillFullMM;

    // IR
    uint16_t irThreshold;

    // Sonstiges
    int8_t   utcOffset;      // Stunden
    bool     dst;            // Sommerzeit
    char     hostname[33];
};

struct Status {
    // INA219
    float    busV;
    float    currentMA;
    float    powerMW;

    // VL53L0X
    uint16_t distMM;
    uint8_t  fillPct;

    // AS5600
    float    angleDeg;

    // IR
    uint16_t ir1Analog;
    uint16_t ir2Analog;
    bool     ir1Digital;
    bool     ir2Digital;

    // Endschalter
    bool     s1Open;
    bool     s1Close;
    bool     s2Open;
    bool     s2Close;

    // Sensor-Health
    bool     ok_ina;
    bool     ok_vl;
    bool     ok_as;
    bool     ok_rtc;

    // System
    bool     wifiOK;
    bool     apMode;
    bool     otaReady;
    uint16_t otaPort;
    int32_t  wifiRSSI;
    char     timeStr[20];    // "HH:MM:SS DD.MM.YYYY"
    char     ip[16];         // "255.255.255.255"
    char     hostname[33];
    char     wifiMode[5];    // "STA" / "AP"
    char     otaPhase[16];
    char     lastOtaError[24];
    uint32_t uptimeS;
    uint32_t heap;
    uint32_t feeds;          // Gesamtfütterungen
    bool     overcurrent;
    bool     fillLow;
};

// ═════════════════════════════════════════════════════════════════════════════
// NVS Konfigurations-Manager
// ═════════════════════════════════════════════════════════════════════════════

class CfgManager {
public:
    void     begin();
    void     load(Config &c);
    void     save(const Config &c);
    void     defaults(Config &c);
    void     saveFeedCount(uint32_t n);
    uint32_t loadFeedCount();
private:
    Preferences _p;
};
