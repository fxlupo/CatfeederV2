// =============================================================================
// CatFeeder ESP32 - Main Firmware
// Web UI, OTA, NTP, scheduler, monitoring and feeding orchestration.
// =============================================================================
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <time.h>
#include <Callmebot_ESP32.h>
#include "config.h"
#include "sensors.h"
#include "motors.h"
#include "web.h"

CfgManager cfgMgr;
Config     cfg;
Status     statusData;
Sensors    sensors;
Motors     motors;
Web        web;
FeedLog    feedLog;

static uint8_t lastMinute      = 255;
static bool    dayResetDone    = false;
static bool    otaReady        = false;
static bool    otaActive       = false;
static bool    wasDispensing   = false;
static bool    pendingEventValid = false;
static FeedEvent pendingEvent;
static const uint16_t OTA_PORT = 3232;

static void setOtaPhase(const char *phase) {
    strlcpy(statusData.otaPhase, phase, sizeof(statusData.otaPhase));
}
static void setOtaError(const char *error) {
    strlcpy(statusData.lastOtaError, error, sizeof(statusData.lastOtaError));
}
static void notifyEvent(const __FlashStringHelper *event) {
    Serial.print(F("[Notify] ")); Serial.println(event);
}
static void notifyEvent(const char *event) {
    Serial.print(F("[Notify] ")); Serial.println(event);
}

// ─── WhatsApp Benachrichtigung ───────────────────────────────────────────────

static void sendWhatsAppAlert(const char *msg) {
    if (web.apMode()) return;
    for (int i = 0; i < WA_USERS; i++) {
        const Config::WaUser &u = cfg.waUsers[i];
        if (!u.active || strlen(u.phone) == 0 || strlen(u.apikey) == 0) continue;
        Serial.printf("[WA] Sende an %s...\n", u.phone);
        Callmebot.whatsappMessage(u.phone, u.apikey, msg);
        Serial.println(F("[WA] Gesendet"));
    }
}

// ─── NTP ────────────────────────────────────────────────────────────────────

static void syncNTP() {
    if (web.apMode()) return;
    long offset = ((long)cfg.utcOffset + (cfg.dst ? 1 : 0)) * 3600L;
    configTime(offset, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print(F("[NTP] Sync"));
    struct tm ti;
    uint32_t t0 = millis();
    while (!getLocalTime(&ti, 200) && millis() - t0 < 8000) {
        Serial.print('.');
    }
    if (getLocalTime(&ti, 200)) {
        sensors.setTime(ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
                        ti.tm_hour, ti.tm_min, ti.tm_sec);
        Serial.printf(" %02d:%02d:%02d OK\n", ti.tm_hour, ti.tm_min, ti.tm_sec);
    } else {
        Serial.println(F(" Timeout"));
    }
}

// ─── OTA ────────────────────────────────────────────────────────────────────

static void beginOTA() {
    ArduinoOTA.setHostname(cfg.hostname);
    ArduinoOTA.setPort(OTA_PORT);

    ArduinoOTA.onStart([]() {
        otaActive = true;
        setOtaPhase("start");
        notifyEvent(F("OTA start"));
        motors.stop();
        motors.detachServos();
        web.closeSSE();          // SSE-Clients trennen → weniger WiFi-Contention
    });
    ArduinoOTA.onEnd([]() {
        otaActive = false;
        setOtaPhase("complete");
        notifyEvent(F("OTA complete"));
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("[OTA] %u%%\n", total > 0 ? progress * 100U / total : 0);
    });
    ArduinoOTA.onError([](ota_error_t error) {
        otaActive = false;       // WICHTIG: Loop wieder freigeben nach Fehler
        setOtaPhase("error");
        switch (error) {
            case OTA_AUTH_ERROR:    setOtaError("auth");    break;
            case OTA_BEGIN_ERROR:   setOtaError("begin");   break;
            case OTA_CONNECT_ERROR: setOtaError("connect"); break;
            case OTA_RECEIVE_ERROR: setOtaError("receive"); break;
            case OTA_END_ERROR:     setOtaError("end");     break;
            default:                setOtaError("unknown"); break;
        }
        Serial.println(F("[OTA] Fehler – Betrieb wird fortgesetzt"));
    });

    setOtaPhase("ready");
    setOtaError("");
    ArduinoOTA.begin();
    otaReady = true;
    Serial.printf("[OTA] Ready as %s.local:%u\n", cfg.hostname, OTA_PORT);
}

// ─── Status ─────────────────────────────────────────────────────────────────

static void refreshStatus() {
    sensors.update();
    sensors.fillStatus(statusData, cfg);
    statusData.feeds    = cfgMgr.loadFeedCount();
    statusData.wifiOK   = !web.apMode();
    statusData.apMode   = web.apMode();
    statusData.otaReady = otaReady;
    statusData.otaPort  = OTA_PORT;
    statusData.wifiRSSI = WiFi.isConnected() ? WiFi.RSSI() : 0;
    strlcpy(statusData.ip,       web.ip().c_str(),             sizeof(statusData.ip));
    strlcpy(statusData.hostname, cfg.hostname,                 sizeof(statusData.hostname));
    strlcpy(statusData.wifiMode, web.apMode() ? "AP" : "STA", sizeof(statusData.wifiMode));

    // Nur auf steigende Flanke feuern – kein Spam bei dauerhaftem Zustand
    static bool lastOvercurrent = false;
    static bool lastFillLow     = false;
    if (statusData.overcurrent && !lastOvercurrent) notifyEvent(F("Overcurrent detected"));
    if (statusData.fillLow     && !lastFillLow)     notifyEvent(F("Fill level low"));
    lastOvercurrent = statusData.overcurrent;
    lastFillLow     = statusData.fillLow;
}

// ─── Fütterung ──────────────────────────────────────────────────────────────

static void startFeed(uint16_t grams, uint8_t servo, const char *reason) {
    if (motors.dispensing()) {
        Serial.println(F("[Feed] ignored: already running"));
        return;
    }
    Serial.printf("[Feed] reason=%s grams=%u servo=%u\n", reason, grams, servo);

    // Sensordaten vor Fütterung für den Event-Log sichern
    pendingEvent = {};
    strlcpy(pendingEvent.timeStr, statusData.timeStr, sizeof(pendingEvent.timeStr));
    pendingEvent.isAuto   = (strcmp(reason, "schedule") == 0);
    pendingEvent.grams    = grams;
    pendingEvent.servo    = servo;
    pendingEvent.distBefore = statusData.distMM;
    pendingEvent.fillBefore = statusData.fillPct;
    pendingEventValid = true;

    motors.dispense(grams, servo, cfg);
}

static void resetDailyScheduleIfNeeded(uint8_t hour, uint8_t minute) {
    if (hour == 0 && minute == 0) {
        if (!dayResetDone) {
            for (int i = 0; i < MAX_SLOTS; i++) cfg.slots[i].doneToday = false;
            cfgMgr.save(cfg);
            dayResetDone = true;
            Serial.println(F("[Scheduler] daily flags reset"));
        }
    } else {
        dayResetDone = false;
    }
}

static void schedulerLoop() {
    if (!sensors.ok_rtc || motors.dispensing()) return;

    uint8_t hour   = sensors.hour();
    uint8_t minute = sensors.minute();
    resetDailyScheduleIfNeeded(hour, minute);

    if (minute == lastMinute) return;
    lastMinute = minute;

    for (int i = 0; i < MAX_SLOTS; i++) {
        FeedSlot &slot = cfg.slots[i];
        if (!slot.active || slot.doneToday) continue;
        if (slot.hour != hour || slot.minute != minute) continue;

        slot.doneToday = true;
        cfgMgr.save(cfg);
        startFeed(slot.grams, slot.servo, "schedule");
        break;
    }
}

static void handleWebFeedRequest() {
    if (!web.feedRequested) return;
    uint16_t grams = web.feedGrams;
    uint8_t  servo = web.feedServo;
    web.feedRequested = false;
    startFeed(grams, servo, "web");
}

// Flanke dispensing true→false: Feed-Counter buchen + Log-Eintrag abschließen
static void checkFeedComplete() {
    bool nowDispensing = motors.dispensing();
    if (wasDispensing && !nowDispensing) {
        if (!motors.feedAborted()) {
            statusData.feeds++;
            cfgMgr.saveFeedCount(statusData.feeds);
            notifyEvent("Feeding completed");
        } else {
            notifyEvent("Feeding aborted - blockage!");
            char waMsg[200];
            snprintf(waMsg, sizeof(waMsg),
                "CatFeeder Blockade! Fuetterung (%dg) nach %d Versuch(en) "
                "abgebrochen. Bitte Futterzufuhr pruefen. [%s]",
                pendingEvent.grams, motors.blockRetries(),
                statusData.timeStr);
            sendWhatsAppAlert(waMsg);
        }

        if (pendingEventValid) {
            pendingEvent.distAfter      = statusData.distMM;
            pendingEvent.fillAfter      = statusData.fillPct;
            pendingEvent.ir1Pulses      = motors.irCount1();
            pendingEvent.ir2Pulses      = motors.irCount2();
            pendingEvent.feedAborted    = motors.feedAborted();
            pendingEvent.blockRetryCount= motors.blockRetries();
            feedLog.add(pendingEvent);
            pendingEventValid = false;
        }
    }
    wasDispensing = nowDispensing;
}

// ─── Setup / Loop ───────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.printf("CatFeeder firmware %s\n", FW_VERSION);
    setOtaPhase("boot");
    setOtaError("");

    cfgMgr.begin();
    cfgMgr.load(cfg);
    statusData.feeds = cfgMgr.loadFeedCount();

    sensors.begin();
    motors.begin(cfg, sensors);
    motors.selfTest(cfg);
    refreshStatus();

    web.begin(cfg, statusData, sensors, motors, cfgMgr, feedLog);
    syncNTP();
    beginOTA();

    notifyEvent(F("Boot complete"));
}

void loop() {
    ArduinoOTA.handle();
    if (otaActive) return;
    motors.loop();
    refreshStatus();
    web.loop();
    handleWebFeedRequest();
    checkFeedComplete();
    schedulerLoop();
    yield();
}
