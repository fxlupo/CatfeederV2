// =============================================================================
// CatFeeder ESP32 – Konfigurations-Manager (NVS Flash)
//
// Jedes Feld wird unter einem eigenen NVS-Key gespeichert. Damit ist das
// Layout der Config-Struct vollständig irrelevant für die Persistenz:
// neue Felder bekommen beim ersten Lesen automatisch ihren Default,
// alle bestehenden Werte (insb. Servo-Kalibrierung) bleiben bei jedem
// OTA-Update erhalten.
// =============================================================================
#include "config.h"

void CfgManager::begin() {
    _p.begin("cf", false);
}

void CfgManager::defaults(Config &c) {
    memset(&c, 0, sizeof(Config));

    c.slots[0] = {true,  7, 30, 25, 0, false};
    c.slots[1] = {true, 18,  0, 25, 0, false};
    for (int i = 2; i < MAX_SLOTS; i++)
        c.slots[i] = {false, 12, 0, 20, 0, false};

    c.stepsPerGram     = DEFAULT_STEPS_PER_GRAM;
    c.stepperSpeed     = STEPPER_DEFAULT_SPEED;
    c.stepperPulseUS   = STEPPER_DEFAULT_PULSE_US;
    c.stepperInvertDir = false;
    c.stepperDirSetupUS= STEPPER_DEFAULT_DIR_SETUP_US;
    c.stepperHoldMS    = STEPPER_DEFAULT_HOLD_MS;
    c.stepperBlockMA    = STEPPER_DEFAULT_BLOCK_MA;
    c.blockRetries      = BLOCK_DEFAULT_RETRIES;
    c.blockReverseSteps = BLOCK_DEFAULT_REVERSE_STEPS;
    c.blockMinRotPct    = BLOCK_DEFAULT_MIN_ROT_PCT;
    c.s1Open            = SERVO_DEFAULT_OPEN;
    c.s1Close          = SERVO_DEFAULT_CLOSE;
    c.s2Open           = SERVO_DEFAULT_OPEN;
    c.s2Close          = SERVO_DEFAULT_CLOSE;
    c.servoSpeedDPS    = SERVO_DEFAULT_SPEED_DPS;
    c.fillEmptyMM      = FILL_EMPTY_MM;
    c.fillFullMM       = FILL_FULL_MM;
    c.irThreshold      = IR_THRESHOLD;
    for (int i = 0; i < WA_USERS; i++) {
        c.waUsers[i].active    = false;
        c.waUsers[i].phone[0]  = '\0';
        c.waUsers[i].apikey[0] = '\0';
    }
    c.defaultGrams     = DEFAULT_FEED_GRAMS;
    c.utcOffset        = 1;
    c.dst              = true;
    strlcpy(c.ssid,     WIFI_DEFAULT_SSID, sizeof(c.ssid));
    strlcpy(c.pass,     WIFI_DEFAULT_PASS, sizeof(c.pass));
    strlcpy(c.hostname, "catfeeder",       sizeof(c.hostname));
}

void CfgManager::save(const Config &c) {
    // Slots
    for (int i = 0; i < MAX_SLOTS; i++) {
        char k[8];
        sprintf(k, "s%da",  i); _p.putBool(k,   c.slots[i].active);
        sprintf(k, "s%dh",  i); _p.putUChar(k,  c.slots[i].hour);
        sprintf(k, "s%dm",  i); _p.putUChar(k,  c.slots[i].minute);
        sprintf(k, "s%dg",  i); _p.putUShort(k, c.slots[i].grams);
        sprintf(k, "s%dsv", i); _p.putUChar(k,  c.slots[i].servo);
        sprintf(k, "s%ddt", i); _p.putBool(k,   c.slots[i].doneToday);
    }
    // WLAN
    _p.putString("ssid", c.ssid);
    _p.putString("pass", c.pass);
    // Stepper
    _p.putUShort("spg", c.stepsPerGram);
    _p.putUShort("spd", c.stepperSpeed);
    _p.putUShort("spu", c.stepperPulseUS);
    _p.putBool(  "sdi", c.stepperInvertDir);
    _p.putUShort("sds", c.stepperDirSetupUS);
    _p.putUShort("shm", c.stepperHoldMS);
    _p.putUShort("sbm", c.stepperBlockMA);
    _p.putUChar( "bkr", c.blockRetries);
    _p.putUShort("bks", c.blockReverseSteps);
    _p.putUChar( "bkp", c.blockMinRotPct);
    // Servos – explizit benannte Keys, layout-unabhängig
    _p.putUChar( "s1o", c.s1Open);
    _p.putUChar( "s1c", c.s1Close);
    _p.putUChar( "s2o", c.s2Open);
    _p.putUChar( "s2c", c.s2Close);
    _p.putUShort("svs", c.servoSpeedDPS);
    // Sensoren
    _p.putUShort("feM", c.fillEmptyMM);
    _p.putUShort("ffM", c.fillFullMM);
    _p.putUShort("irt", c.irThreshold);
    // Sonstiges
    _p.putChar(  "tz",  c.utcOffset);
    _p.putBool(  "dst", c.dst);
    _p.putString("hn",  c.hostname);
    _p.putUShort("dfg", c.defaultGrams);
    for (int i = 0; i < WA_USERS; i++) {
        char k[6];
        sprintf(k, "w%da", i); _p.putBool(k,   c.waUsers[i].active);
        sprintf(k, "w%dp", i); _p.putString(k,  c.waUsers[i].phone);
        sprintf(k, "w%dk", i); _p.putString(k,  c.waUsers[i].apikey);
    }
    Serial.println(F("[Cfg] Gespeichert"));
}

void CfgManager::load(Config &c) {
    defaults(c);

    if (!_p.isKey("spg")) {
        // Kein Individual-Key-Format vorhanden (Erststart oder alter Blob)
        _p.remove("cfg");    // altes Blob-Format bereinigen
        _p.remove("schema");
        Serial.println(F("[Cfg] Keine Daten → Standardwerte"));
        save(c);
        return;
    }

    // Slots
    for (int i = 0; i < MAX_SLOTS; i++) {
        char k[8];
        sprintf(k, "s%da",  i); c.slots[i].active   = _p.getBool(k,   c.slots[i].active);
        sprintf(k, "s%dh",  i); c.slots[i].hour      = _p.getUChar(k,  c.slots[i].hour);
        sprintf(k, "s%dm",  i); c.slots[i].minute    = _p.getUChar(k,  c.slots[i].minute);
        sprintf(k, "s%dg",  i); c.slots[i].grams     = _p.getUShort(k, c.slots[i].grams);
        sprintf(k, "s%dsv", i); c.slots[i].servo     = _p.getUChar(k,  c.slots[i].servo);
        sprintf(k, "s%ddt", i); c.slots[i].doneToday = _p.getBool(k,   false);
    }
    // WLAN
    { String s = _p.getString("ssid", c.ssid); strlcpy(c.ssid, s.c_str(), sizeof(c.ssid)); }
    { String s = _p.getString("pass", c.pass); strlcpy(c.pass, s.c_str(), sizeof(c.pass)); }
    // Stepper
    c.stepsPerGram      = _p.getUShort("spg", c.stepsPerGram);
    c.stepperSpeed      = _p.getUShort("spd", c.stepperSpeed);
    c.stepperPulseUS    = _p.getUShort("spu", c.stepperPulseUS);
    c.stepperInvertDir  = _p.getBool(  "sdi", c.stepperInvertDir);
    c.stepperDirSetupUS = _p.getUShort("sds", c.stepperDirSetupUS);
    c.stepperHoldMS     = _p.getUShort("shm", c.stepperHoldMS);
    c.stepperBlockMA    = _p.getUShort("sbm", c.stepperBlockMA);
    c.blockRetries      = _p.getUChar( "bkr", c.blockRetries);
    c.blockReverseSteps = _p.getUShort("bks", c.blockReverseSteps);
    c.blockMinRotPct    = _p.getUChar( "bkp", c.blockMinRotPct);
    // Servos
    c.s1Open        = _p.getUChar( "s1o", c.s1Open);
    c.s1Close       = _p.getUChar( "s1c", c.s1Close);
    c.s2Open        = _p.getUChar( "s2o", c.s2Open);
    c.s2Close       = _p.getUChar( "s2c", c.s2Close);
    c.servoSpeedDPS = _p.getUShort("svs", c.servoSpeedDPS);
    // Sensoren
    c.fillEmptyMM   = _p.getUShort("feM", c.fillEmptyMM);
    c.fillFullMM    = _p.getUShort("ffM", c.fillFullMM);
    c.irThreshold   = _p.getUShort("irt", c.irThreshold);
    // Sonstiges
    c.utcOffset     = _p.getChar(  "tz",  c.utcOffset);
    c.dst           = _p.getBool(  "dst", c.dst);
    { String s = _p.getString("hn", c.hostname); strlcpy(c.hostname, s.c_str(), sizeof(c.hostname)); }
    c.defaultGrams  = _p.getUShort("dfg", c.defaultGrams);
    for (int i = 0; i < WA_USERS; i++) {
        char k[6];
        sprintf(k, "w%da", i); c.waUsers[i].active = _p.getBool(k, false);
        sprintf(k, "w%dp", i); { String s = _p.getString(k, ""); strlcpy(c.waUsers[i].phone,  s.c_str(), sizeof(c.waUsers[i].phone)); }
        sprintf(k, "w%dk", i); { String s = _p.getString(k, ""); strlcpy(c.waUsers[i].apikey, s.c_str(), sizeof(c.waUsers[i].apikey)); }
    }

    // WLAN-Credentials aus lokalem Header falls leer
    if (strlen(c.ssid) == 0 && strlen(WIFI_DEFAULT_SSID) > 0) {
        strlcpy(c.ssid, WIFI_DEFAULT_SSID, sizeof(c.ssid));
        strlcpy(c.pass, WIFI_DEFAULT_PASS, sizeof(c.pass));
        save(c);
        Serial.println(F("[Cfg] WLAN-Defaults übernommen"));
        return;
    }

    Serial.println(F("[Cfg] Geladen"));
}

void CfgManager::saveFeedCount(uint32_t n) { _p.putUInt("fc", n); }
uint32_t CfgManager::loadFeedCount()       { return _p.getUInt("fc", 0); }
