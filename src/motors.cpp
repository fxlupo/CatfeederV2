// =============================================================================
// CatFeeder ESP32 – Motor-Steuerung Implementation
// =============================================================================
#include "motors.h"

void Motors::begin(const Config &c) {
    pinMode(PIN_STEP,   OUTPUT);
    pinMode(PIN_DIR,    OUTPUT);
    pinMode(PIN_EN_DRV, OUTPUT);
    digitalWrite(PIN_STEP, LOW);
    digitalWrite(PIN_DIR,  LOW);
    drvEnable(false);

    configureStepper(c);

    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    _sv1.setPeriodHertz(50);
    _sv2.setPeriodHertz(50);

    Serial.println(F("[Motor] Init fertig"));
}

// ═══════════════════════ Stepper ════════════════════════════════════════════

void Motors::drvEnable(bool on) {
    _enabled = on;
    digitalWrite(PIN_EN_DRV, on ? LOW : HIGH);
}

void Motors::configureStepper(const Config &c) {
    _pulseUS    = constrain(c.stepperPulseUS,    2,    50);
    _dirSetupUS = constrain(c.stepperDirSetupUS, 0,  2000);
    _holdMS     = constrain(c.stepperHoldMS,     0,  5000);
    _dirInvert  = c.stepperInvertDir;
    setSpeed(c.stepperSpeed);
}

void Motors::setSpeed(uint16_t sps) {
    if (sps < 1) sps = 1;
    _ivlUS = 1000000UL / sps;
    if (_ivlUS < _pulseUS * 2)
        _ivlUS = _pulseUS * 2;
}

void Motors::run(int32_t steps) {
    if (steps == 0) return;
    drvEnable(true);
    _dir = (steps > 0) ? 1 : -1;
    _remain = abs(steps);
    bool dirLevel = steps > 0;
    if (_dirInvert) dirLevel = !dirLevel;
    digitalWrite(PIN_DIR, dirLevel ? HIGH : LOW);
    delayMicroseconds(_dirSetupUS);
    _lastUS = micros();
    Serial.printf("[Step] %d Steps %s\n", _remain, _dir > 0 ? "→" : "←");
}

void Motors::stop() {
    _remain = 0;
    _dispState = DS_IDLE;
    drvEnable(false);
}

void Motors::moveBlocking(int32_t steps, const Config &c) {
    if (steps == 0) return;

    configureStepper(c);
    uint32_t periodUS = _ivlUS;
    uint32_t lowUS = periodUS > _pulseUS ? periodUS - _pulseUS : 5;
    uint32_t count = abs(steps);

    drvEnable(true);
    _dir = steps > 0 ? 1 : -1;
    bool dirLevel = steps > 0;
    if (_dirInvert) dirLevel = !dirLevel;
    digitalWrite(PIN_DIR, dirLevel ? HIGH : LOW);
    delayMicroseconds(_dirSetupUS);

    Serial.printf("[Step] blocking %lu Steps %s @ %lu us\n",
                  (unsigned long)count, _dir > 0 ? "→" : "←",
                  (unsigned long)periodUS);

    for (uint32_t i = 0; i < count; i++) {
        digitalWrite(PIN_STEP, HIGH);
        delayMicroseconds(_pulseUS);
        digitalWrite(PIN_STEP, LOW);
        delayMicroseconds(lowUS);
        _pos += _dir;
        if ((i & 0x3F) == 0x3F) yield();
    }

    if (_holdMS > 0) delay(_holdMS);
    drvEnable(false);
    _remain = 0;
    Serial.printf("[Step] Fertig @ pos %d\n", _pos);
}

void Motors::_pulse() {
    digitalWrite(PIN_STEP, HIGH);
    delayMicroseconds(_pulseUS);
    digitalWrite(PIN_STEP, LOW);
    _pos += _dir;
}

void Motors::loop() {
    // ── Stepper-Schritte abarbeiten ──────────────────────────────────────────
    static uint32_t idleT = 0;
    if (_remain > 0) {
        idleT = 0;
        uint8_t pulses = 0;
        while (_remain > 0 && (uint32_t)(micros() - _lastUS) >= _ivlUS) {
            _pulse();
            _remain--;
            _lastUS += _ivlUS;
            if (++pulses >= 16) break;
        }
        if (_remain == 0)
            Serial.printf("[Step] Fertig @ pos %d\n", _pos);
    } else if (_enabled) {
        if (idleT == 0) idleT = millis();
        if (millis() - idleT > _holdMS) { drvEnable(false); idleT = 0; }
    }

    // ── Fütterungs-State-Machine ─────────────────────────────────────────────
    _dispenseLoop();
}

// ═══════════════════════ Selbsttest ═════════════════════════════════════════

void Motors::selfTest(const Config &c) {
    Serial.println(F("[SelfTest] Start"));

    moveBlocking(50, c);
    delay(200);
    moveBlocking(-50, c);
    delay(300);

    s1Open(c);  delay(600);
    s1Close(c); delay(400);
    s2Open(c);  delay(600);
    s2Close(c); delay(400);

    detachServos();
    Serial.println(F("[SelfTest] OK"));
}

// ═══════════════════════ Fütterungs-State-Machine ═══════════════════════════

void Motors::dispense(uint16_t grams, uint8_t servo, const Config &c) {
    if (_dispState != DS_IDLE) return;  // läuft noch
    Serial.printf("[Feed] start %dg servo=%d\n", grams, servo);
    _dispGrams = grams;
    _dispServo = servo;
    _dispCfg   = &c;
    _dispNext(DS_SERVO_OPEN);
}

void Motors::_dispNext(DispState s) {
    _dispState = s;
    _dispNew   = true;
}

void Motors::_svOpen() {
    if (_dispServo == 0 || _dispServo == 1) s1Open(*_dispCfg);
    if (_dispServo == 0 || _dispServo == 2) s2Open(*_dispCfg);
}

void Motors::_svClose() {
    if (_dispServo == 0 || _dispServo == 1) s1Close(*_dispCfg);
    if (_dispServo == 0 || _dispServo == 2) s2Close(*_dispCfg);
}

void Motors::_dispenseLoop() {
    switch (_dispState) {

        case DS_IDLE:
            return;

        case DS_SERVO_OPEN:
            if (_dispNew) { _svOpen(); _dispTimer = millis(); _dispNew = false; }
            if (millis() - _dispTimer >= 600) _dispNext(DS_STEPPER);
            break;

        case DS_STEPPER:
            if (_dispNew) {
                _dispNew = false;
                // moveBlocking() nutzt eine eigene enge Timing-Schleife und ist
                // unabhängig vom äußeren Loop-Takt. run() + loop() würde bei
                // verzögerten loop()-Aufrufen (VL53L0X ~33 ms) Steps akkumulieren
                // und dann als Burst feuern → Klopfen.
                moveBlocking((int32_t)_dispGrams * _dispCfg->stepsPerGram, *_dispCfg);
                _dispNext(DS_STEPPER_WAIT);
            }
            break;

        case DS_STEPPER_WAIT:
            if (_dispNew) { _dispTimer = millis(); _dispNew = false; }
            if (millis() - _dispTimer >= 400) _dispNext(DS_SERVO_CLOSE);
            break;

        case DS_SERVO_CLOSE:
            if (_dispNew) { _svClose(); _dispTimer = millis(); _dispNew = false; }
            if (millis() - _dispTimer >= 1000) _dispNext(DS_SHAKE_OPEN);
            break;

        case DS_SHAKE_OPEN:
            if (_dispNew) { _svOpen(); _dispTimer = millis(); _dispNew = false; }
            if (millis() - _dispTimer >= 500) _dispNext(DS_SHAKE_CLOSE);
            break;

        case DS_SHAKE_CLOSE:
            if (_dispNew) { _svClose(); _dispTimer = millis(); _dispNew = false; }
            if (millis() - _dispTimer >= 400) _dispNext(DS_DONE);
            break;

        case DS_DONE:
            detachServos();
            Serial.println(F("[Feed] Fertig"));
            _dispState = DS_IDLE;
            break;
    }
}

// ═══════════════════════ Servos ═════════════════════════════════════════════

void Motors::_writeServo(Servo &servo, int16_t &current, uint8_t pin, uint8_t deg, const Config &c) {
    deg = constrain(deg, 0, 180);
    servo.attach(pin, SERVO_MIN_US, SERVO_MAX_US);

    if (current < 0) {
        servo.write(deg);
        current = deg;
        return;
    }

    uint16_t speed = c.servoSpeedDPS > 0 ? c.servoSpeedDPS : SERVO_DEFAULT_SPEED_DPS;
    if (speed >= 1000) {
        servo.write(deg);
        current = deg;
        return;
    }

    uint16_t delayMs = 1000UL / speed;
    if (delayMs < 1) delayMs = 1;
    int8_t step = deg >= current ? 1 : -1;
    while (current != deg) {
        current += step;
        servo.write(current);
        delay(delayMs);
        yield();
    }
}

void Motors::s1Open(const Config &c) {
    _writeServo(_sv1, _sv1Pos, PIN_SERVO1, c.s1Open, c);
    Serial.printf("[Sv1] open → %d°\n", c.s1Open);
}
void Motors::s1Close(const Config &c) {
    _writeServo(_sv1, _sv1Pos, PIN_SERVO1, c.s1Close, c);
    Serial.printf("[Sv1] close → %d°\n", c.s1Close);
}
void Motors::s2Open(const Config &c) {
    _writeServo(_sv2, _sv2Pos, PIN_SERVO2, c.s2Open, c);
    Serial.printf("[Sv2] open → %d°\n", c.s2Open);
}
void Motors::s2Close(const Config &c) {
    _writeServo(_sv2, _sv2Pos, PIN_SERVO2, c.s2Close, c);
    Serial.printf("[Sv2] close → %d°\n", c.s2Close);
}

void Motors::setAngle(uint8_t num, uint8_t deg) {
    deg = constrain(deg, 0, 180);
    if (num == 1) {
        _sv1.attach(PIN_SERVO1, SERVO_MIN_US, SERVO_MAX_US);
        _sv1.write(deg);
        _sv1Pos = deg;
    } else {
        _sv2.attach(PIN_SERVO2, SERVO_MIN_US, SERVO_MAX_US);
        _sv2.write(deg);
        _sv2Pos = deg;
    }
}

void Motors::detachServos() {
    _sv1.detach();
    _sv2.detach();
}
