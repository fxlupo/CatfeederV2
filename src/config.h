// =============================================================================
// CatFeeder ESP32 – Konfiguration & Datenstrukturen
// =============================================================================
#pragma once
#include <Arduino.h>
#include <Preferences.h>

// ─── Firmware ───────────────────────────────────────────────────────────────
#define FW_VERSION             "1.2.0"
#define CONFIG_SCHEMA_VERSION  8

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
#define STEPPER_DEFAULT_SPEED  1200    // Steps/s
#define STEPPER_DEFAULT_PULSE_US 10    // STEP High-Zeit µs, konservativ fuer TMC/DRV
#define STEPPER_DEFAULT_DIR_SETUP_US 300 // DIR-Setup-Zeit vor erstem STEP
#define STEPPER_DEFAULT_HOLD_MS 0      // Treiber nach Lauf sofort loesen
#define STEPPER_PULSE_US       STEPPER_DEFAULT_PULSE_US

// ─── Servo ──────────────────────────────────────────────────────────────────
#define SERVO_MIN_US           500
#define SERVO_MAX_US           2500
#define SERVO_DEFAULT_OPEN     90
#define SERVO_DEFAULT_CLOSE    0
#define SERVO_DEFAULT_SPEED_DPS 1000   // Grad/s, hohe Werte fahren direkt

// ─── Sensoren ───────────────────────────────────────────────────────────────
#define FILL_EMPTY_MM          70     // VL53L0X Abstand = Behälter leer
#define FILL_FULL_MM           10      // VL53L0X Abstand = Behälter voll
#define IR_THRESHOLD           2048    // Analog-Schwelle IR
#define INA_OVERCURRENT_MA     2000    // Überstrom-Grenze mA
#define STEPPER_DEFAULT_BLOCK_MA  1500 // Blockierstrom-Schwelle mA (Stepper-Stall)
#define BLOCK_DEFAULT_RETRIES        2 // Max. Wiederholversuche bei Blockade
#define BLOCK_DEFAULT_REVERSE_STEPS 1000 // Rückwärts-Steps zur Freigabe
#define BLOCK_DEFAULT_MIN_ROT_PCT    5 // Min. Rotation % pro 64-Schritt-Fenster

// ─── Benachrichtigungen ─────────────────────────────────────────────────────
#define WA_USERS               2       // Konfigurierbare WhatsApp-Empfänger

// ─── Fütterung ──────────────────────────────────────────────────────────────
#define MAX_SLOTS              4       // Fütterungszeiten pro Tag
#define DEFAULT_STEPS_PER_GRAM 300     // Stepper-Schritte pro Gramm
#define DEFAULT_FEED_GRAMS     20      // Standard-Fütterungsmenge g
#define FEED_LOG_SIZE          20      // Einträge im RAM-Ringpuffer

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
    uint16_t servoSpeedDPS;  // Grad pro Sekunde

    // VL53L0X
    uint16_t fillEmptyMM;
    uint16_t fillFullMM;

    // IR
    uint16_t irThreshold;

    // Sonstiges
    int8_t   utcOffset;      // Stunden
    bool     dst;            // Sommerzeit
    char     hostname[33];

    // Stepper-Feinabstimmung
    uint16_t stepperPulseUS; // STEP High-Zeit in µs
    bool     stepperInvertDir;
    uint16_t stepperDirSetupUS;
    uint16_t stepperHoldMS;

    // Blockadeerkennung
    uint16_t stepperBlockMA;    // Strom-Schwelle INA219 (mA)
    uint8_t  blockRetries;      // Max. Versuche nach Blockade
    uint16_t blockReverseSteps; // Rückwärts-Steps zur Freigabe
    uint8_t  blockMinRotPct;    // Min. Rotation % pro 64-Schritt-Fenster

    // WhatsApp-Benachrichtigungen
    struct WaUser {
        bool active;
        char phone[20];    // "+49176..."
        char apikey[32];   // CallMeBot API-Key
    } waUsers[WA_USERS];

    // Fütterung
    uint16_t defaultGrams;   // Standard-Menge für manuelles Füttern
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
// Event-Log (RAM-Ringpuffer, nicht persistent)
// ═════════════════════════════════════════════════════════════════════════════

struct FeedEvent {
    char     timeStr[20];     // Zeitstempel beim Start "DD.MM.YYYY HH:MM:SS"
    bool     isAuto;          // true = Zeitplan, false = manuell
    uint16_t grams;
    uint8_t  servo;
    uint16_t distBefore;      // VL53L0X mm vor Fütterung
    uint16_t distAfter;       // VL53L0X mm nach Fütterung
    uint8_t  fillBefore;      // Füllstand % vor
    uint8_t  fillAfter;       // Füllstand % nach
    uint16_t ir1Pulses;       // Flanken-Impulse IR1 während Stepper-Lauf
    uint16_t ir2Pulses;       // Flanken-Impulse IR2 während Stepper-Lauf
    bool     feedAborted;     // Fütterung durch Blockade abgebrochen
    uint8_t  blockRetryCount; // Anzahl Blockade-Versuche (0 = keine Blockade)
};

struct FeedLog {
    FeedEvent entries[FEED_LOG_SIZE];
    uint8_t   head  = 0;   // nächste Schreibposition
    uint8_t   count = 0;   // gültige Einträge (0..FEED_LOG_SIZE)

    void add(const FeedEvent &e) {
        entries[head] = e;
        head = (head + 1) % FEED_LOG_SIZE;
        if (count < FEED_LOG_SIZE) count++;
    }
    // i=0 → ältester Eintrag
    const FeedEvent& get(uint8_t i) const {
        if (count < FEED_LOG_SIZE) return entries[i];
        return entries[(head + i) % FEED_LOG_SIZE];
    }
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
