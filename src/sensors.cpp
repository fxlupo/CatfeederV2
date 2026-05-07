// =============================================================================
// CatFeeder ESP32 – Sensor-Manager Implementation
// =============================================================================
#include "sensors.h"

void Sensors::begin() {
    // ── I2C ─────────────────────────────────────────────────────────────────
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(100000);

    // ── VL53L0X: XSHUT steuern ─────────────────────────────────────────────
    pinMode(PIN_VL53_XSHUT, OUTPUT);
    digitalWrite(PIN_VL53_XSHUT, HIGH);   // Sensor aktivieren
    delay(50);

    // ── INA219 ──────────────────────────────────────────────────────────────
    ok_ina = _ina.begin();
    if (ok_ina) {
        _ina.setCalibration_32V_1A();
        Serial.println(F("[Sensor] INA219  ✓"));
    } else {
        Serial.println(F("[Sensor] INA219  ✗"));
    }

    // ── VL53L0X ─────────────────────────────────────────────────────────────
    ok_vl = _vl.begin();
    if (ok_vl) {
        // Long-Range Modus für bessere Reichweite im Futterbehälter
        _vl.configSensor(Adafruit_VL53L0X::VL53L0X_SENSE_LONG_RANGE);
        Serial.println(F("[Sensor] VL53L0X ✓"));
    } else {
        Serial.println(F("[Sensor] VL53L0X ✗"));
    }

    // ── AS5600 ──────────────────────────────────────────────────────────────
    ok_as = _as.begin();
    Serial.printf("[Sensor] AS5600  %s\n", ok_as ? "✓" : "✗");

    // ── DS3231 ──────────────────────────────────────────────────────────────
    ok_rtc = _rtc.begin();
    if (ok_rtc) {
        if (_rtc.lostPower()) {
            Serial.println(F("[Sensor] DS3231 lost power, using compile time"));
            _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        Serial.println(F("[Sensor] DS3231  ✓"));
    } else {
        Serial.println(F("[Sensor] DS3231  ✗"));
    }

    // ── Endschalter ─────────────────────────────────────────────────────────
    pinMode(PIN_S1_OPEN,  INPUT_PULLUP);
    pinMode(PIN_S1_CLOSE, INPUT_PULLUP);
    pinMode(PIN_S2_OPEN,  INPUT_PULLUP);
    pinMode(PIN_S2_CLOSE, INPUT_PULLUP);

    // ── IR Sensoren ─────────────────────────────────────────────────────────
    // Analog: GPIO34, GPIO35 (Input-Only, kein Pullup)
    analogSetAttenuation(ADC_11db);
    // Digital: GPIO2, GPIO4
    pinMode(PIN_IR1_D0, INPUT);
    pinMode(PIN_IR2_D0, INPUT);

    Serial.println(F("[Sensor] Init fertig"));
}

// ─────────────────────────────────────────────────────────────────────────────
void Sensors::update() {
    uint32_t now = millis();
    if (now - _lastRead < READ_INTERVAL) return;
    _lastRead = now;

    // INA219
    if (ok_ina) {
        _busV  = _ina.getBusVoltage_V();
        _curMA = _ina.getCurrent_mA();
        _powMW = _ina.getPower_mW();
    }

    // VL53L0X
    if (ok_vl) {
        VL53L0X_RangingMeasurementData_t m;
        _vl.rangingTest(&m, false);
        if (m.RangeStatus != 4)
            _distMM = m.RangeMilliMeter;
    }

    // AS5600
    if (ok_as)
        _angle = _as.getRawAngle() * (360.0f / 4096.0f);

    // IR Analog + Digital
    _ir1a = analogRead(PIN_IR1_A0);
    _ir2a = analogRead(PIN_IR2_A0);
    _ir1d = digitalRead(PIN_IR1_D0);
    _ir2d = digitalRead(PIN_IR2_D0);
}

// ─────────────────────────────────────────────────────────────────────────────
void Sensors::fillStatus(Status &st, const Config &c) {
    st.busV      = _busV;
    st.currentMA = _curMA;
    st.powerMW   = _powMW;
    st.distMM    = _distMM;
    st.angleDeg  = _angle;
    st.ir1Analog = _ir1a;
    st.ir2Analog = _ir2a;
    st.ir1Digital= _ir1d;
    st.ir2Digital= _ir2d;

    // Füllstand berechnen
    if (ok_vl && _distMM > 0) {
        if      (_distMM >= c.fillEmptyMM) st.fillPct = 0;
        else if (_distMM <= c.fillFullMM)  st.fillPct = 100;
        else {
            float range = (float)(c.fillEmptyMM - c.fillFullMM);
            float level = (float)(c.fillEmptyMM - _distMM);
            st.fillPct = constrain((uint8_t)(level / range * 100.0f), 0, 100);
        }
    } else {
        st.fillPct = 0;
    }

    // Endschalter
    st.s1Open  = s1Open();
    st.s1Close = s1Close();
    st.s2Open  = s2Open();
    st.s2Close = s2Close();

    // Health
    st.ok_ina = ok_ina;
    st.ok_vl  = ok_vl;
    st.ok_as  = ok_as;
    st.ok_rtc = ok_rtc;

    // Warnungen
    st.overcurrent = (_curMA > INA_OVERCURRENT_MA);
    st.fillLow     = (st.fillPct < 10 && st.fillPct > 0);

    // System
    st.uptimeS = millis() / 1000;
    st.heap    = ESP.getFreeHeap();

    String t = timeString();
    strncpy(st.timeStr, t.c_str(), sizeof(st.timeStr) - 1);
}

// ═════════════════════════════════════════════════════════════════════════════
// RTC (DS3231)
// ═════════════════════════════════════════════════════════════════════════════

uint8_t Sensors::hour() {
    if (!ok_rtc) return 0;
    return _rtc.now().hour();
}

uint8_t Sensors::minute() {
    if (!ok_rtc) return 0;
    return _rtc.now().minute();
}

String Sensors::timeString() {
    if (!ok_rtc) return String("--:--:--");
    DateTime now = _rtc.now();
    char buf[20];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d %02d.%02d.%04d",
             now.hour(), now.minute(), now.second(),
             now.day(), now.month(), now.year());
    return String(buf);
}

bool Sensors::setTime(uint16_t y, uint8_t mo, uint8_t d,
                      uint8_t h, uint8_t mi, uint8_t s) {
    if (!ok_rtc) return false;
    _rtc.adjust(DateTime(y, mo, d, h, mi, s));
    Serial.printf("[RTC] → %02d:%02d:%02d %02d.%02d.%04d\n", h, mi, s, d, mo, y);
    return true;
}
