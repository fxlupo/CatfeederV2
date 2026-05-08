// =============================================================================
// CatFeeder ESP32 - MQTT Bridge Implementation
// =============================================================================
#include "mqtt_bridge.h"

void MqttBridge::begin(Config &c, Status &st, CfgManager &cm) {
    _cfg = &c;
    _st = &st;
    _cm = &cm;
    _refreshBaseTopic();
    _client.setBufferSize(1536);
    _client.setCallback([this](char *topic, uint8_t *payload, unsigned int length) {
        _callback(topic, payload, length);
    });
    _setState(c.mqttEnabled ? "idle" : "disabled", false);
}

void MqttBridge::loop() {
    if (!_cfg || !_st) return;
    _refreshBaseTopic();
    if (!_cfg->mqttEnabled || strlen(_cfg->mqttHost) == 0 || _cfg->mqttTls) {
        if (_client.connected()) _client.disconnect();
        _setState(_cfg->mqttTls ? "tls-pending" : "disabled", false);
        return;
    }
    if (WiFi.status() != WL_CONNECTED) {
        _setState("no-wifi", false);
        return;
    }

    if (!_client.connected()) {
        _connect();
        return;
    }

    _client.loop();

    uint32_t now = millis();
    if (now - _lastStatus >= MQTT_STATUS_INTERVAL) {
        publishStatus();
        _lastStatus = now;
    }
    if (now - _lastTelemetry >= MQTT_TELEMETRY_INTERVAL) {
        publishTelemetry();
        _lastTelemetry = now;
    }
}

bool MqttBridge::consumeFeedRequest(uint16_t &grams, uint8_t &servo, char *cmdId, size_t cmdIdLen) {
    if (!_feedRequested) return false;
    grams = _feedGrams;
    servo = _feedServo;
    strlcpy(cmdId, _feedCmdId, cmdIdLen);
    _feedRequested = false;
    _feedCmdId[0] = '\0';
    return true;
}

void MqttBridge::_connect() {
    uint32_t now = millis();
    if (now - _lastConnectAttempt < 5000) return;
    _lastConnectAttempt = now;

    _client.setServer(_cfg->mqttHost, _cfg->mqttPort);
    char clientId[64];
    snprintf(clientId, sizeof(clientId), "catfeeder-%s", _deviceId);

    _setState("connecting", false);
    bool ok;
    if (strlen(_cfg->mqttUser) > 0) {
        ok = _client.connect(clientId, _cfg->mqttUser, _cfg->mqttPass);
    } else {
        ok = _client.connect(clientId);
    }

    if (!ok) {
        char st[16];
        snprintf(st, sizeof(st), "err%d", _client.state());
        _setState(st, false);
        Serial.printf("[MQTT] connect failed state=%d\n", _client.state());
        return;
    }

    _setState("connected", true);
    Serial.printf("[MQTT] connected %s:%u as %s\n", _cfg->mqttHost, _cfg->mqttPort, clientId);
    _subscribe();
    publishConfigReported();
    publishStatus();
    publishTelemetry();
}

void MqttBridge::_refreshBaseTopic() {
    if (!_cfg) return;

    char nextId[sizeof(_deviceId)];
    strlcpy(nextId, _cfg->mqttDeviceId, sizeof(nextId));
    if (strlen(nextId) == 0) strlcpy(nextId, "catfeeder", sizeof(nextId));

    if (strcmp(_deviceId, nextId) == 0) return;
    if (_client.connected()) _client.disconnect();
    strlcpy(_deviceId, nextId, sizeof(_deviceId));
    snprintf(_base, sizeof(_base), "catfeeder/%s", _deviceId);
}

void MqttBridge::_subscribe() {
    char t[120];
    if (_topic(t, sizeof(t), "cmd/feed")) _client.subscribe(t);
    if (_topic(t, sizeof(t), "cmd/config/get")) _client.subscribe(t);
    if (_topic(t, sizeof(t), "cmd/config/set")) _client.subscribe(t);
}

void MqttBridge::_callback(char *topic, uint8_t *payload, unsigned int length) {
    char msg[1536];
    size_t n = length < sizeof(msg) - 1 ? length : sizeof(msg) - 1;
    memcpy(msg, payload, n);
    msg[n] = '\0';

    char feedTopic[120], cfgGetTopic[120], cfgSetTopic[120];
    _topic(feedTopic, sizeof(feedTopic), "cmd/feed");
    _topic(cfgGetTopic, sizeof(cfgGetTopic), "cmd/config/get");
    _topic(cfgSetTopic, sizeof(cfgSetTopic), "cmd/config/set");

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, msg);
    const char *id = "";
    if (!err) id = doc["id"] | "";

    if (strcmp(topic, feedTopic) == 0) {
        if (err) {
            publishCommandResult("", false, "rejected", "invalid-json");
            return;
        }
        if (_feedRequested) {
            publishCommandResult(id, false, "rejected", "busy");
            return;
        }
        uint16_t grams = constrain((uint16_t)(doc["grams"] | _cfg->defaultGrams), 1, 500);
        uint8_t servo = constrain((uint8_t)(doc["servo"] | 0), 0, 2);
        _feedGrams = grams;
        _feedServo = servo;
        strlcpy(_feedCmdId, id, sizeof(_feedCmdId));
        _feedRequested = true;
        return;
    }

    if (strcmp(topic, cfgGetTopic) == 0) {
        publishCommandResult(id, true, "accepted");
        publishConfigReported();
        return;
    }

    if (strcmp(topic, cfgSetTopic) == 0) {
        if (err) {
            publishCommandResult("", false, "rejected", "invalid-json");
            return;
        }
        JsonObject src = doc["config"].is<JsonObject>() ? doc["config"].as<JsonObject>() : doc.as<JsonObject>();
        if (!_applyConfigSet(src)) {
            publishCommandResult(id, false, "rejected", "invalid-config");
            return;
        }
        if (_cm) _cm->save(*_cfg);
        publishCommandResult(id, true, "accepted");
        publishConfigReported();
        publishCommandResult(id, true, "done");
    }
}

bool MqttBridge::_applyConfigSet(JsonObject src) {
    if (!_cfg) return false;

    if (!src["slots"].isNull()) {
        JsonArray slots = src["slots"];
        if (slots.size() > MAX_SLOTS) return false;
        for (int i = 0; i < min((int)slots.size(), MAX_SLOTS); i++) {
            _cfg->slots[i].active = slots[i]["on"] | false;
            _cfg->slots[i].hour   = constrain((uint8_t)(slots[i]["h"] | 0), 0, 23);
            _cfg->slots[i].minute = constrain((uint8_t)(slots[i]["m"] | 0), 0, 59);
            _cfg->slots[i].grams  = constrain((uint16_t)(slots[i]["g"] | DEFAULT_FEED_GRAMS), 1, 500);
            _cfg->slots[i].servo  = constrain((uint8_t)(slots[i]["sv"] | 0), 0, 2);
            _cfg->slots[i].doneToday = false;
        }
    }

    if (!src["defaultGrams"].isNull()) _cfg->defaultGrams = constrain((uint16_t)src["defaultGrams"], 1, 500);
    if (!src["stepsPerGram"].isNull()) _cfg->stepsPerGram = constrain((uint16_t)src["stepsPerGram"], 1, 5000);
    if (!src["stepperSpeed"].isNull()) _cfg->stepperSpeed = constrain((uint16_t)src["stepperSpeed"], 100, 10000);
    if (!src["stepperPulseUS"].isNull()) _cfg->stepperPulseUS = constrain((uint16_t)src["stepperPulseUS"], 2, 50);
    if (!src["stepperDirSetupUS"].isNull()) _cfg->stepperDirSetupUS = constrain((uint16_t)src["stepperDirSetupUS"], 0, 2000);
    if (!src["stepperHoldMS"].isNull()) _cfg->stepperHoldMS = constrain((uint16_t)src["stepperHoldMS"], 0, 5000);
    if (!src["stepperBlockMA"].isNull()) _cfg->stepperBlockMA = constrain((uint16_t)src["stepperBlockMA"], 100, 5000);
    if (!src["blockRetries"].isNull()) _cfg->blockRetries = constrain((uint8_t)src["blockRetries"], 0, 10);
    if (!src["blockReverseSteps"].isNull()) _cfg->blockReverseSteps = constrain((uint16_t)src["blockReverseSteps"], 50, 5000);
    if (!src["blockMinRotPct"].isNull()) _cfg->blockMinRotPct = constrain((uint8_t)src["blockMinRotPct"], 5, 90);
    if (!src["servoSpeedDPS"].isNull()) _cfg->servoSpeedDPS = constrain((uint16_t)src["servoSpeedDPS"], 20, 3000);
    if (!src["s1Open"].isNull()) _cfg->s1Open = constrain((uint8_t)src["s1Open"], 0, 180);
    if (!src["s1Close"].isNull()) _cfg->s1Close = constrain((uint8_t)src["s1Close"], 0, 180);
    if (!src["s2Open"].isNull()) _cfg->s2Open = constrain((uint8_t)src["s2Open"], 0, 180);
    if (!src["s2Close"].isNull()) _cfg->s2Close = constrain((uint8_t)src["s2Close"], 0, 180);
    return true;
}

void MqttBridge::publishStatus() {
    if (!_client.connected() || !_st) return;
    JsonDocument d;
    d["fw"] = FW_VERSION;
    d["uptimeS"] = _st->uptimeS;
    d["heap"] = _st->heap;
    d["wifiRssi"] = _st->wifiRSSI;
    d["ip"] = _st->ip;
    d["hostname"] = _st->hostname;
    d["wifiMode"] = _st->wifiMode;
    d["feeds"] = _st->feeds;
    d["otaReady"] = _st->otaReady;
    d["mqttConnected"] = _client.connected();
    _publishJson("status", d, true);
}

void MqttBridge::publishTelemetry() {
    if (!_client.connected() || !_st) return;
    JsonDocument d;
    d["busV"] = _st->busV;
    d["currentMA"] = _st->currentMA;
    d["powerMW"] = _st->powerMW;
    d["fillMM"] = _st->distMM;
    d["fillPct"] = _st->fillPct;
    d["angleDeg"] = _st->angleDeg;
    d["ir1Analog"] = _st->ir1Analog;
    d["ir2Analog"] = _st->ir2Analog;
    d["ir1Digital"] = _st->ir1Digital;
    d["ir2Digital"] = _st->ir2Digital;
    d["overcurrent"] = _st->overcurrent;
    d["fillLow"] = _st->fillLow;
    _publishJson("telemetry", d);
}

void MqttBridge::publishConfigReported() {
    if (!_client.connected() || !_cfg) return;
    JsonDocument d;
    d["fw"] = FW_VERSION;
    d["deviceId"] = _cfg->mqttDeviceId;
    d["hostname"] = _cfg->hostname;
    d["defaultGrams"] = _cfg->defaultGrams;
    d["stepsPerGram"] = _cfg->stepsPerGram;
    d["stepperSpeed"] = _cfg->stepperSpeed;
    d["stepperPulseUS"] = _cfg->stepperPulseUS;
    d["stepperDirSetupUS"] = _cfg->stepperDirSetupUS;
    d["stepperHoldMS"] = _cfg->stepperHoldMS;
    d["stepperBlockMA"] = _cfg->stepperBlockMA;
    d["blockRetries"] = _cfg->blockRetries;
    d["blockReverseSteps"] = _cfg->blockReverseSteps;
    d["blockMinRotPct"] = _cfg->blockMinRotPct;
    d["servoSpeedDPS"] = _cfg->servoSpeedDPS;
    d["s1Open"] = _cfg->s1Open;
    d["s1Close"] = _cfg->s1Close;
    d["s2Open"] = _cfg->s2Open;
    d["s2Close"] = _cfg->s2Close;
    JsonArray slots = d["slots"].to<JsonArray>();
    for (int i = 0; i < MAX_SLOTS; i++) {
        JsonObject s = slots.add<JsonObject>();
        s["on"] = _cfg->slots[i].active;
        s["h"] = _cfg->slots[i].hour;
        s["m"] = _cfg->slots[i].minute;
        s["g"] = _cfg->slots[i].grams;
        s["sv"] = _cfg->slots[i].servo;
    }
    _publishJson("config/reported", d, true);
}

void MqttBridge::publishFeedEvent(const FeedEvent &e) {
    if (!_client.connected()) return;
    JsonDocument d;
    d["type"] = e.feedAborted ? "feed_aborted" : "feed_done";
    d["ts"] = e.timeStr;
    d["automatic"] = e.isAuto;
    d["grams"] = e.grams;
    d["servo"] = e.servo;
    d["fillBefore"] = e.fillBefore;
    d["fillAfter"] = e.fillAfter;
    d["distBefore"] = e.distBefore;
    d["distAfter"] = e.distAfter;
    d["ir1Pulses"] = e.ir1Pulses;
    d["ir2Pulses"] = e.ir2Pulses;
    d["aborted"] = e.feedAborted;
    d["retries"] = e.blockRetryCount;
    _publishJson("feed/log", d);
    _publishJson("event", d);
}

void MqttBridge::publishCommandResult(const char *id, bool ok, const char *state, const char *message) {
    if (!_client.connected()) return;
    JsonDocument d;
    d["id"] = id;
    d["ok"] = ok;
    d["state"] = state;
    d["message"] = message;
    _publishJson(strcmp(state, "accepted") == 0 ? "cmd/ack" : "cmd/result", d);
}

bool MqttBridge::_topic(char *out, size_t outLen, const char *suffix) const {
    return snprintf(out, outLen, "%s/%s", _base, suffix) < (int)outLen;
}

void MqttBridge::_publishJson(const char *suffix, JsonDocument &doc, bool retained) {
    char topic[120];
    if (!_topic(topic, sizeof(topic), suffix)) return;
    char payload[1536];
    size_t n = serializeJson(doc, payload, sizeof(payload));
    if (n == 0 || n >= sizeof(payload)) return;
    _client.publish(topic, (const uint8_t*)payload, n, retained);
}

void MqttBridge::_setState(const char *state, bool connected) {
    if (!_st) return;
    _st->mqttEnabled = _cfg ? _cfg->mqttEnabled : false;
    _st->mqttConnected = connected;
    _st->mqttLastSeenS = millis() / 1000;
    strlcpy(_st->mqttState, state, sizeof(_st->mqttState));
}
