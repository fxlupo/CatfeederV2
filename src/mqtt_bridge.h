// =============================================================================
// CatFeeder ESP32 - MQTT Bridge fuer Remote-Plattform
// =============================================================================
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

class MqttBridge {
public:
    void begin(Config &c, Status &st);
    void loop();
    void publishStatus();
    void publishTelemetry();
    void publishConfigReported();
    void publishFeedEvent(const FeedEvent &e);
    void publishCommandResult(const char *id, bool ok, const char *state, const char *message = "");

    bool consumeFeedRequest(uint16_t &grams, uint8_t &servo, char *cmdId, size_t cmdIdLen);
    bool connected() { return _client.connected(); }

private:
    WiFiClient   _wifi;
    PubSubClient _client{_wifi};
    Config      *_cfg = nullptr;
    Status      *_st  = nullptr;

    uint32_t _lastConnectAttempt = 0;
    uint32_t _lastStatus = 0;
    uint32_t _lastTelemetry = 0;
    bool     _feedRequested = false;
    uint16_t _feedGrams = 0;
    uint8_t  _feedServo = 0;
    char     _feedCmdId[40] = "";
    char     _deviceId[33] = "";
    char     _base[80] = "";

    void _refreshBaseTopic();
    void _connect();
    void _subscribe();
    void _callback(char *topic, uint8_t *payload, unsigned int length);
    bool _topic(char *out, size_t outLen, const char *suffix) const;
    void _publishJson(const char *suffix, JsonDocument &doc, bool retained = false);
    void _setState(const char *state, bool connected);
};
