// =============================================================================
// CatFeeder ESP32 – Konfigurations-Manager (NVS Flash)
// =============================================================================
#include "config.h"

void CfgManager::begin() {
    _p.begin("cf", false);
}

void CfgManager::defaults(Config &c) {
    memset(&c, 0, sizeof(Config));

    // Zwei Standard-Fütterungen
    c.slots[0] = {true,  7, 30, 25, 0, false};
    c.slots[1] = {true, 18,  0, 25, 0, false};
    for (int i = 2; i < MAX_SLOTS; i++)
        c.slots[i] = {false, 12, 0, 20, 0, false};

    c.stepsPerGram = DEFAULT_STEPS_PER_GRAM;
    c.stepperSpeed = STEPPER_DEFAULT_SPEED;
    c.stepperPulseUS = STEPPER_DEFAULT_PULSE_US;
    c.stepperInvertDir = false;
    c.stepperDirSetupUS = STEPPER_DEFAULT_DIR_SETUP_US;
    c.stepperHoldMS = STEPPER_DEFAULT_HOLD_MS;
    c.s1Open       = SERVO_DEFAULT_OPEN;
    c.s1Close      = SERVO_DEFAULT_CLOSE;
    c.s2Open       = SERVO_DEFAULT_OPEN;
    c.s2Close      = SERVO_DEFAULT_CLOSE;
    c.servoSpeedDPS= SERVO_DEFAULT_SPEED_DPS;
    c.fillEmptyMM  = FILL_EMPTY_MM;
    c.fillFullMM   = FILL_FULL_MM;
    c.irThreshold  = IR_THRESHOLD;
    c.stepperBlockMA = STEPPER_DEFAULT_BLOCK_MA;
    c.defaultGrams   = DEFAULT_FEED_GRAMS;
    c.utcOffset    = 1;          // MEZ
    c.dst          = true;
    strlcpy(c.ssid, WIFI_DEFAULT_SSID, sizeof(c.ssid));
    strlcpy(c.pass, WIFI_DEFAULT_PASS, sizeof(c.pass));
    strlcpy(c.hostname, "catfeeder", sizeof(c.hostname));
}

void CfgManager::save(const Config &c) {
    _p.putBytes("cfg", &c, sizeof(Config));
    _p.putUShort("schema", CONFIG_SCHEMA_VERSION);
    Serial.println(F("[Cfg] Gespeichert"));
}

void CfgManager::load(Config &c) {
    size_t cfgLen = _p.getBytesLength("cfg");
    if (cfgLen == 0) {
        Serial.println(F("[Cfg] Keine Daten → Standardwerte"));
        defaults(c);
        save(c);
    } else {
        defaults(c);
        size_t readLen = cfgLen < sizeof(Config) ? cfgLen : sizeof(Config);
        _p.getBytes("cfg", &c, readLen);
        Serial.println(F("[Cfg] Geladen"));
        bool migrated = false;

        uint16_t schema = _p.getUShort("schema", 0);
        if (schema != CONFIG_SCHEMA_VERSION) migrated = true;

        if (c.stepperSpeed < 100 || c.stepperSpeed > 10000) {
            c.stepperSpeed = STEPPER_DEFAULT_SPEED;
            migrated = true;
        }
        if (c.stepperPulseUS < 2 || c.stepperPulseUS > 50) {
            c.stepperPulseUS = STEPPER_DEFAULT_PULSE_US;
            migrated = true;
        }
        if (c.stepperDirSetupUS > 2000) {
            c.stepperDirSetupUS = STEPPER_DEFAULT_DIR_SETUP_US;
            migrated = true;
        }
        if (c.stepperHoldMS > 5000) {
            c.stepperHoldMS = STEPPER_DEFAULT_HOLD_MS;
            migrated = true;
        }
        if (c.servoSpeedDPS < 20 || c.servoSpeedDPS > 3000) {
            c.servoSpeedDPS = SERVO_DEFAULT_SPEED_DPS;
            migrated = true;
        }
        if (c.stepperBlockMA < 100 || c.stepperBlockMA > 5000) {
            c.stepperBlockMA = STEPPER_DEFAULT_BLOCK_MA;
            migrated = true;
        }
        if (c.defaultGrams < 1 || c.defaultGrams > 500) {
            c.defaultGrams = DEFAULT_FEED_GRAMS;
            migrated = true;
        }

        if (strlen(c.ssid) == 0 && strlen(WIFI_DEFAULT_SSID) > 0) {
            strlcpy(c.ssid, WIFI_DEFAULT_SSID, sizeof(c.ssid));
            strlcpy(c.pass, WIFI_DEFAULT_PASS, sizeof(c.pass));
            migrated = true;
            Serial.println(F("[Cfg] WLAN-Defaults übernommen"));
        }

        if (migrated) save(c);
    }
}

void CfgManager::saveFeedCount(uint32_t n) { _p.putUInt("fc", n); }
uint32_t CfgManager::loadFeedCount()       { return _p.getUInt("fc", 0); }
