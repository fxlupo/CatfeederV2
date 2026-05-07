// =============================================================================
// CatFeeder ESP32 - Main Firmware
// Web UI, OTA, scheduler, monitoring and feeding orchestration.
// =============================================================================
#include <Arduino.h>
#include <ArduinoOTA.h>
#include "config.h"
#include "sensors.h"
#include "motors.h"
#include "web.h"

CfgManager cfgMgr;
Config cfg;
Status statusData;
Sensors sensors;
Motors motors;
Web web;

static uint8_t lastMinute = 255;
static bool dayResetDone = false;
static bool feedInProgress = false;
static bool otaReady = false;
static const uint16_t OTA_PORT = 3232;

static void setOtaPhase(const char *phase) {
    strlcpy(statusData.otaPhase, phase, sizeof(statusData.otaPhase));
}

static void setOtaError(const char *error) {
    strlcpy(statusData.lastOtaError, error, sizeof(statusData.lastOtaError));
}

static void notifyEvent(const __FlashStringHelper *event) {
    Serial.print(F("[Notify] "));
    Serial.println(event);
}

static void notifyEvent(const char *event) {
    Serial.print(F("[Notify] "));
    Serial.println(event);
}

static void beginOTA() {
    ArduinoOTA.setHostname(cfg.hostname);
    ArduinoOTA.setPort(OTA_PORT);

    ArduinoOTA.onStart([]() {
        setOtaPhase("start");
        notifyEvent(F("OTA start"));
        motors.stop();
        motors.detachServos();
    });
    ArduinoOTA.onEnd([]() {
        setOtaPhase("complete");
        notifyEvent(F("OTA complete"));
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        uint8_t pct = total > 0 ? (progress * 100U / total) : 0;
        Serial.printf("[OTA] progress=%u%%\n", pct);
    });
    ArduinoOTA.onError([](ota_error_t error) {
        setOtaPhase("error");
        switch (error) {
            case OTA_AUTH_ERROR: setOtaError("auth"); break;
            case OTA_BEGIN_ERROR: setOtaError("begin"); break;
            case OTA_CONNECT_ERROR: setOtaError("connect"); break;
            case OTA_RECEIVE_ERROR: setOtaError("receive"); break;
            case OTA_END_ERROR: setOtaError("end"); break;
            default: setOtaError("unknown"); break;
        }
        Serial.printf("[OTA] error=%u\n", error);
    });

    setOtaPhase("ready");
    setOtaError("");
    ArduinoOTA.begin();
    otaReady = true;
    Serial.printf("[OTA] Ready as %s.local:%u\n", cfg.hostname, OTA_PORT);
}

static void refreshStatus() {
    sensors.update();
    sensors.fillStatus(statusData, cfg);
    statusData.feeds = cfgMgr.loadFeedCount();
    statusData.wifiOK = !web.apMode();
    statusData.apMode = web.apMode();
    statusData.otaReady = otaReady;
    statusData.otaPort = OTA_PORT;
    statusData.wifiRSSI = WiFi.isConnected() ? WiFi.RSSI() : 0;
    strlcpy(statusData.ip, web.ip().c_str(), sizeof(statusData.ip));
    strlcpy(statusData.hostname, cfg.hostname, sizeof(statusData.hostname));
    strlcpy(statusData.wifiMode, web.apMode() ? "AP" : "STA", sizeof(statusData.wifiMode));

    if (statusData.overcurrent) notifyEvent(F("Overcurrent detected"));
    if (statusData.fillLow) notifyEvent(F("Fill level low"));
}

static void runFeed(uint16_t grams, uint8_t servo, const char *reason) {
    if (feedInProgress) {
        Serial.println(F("[Feed] ignored: already running"));
        return;
    }

    feedInProgress = true;
    Serial.printf("[Feed] reason=%s grams=%u servo=%u\n", reason, grams, servo);
    motors.dispense(grams, servo, cfg);
    statusData.feeds++;
    cfgMgr.saveFeedCount(statusData.feeds);
    notifyEvent("Feeding completed");
    feedInProgress = false;
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
    if (!sensors.ok_rtc || feedInProgress) return;

    uint8_t hour = sensors.hour();
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
        runFeed(slot.grams, slot.servo, "schedule");
        break;
    }
}

static void handleWebFeedRequest() {
    if (!web.feedRequested) return;
    uint16_t grams = web.feedGrams;
    uint8_t servo = web.feedServo;
    web.feedRequested = false;
    runFeed(grams, servo, "web");
}

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
    motors.begin(cfg);
    refreshStatus();

    web.begin(cfg, statusData, sensors, motors, cfgMgr);
    beginOTA();

    notifyEvent(F("Boot complete"));
}

void loop() {
    ArduinoOTA.handle();
    motors.loop();
    refreshStatus();
    web.loop();
    handleWebFeedRequest();
    schedulerLoop();
    yield();
}
