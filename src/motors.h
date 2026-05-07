// =============================================================================
// CatFeeder ESP32 – Motor-Steuerung
// Stepper (NEMA17 via DRV8825) + 2× Servo
// =============================================================================
#pragma once
#include <Arduino.h>
#include <ESP32Servo.h>
#include "config.h"
#include "pins.h"

class Motors {
public:
    void begin(const Config &c);
    void loop();                              // Nicht-blockierend (Stepper-ISR)

    // ── Stepper ─────────────────────────────────────────────────────────────
    void drvEnable(bool on);
    void configureStepper(const Config &c);
    void setSpeed(uint16_t stepsPerSec);
    void run(int32_t steps);                  // +vorwärts, −rückwärts
    void stop();
    bool running()  { return _remain > 0; }
    int32_t pos()   { return _pos; }
    void resetPos() { _pos = 0; }

    // ── Servos ──────────────────────────────────────────────────────────────
    void s1Open(const Config &c);
    void s1Close(const Config &c);
    void s2Open(const Config &c);
    void s2Close(const Config &c);
    void setAngle(uint8_t num, uint8_t deg);  // 1 oder 2
    void detachServos();

    // ── Fütterung (blockierend) ─────────────────────────────────────────────
    // Gibt `grams` Gramm aus. `servo`: 0=beide, 1=S1, 2=S2
    void dispense(uint16_t grams, uint8_t servo, const Config &c);

private:
    Servo _sv1, _sv2;

    // Stepper-State
    volatile int32_t _remain = 0;
    int32_t  _pos     = 0;
    int8_t   _dir     = 1;
    uint32_t _ivlUS   = 1666;     // µs zwischen Steps
    uint32_t _lastUS  = 0;
    uint16_t _pulseUS = STEPPER_DEFAULT_PULSE_US;
    bool     _dirInvert = false;
    bool     _enabled = false;
    int16_t  _sv1Pos  = -1;
    int16_t  _sv2Pos  = -1;

    void _pulse();
    void _writeServo(Servo &servo, int16_t &current, uint8_t pin, uint8_t deg, const Config &c);
};
