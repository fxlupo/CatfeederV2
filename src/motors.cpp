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

    setSpeed(c.stepperSpeed);

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

void Motors::setSpeed(uint16_t sps) {
    if (sps < 1) sps = 1;
    _ivlUS = 1000000UL / sps;
    if (_ivlUS < STEPPER_PULSE_US * 2)
        _ivlUS = STEPPER_PULSE_US * 2;
}

void Motors::run(int32_t steps) {
    if (steps == 0) return;
    drvEnable(true);
    _dir = (steps > 0) ? 1 : -1;
    _remain = abs(steps);
    digitalWrite(PIN_DIR, (steps > 0) ? HIGH : LOW);
    _lastUS = micros();
    Serial.printf("[Step] %d Steps %s\n", _remain, _dir > 0 ? "→" : "←");
}

void Motors::stop() {
    _remain = 0;
    drvEnable(false);
}

void Motors::_pulse() {
    digitalWrite(PIN_STEP, HIGH);
    delayMicroseconds(STEPPER_PULSE_US);
    digitalWrite(PIN_STEP, LOW);
    _pos += _dir;
}

void Motors::loop() {
    if (_remain <= 0) {
        // Nach Stillstand Treiber nach 1 s abschalten
        if (_enabled) {
            static uint32_t idleT = 0;
            if (idleT == 0) idleT = millis();
            if (millis() - idleT > 1000) { drvEnable(false); idleT = 0; }
        }
        return;
    }
    uint32_t now = micros();
    if (now - _lastUS >= _ivlUS) {
        _pulse();
        _remain--;
        _lastUS = now;
        if (_remain == 0)
            Serial.printf("[Step] Fertig @ pos %d\n", _pos);
    }
}

// ═══════════════════════ Fütterung ══════════════════════════════════════════

void Motors::dispense(uint16_t grams, uint8_t servo, const Config &c) {
    Serial.printf("[Feed] %dg  Servo=%d\n", grams, servo);

    // 1. Klappen öffnen
    if (servo == 0 || servo == 1) s1Open(c);
    if (servo == 0 || servo == 2) s2Open(c);
    delay(600);

    // 2. Stepper fördern
    setSpeed(c.stepperSpeed);
    int32_t steps = (int32_t)grams * c.stepsPerGram;
    run(steps);

    uint32_t t0 = millis();
    while (running() && millis() - t0 < 30000) {
        loop();
        yield();
    }
    if (running()) { Serial.println(F("[Feed] TIMEOUT")); stop(); }

    delay(400);

    // 3. Klappen schließen
    if (servo == 0 || servo == 1) s1Close(c);
    if (servo == 0 || servo == 2) s2Close(c);
    delay(600);

    detachServos();
    Serial.println(F("[Feed] Fertig"));
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
    uint16_t delayMs = 1000UL / speed;
    if (delayMs < 5) delayMs = 5;
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
