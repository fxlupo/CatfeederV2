// =============================================================================
// Kalibrierung: Steps für 360° am Getriebeausgang ermitteln
//
// Adafruit AS5600 Library → getRawAngle() gibt 0-4095 zurück
// AS5600 sitzt am STEPPER (vor dem 5:1 Getriebe)
// Ziel: Rotor (Getriebeausgang) dreht exakt 360°
//       → Stepper muss 5 × 360° = 1800° drehen
//       → AS5600 zählt 5 volle Umdrehungen
//
// TMC2208: DIR invertiert (LOW=CW, HIGH=CCW)
// =============================================================================
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_AS5600.h>

// ─── Pins ───────────────────────────────────────────────────────────────────
#define PIN_STEP      25
#define PIN_DIR       26
#define PIN_EN_DRV    27
#define PIN_SDA       21
#define PIN_SCL       22

// ─── Parameter ──────────────────────────────────────────────────────────────
#define GEAR_RATIO    5.0f
#define STEP_DELAY    200     // µs
#define PULSE_US      5
#define MAX_STEPS     20000   // Sicherheitslimit
#define RAW_TO_DEG    (360.0f / 4096.0f)   // Adafruit: getRawAngle() = 0-4095

Adafruit_AS5600 as5600;

// ─── Kumulativer Winkel-Tracker ─────────────────────────────────────────────
float lastRaw    = 0;
float totalAngle = 0;

float readAbsDeg() {
    return as5600.getRawAngle() * RAW_TO_DEG;
}

void resetTracking() {
    lastRaw    = readAbsDeg();
    totalAngle = 0;
}

float readCumulative() {
    float raw = readAbsDeg();
    float diff = raw - lastRaw;
    if (diff >  180.0f) diff -= 360.0f;
    if (diff < -180.0f) diff += 360.0f;
    totalAngle += diff;
    lastRaw = raw;
    return totalAngle;
}

void singleStep() {
    digitalWrite(PIN_STEP, HIGH);
    delayMicroseconds(PULSE_US);
    digitalWrite(PIN_STEP, LOW);
    delayMicroseconds(STEP_DELAY - PULSE_US);
}

// ═════════════════════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println(F("\n══════════════════════════════════════════════"));
    Serial.println(F("  Step-Kalibrierung: 360° am Getriebeausgang"));
    Serial.println(F("══════════════════════════════════════════════"));
    Serial.printf("Getriebe: %.0f:1\n", GEAR_RATIO);
    Serial.printf("Stepper muss %.0f° drehen für 360° Rotor\n", 360.0f * GEAR_RATIO);
    Serial.println(F("AS5600 am Stepper (Antrieb)"));
    Serial.println(F("Adafruit AS5600 Library\n"));

    Wire.begin(PIN_SDA, PIN_SCL);

    if (as5600.begin()) {
        if (as5600.isMagnetDetected()) {
            Serial.printf("[AS5600] ✓  Magnet erkannt\n");
            Serial.printf("[AS5600] Startposition: %.2f° (raw: %d)\n\n",
                          readAbsDeg(), as5600.getRawAngle());
        } else {
            Serial.println(F("[AS5600] ⚠ Kein Magnet erkannt! Weiter trotzdem...\n"));
        }
    } else {
        Serial.println(F("[AS5600] ✗ NICHT gefunden! Abbruch.\n"));
        while (1) delay(1000);
    }

    pinMode(PIN_STEP,   OUTPUT);
    pinMode(PIN_DIR,    OUTPUT);
    pinMode(PIN_EN_DRV, OUTPUT);
    digitalWrite(PIN_STEP, LOW);
    digitalWrite(PIN_DIR,  LOW);   // CW (TMC2208: LOW=CW)
    digitalWrite(PIN_EN_DRV, HIGH);

    Serial.println(F("Sende 'go' im Serial Monitor um die Messung zu starten."));
    Serial.println(F("Der Stepper fährt CW bis der Rotor 360° erreicht hat.\n"));
}

// ═════════════════════════════════════════════════════════════════════════════
void loop() {
    if (!Serial.available()) return;

    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input != "go") {
        Serial.println(F("Bitte 'go' eingeben."));
        return;
    }

    Serial.println(F("\n━━━━━━━━ MESSUNG START ━━━━━━━━"));

    float startAbs = readAbsDeg();
    Serial.printf("Startwinkel (AS5600): %.2f°\n\n", startAbs);

    resetTracking();

    // Treiber EIN, Richtung CW
    digitalWrite(PIN_EN_DRV, LOW);
    digitalWrite(PIN_DIR, LOW);  // TMC2208: LOW = CW
    delay(10);

    float targetEnc = 360.0f * GEAR_RATIO;  // 1800°

    Serial.printf("Ziel: %.0f° Encoder (= 360° Rotor)\n\n", targetEnc);
    Serial.println(F("  Steps   Enc(°)    Rotor(°)  Enc-Umdrehungen"));
    Serial.println(F("  ─────   ──────    ────────  ───────────────"));

    int steps = 0;
    float encAngle = 0;

    while (steps < MAX_STEPS) {
        singleStep();
        steps++;

        encAngle = readCumulative();

        if (steps % 100 == 0) {
            float rotorAngle = encAngle / GEAR_RATIO;
            float encRevs = encAngle / 360.0f;
            Serial.printf("  %5d   %7.1f   %7.1f°   %.2f\n",
                          steps, encAngle, rotorAngle, encRevs);
        }

        if (encAngle >= targetEnc) break;
    }

    // Treiber AUS
    digitalWrite(PIN_EN_DRV, HIGH);

    // ─── Ergebnis ───────────────────────────────────────────────────────
    float endAbs = readAbsDeg();
    float rotorFinal = encAngle / GEAR_RATIO;
    float encRevsFinal = encAngle / 360.0f;

    Serial.println(F("\n━━━━━━━━ ERGEBNIS ━━━━━━━━\n"));
    Serial.println(F("┌───────────────────────────────────────────────┐"));
    Serial.printf("│  Gemessene Steps gesamt:       %5d           │\n", steps);
    Serial.println(F("├───────────────────────────────────────────────┤"));
    Serial.printf("│  Encoder (AS5600) total:      %7.1f°         │\n", encAngle);
    Serial.printf("│  Encoder Umdrehungen:           %5.2f          │\n", encRevsFinal);
    Serial.printf("│  Encoder Start → Ende:  %6.2f° → %6.2f°     │\n", startAbs, endAbs);
    Serial.println(F("├───────────────────────────────────────────────┤"));
    Serial.printf("│  Rotor (Getriebe) total:      %7.1f°         │\n", rotorFinal);
    Serial.printf("│  Rotor Umdrehungen:             %5.2f          │\n", rotorFinal / 360.0f);
    Serial.println(F("├───────────────────────────────────────────────┤"));
    Serial.printf("│  Steps / 360° Motor:           %5.1f           │\n", steps / encRevsFinal);
    Serial.printf("│  Steps / 360° Rotor:           %5d           │\n", steps);
    Serial.printf("│  Steps / Grad Rotor:            %6.2f          │\n", (float)steps / rotorFinal);
    Serial.println(F("└───────────────────────────────────────────────┘"));
    Serial.println();

    if (steps >= MAX_STEPS)
        Serial.println(F("⚠ MAX_STEPS erreicht! Getriebe-Ratio prüfen."));

    Serial.printf("Theoretisch (200 Fullsteps × %.0f):  %d Steps\n",
                  GEAR_RATIO, (int)(200 * GEAR_RATIO));
    Serial.printf("Gemessen:                           %d Steps\n", steps);
    Serial.printf("Abweichung:                         %+d Steps (%.1f%%)\n",
                  steps - (int)(200 * GEAR_RATIO),
                  ((float)steps / (200.0f * GEAR_RATIO) - 1.0f) * 100.0f);

    Serial.println(F("\n'go' eingeben für nächste Messung."));
}