// =============================================================================
// CatFeeder ESP32 – Motor-Steuerung
// Stepper (NEMA17 via DRV8825) + 2× Servo
// =============================================================================
#pragma once
#include <Arduino.h>
#include <ESP32Servo.h>
#include "config.h"
#include "pins.h"

// Fütterungs-State-Machine
enum DispState : uint8_t {
    DS_IDLE = 0,
    DS_SERVO_OPEN,      // Servos öffnen, 600 ms warten
    DS_STEPPER,         // Stepper starten, auf Fertig warten
    DS_STEPPER_WAIT,    // 400 ms Pause nach Stepper
    DS_SERVO_CLOSE,     // Servos schließen, 1000 ms warten
    DS_SHAKE_OPEN,      // Nachklappen: Servos auf, 500 ms warten
    DS_SHAKE_CLOSE,     // Nachklappen: Servos zu, 400 ms warten
    DS_DONE             // Servos detachen, zurück zu IDLE
};

class Motors {
public:
    void begin(const Config &c);
    void loop();

    // ── Stepper ─────────────────────────────────────────────────────────────
    void drvEnable(bool on);
    void configureStepper(const Config &c);
    void setSpeed(uint16_t stepsPerSec);
    void run(int32_t steps);                  // +vorwärts, −rückwärts
    void moveBlocking(int32_t steps, const Config &c);
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

    // ── Selbsttest beim Start ───────────────────────────────────────────────
    // Stepper 50 vor / 50 zurück, dann Servo 1 und 2 auf/zu (blockierend)
    void selfTest(const Config &c);

    // ── Fütterung (nicht-blockierend, State Machine) ────────────────────────
    // Startet den Ablauf und kehrt sofort zurück.
    // servo: 0=beide, 1=S1, 2=S2
    void dispense(uint16_t grams, uint8_t servo, const Config &c);
    bool dispensing() { return _dispState != DS_IDLE; }

private:
    Servo _sv1, _sv2;

    // Stepper-State
    volatile int32_t _remain = 0;
    int32_t  _pos        = 0;
    int8_t   _dir        = 1;
    uint32_t _ivlUS      = 1666;
    uint32_t _lastUS     = 0;
    uint16_t _pulseUS    = STEPPER_DEFAULT_PULSE_US;
    uint16_t _dirSetupUS = STEPPER_DEFAULT_DIR_SETUP_US;
    uint16_t _holdMS     = STEPPER_DEFAULT_HOLD_MS;
    bool     _dirInvert  = false;
    bool     _enabled    = false;
    int16_t  _sv1Pos     = -1;
    int16_t  _sv2Pos     = -1;

    // Fütterungs-State-Machine
    DispState        _dispState = DS_IDLE;
    bool             _dispNew   = false;    // Entry-Flag: true beim ersten Tick im State
    uint32_t         _dispTimer = 0;
    uint16_t         _dispGrams = 0;
    uint8_t          _dispServo = 0;
    const Config    *_dispCfg   = nullptr;

    void _pulse();
    void _writeServo(Servo &servo, int16_t &current, uint8_t pin, uint8_t deg, const Config &c);
    void _dispenseLoop();
    void _dispNext(DispState s);

    // Hilfsfunktionen: öffnen/schließen je nach _dispServo
    void _svOpen();
    void _svClose();
};
