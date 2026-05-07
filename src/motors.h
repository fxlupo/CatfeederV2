// =============================================================================
// CatFeeder ESP32 – Motor-Steuerung
// Stepper (NEMA17 via DRV8825) + 2× Servo
// =============================================================================
#pragma once
#include <Arduino.h>
#include <ESP32Servo.h>
#include "config.h"
#include "pins.h"
#include "sensors.h"

// Fütterungs-State-Machine
enum DispState : uint8_t {
    DS_IDLE = 0,
    DS_SERVO_OPEN,       // Servos öffnen, 600 ms warten
    DS_STEPPER,          // Stepper mit Blockadeerkennung
    DS_STEPPER_WAIT,     // 400 ms Pause nach Stepper
    DS_SERVO_CLOSE,      // Servos schließen, 1000 ms warten
    DS_SHAKE_OPEN,       // Nachklappen: Servos auf, 500 ms warten
    DS_FINAL_CLOSE,      // Garantiert schließen, 1000 ms warten
    DS_DONE,             // Servos detachen, zurück zu IDLE
    DS_BLOCK_REVERSE,    // Rückwärts-Fahrt zur Blockadefreigabe
    DS_BLOCK_WAIT,       // 300 ms Pause nach Reverse
    DS_BLOCK_RETRY       // Wiederholung oder Abbruch
};

class Motors {
public:
    void begin(const Config &c, Sensors &se);
    void loop();

    // ── Stepper ─────────────────────────────────────────────────────────────
    void drvEnable(bool on);
    void configureStepper(const Config &c);
    void setSpeed(uint16_t stepsPerSec);
    void run(int32_t steps);
    bool moveBlocking(int32_t steps, const Config &c); // true = OK, false = Blockade
    void stop();
    bool running()  { return _remain > 0; }
    int32_t pos()   { return _pos; }
    void resetPos() { _pos = 0; }

    // ── Servos ──────────────────────────────────────────────────────────────
    void s1Open(const Config &c);
    void s1Close(const Config &c);
    void s2Open(const Config &c);
    void s2Close(const Config &c);
    void setAngle(uint8_t num, uint8_t deg);
    void detachServos();

    // ── Selbsttest beim Start ───────────────────────────────────────────────
    void selfTest(const Config &c);

    // ── Fütterung (nicht-blockierend, State Machine) ────────────────────────
    void dispense(uint16_t grams, uint8_t servo, const Config &c);
    bool dispensing()      { return _dispState != DS_IDLE; }
    bool feedAborted()     { return _feedAborted; }
    uint8_t blockRetries() { return _dispRetries; }

    // ── IR-Impulszähler ─────────────────────────────────────────────────────
    uint16_t irCount1()    { return _irCount1; }
    uint16_t irCount2()    { return _irCount2; }

private:
    Servo    _sv1, _sv2;
    Sensors *_sensors = nullptr;

    // Stepper-State
    volatile int32_t _remain     = 0;
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

    // IR-Impulszähler
    bool     _countIR  = false;
    uint16_t _irCount1 = 0;
    uint16_t _irCount2 = 0;

    // Blockadeerkennung
    bool     _blockDetect   = false;
    uint16_t _blockMA       = 1500;  // aus Config
    float    _blockMinAngle = 34.0f; // aus Config (blockMinRotPct × 115,2°)

    // Fütterungs-State-Machine
    DispState     _dispState   = DS_IDLE;
    bool          _dispNew     = false;
    uint32_t      _dispTimer   = 0;
    uint16_t      _dispGrams   = 0;
    uint8_t       _dispServo   = 0;
    const Config *_dispCfg     = nullptr;
    uint8_t       _dispRetries = 0;   // Blockade-Wiederholversuche
    bool          _feedAborted = false;

    void _pulse();
    void _writeServo(Servo &servo, int16_t &current, uint8_t pin, uint8_t deg, const Config &c);
    void _dispenseLoop();
    void _dispNext(DispState s);
    void _svOpen();
    void _svClose();
    void _svCloseAll();
};
