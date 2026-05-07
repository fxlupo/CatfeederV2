// =============================================================================
// CatFeeder ESP32 – Webserver Implementation
// =============================================================================
#include "web.h"
#include <ESPmDNS.h>

void Web::begin(Config &c, Status &st, Sensors &se, Motors &mo, CfgManager &cm, FeedLog &fl) {
    _c = &c; _st = &st; _se = &se; _mo = &mo; _cm = &cm; _fl = &fl;
    _wifi();
    _routes();
    _srv.addHandler(&_sse);
    _srv.begin();
    if (MDNS.begin(_c->hostname)) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("[Web] http://%s.local\n", _c->hostname);
    }
    Serial.println(F("[Web] Server OK"));
}

void Web::loop() {
    if (millis() - _lastSSE > 2000) { _sendSSE(); _lastSSE = millis(); }
}

void Web::closeSSE() {
    _sse.close();
}

String Web::ip() {
    return _ap ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
}

// ═══════════════════════ WiFi ═══════════════════════════════════════════════

void Web::_wifi() {
    if (strlen(_c->ssid) > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.setHostname(_c->hostname);
        WiFi.begin(_c->ssid, _c->pass);
        Serial.printf("[WiFi] Verbinde '%s' ", _c->ssid);
        uint32_t t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_CONNECT_TIMEOUT) {
            delay(400); Serial.print(".");
        }
        if (WiFi.status() == WL_CONNECTED) {
            _ap = false;
            WiFi.setSleep(false);   // Power-Save deaktivieren für stabiles OTA
            Serial.printf("\n[WiFi] OK  IP %s\n", WiFi.localIP().toString().c_str());
            return;
        }
        Serial.println(F("\n[WiFi] Fehlgeschlagen"));
    }
    _ap = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
    WiFi.setSleep(false);
    Serial.printf("[WiFi] AP '%s'  pw '%s'  IP %s\n",
                  WIFI_AP_SSID, WIFI_AP_PASS, WiFi.softAPIP().toString().c_str());
}

// ═══════════════════════ SSE ════════════════════════════════════════════════

void Web::_sendSSE() {
    if (_sse.count() == 0) return;
    JsonDocument d;
    d["fw"]  = FW_VERSION;
    d["fdi"] = _mo->dispensing();
    d["fba"] = _mo->feedAborted();
    d["v"]   = _st->busV;
    d["ma"]  = _st->currentMA;
    d["mw"]  = _st->powerMW;
    d["mm"]  = _st->distMM;
    d["fl"]  = _st->fillPct;
    d["ang"] = _st->angleDeg;
    d["t"]   = _st->timeStr;
    d["up"]  = _st->uptimeS;
    d["hp"]  = _st->heap;
    d["fc"]  = _st->feeds;
    d["ip"]  = _st->ip;
    d["hn"]  = _st->hostname;
    d["wm"]  = _st->wifiMode;
    d["wr"]  = _st->wifiRSSI;
    d["ot"]  = _st->otaReady;
    d["op"]  = _st->otaPort;
    d["oph"] = _st->otaPhase;
    d["oe"]  = _st->lastOtaError;

    JsonObject ir = d["ir"].to<JsonObject>();
    ir["a1"] = _st->ir1Analog;  ir["a2"] = _st->ir2Analog;
    ir["d1"] = _st->ir1Digital; ir["d2"] = _st->ir2Digital;

    JsonObject er = d["er"].to<JsonObject>();
    er["i"] = !_st->ok_ina; er["v"] = !_st->ok_vl;
    er["a"] = !_st->ok_as;  er["r"] = !_st->ok_rtc;
    er["oc"]= _st->overcurrent; er["lo"]= _st->fillLow;

    String j; serializeJson(d, j);
    _sse.send(j.c_str(), "s", millis());
}

// ═══════════════════════ Routen ═════════════════════════════════════════════

void Web::_routes() {
    // Hauptseite
    _srv.on("/", HTTP_GET, [this](AsyncWebServerRequest *r) {
        r->send(200, "text/html", _html());
    });

    // GET Status
    _srv.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *r) {
        JsonDocument d;
        d["v"]=_st->busV; d["ma"]=_st->currentMA; d["mw"]=_st->powerMW;
        d["mm"]=_st->distMM; d["fl"]=_st->fillPct; d["ang"]=_st->angleDeg;
        d["t"]=_st->timeStr; d["up"]=_st->uptimeS; d["hp"]=_st->heap;
        d["fc"]=_st->feeds; d["fw"]=FW_VERSION;
        d["wifi"]=_st->wifiOK; d["ap"]=_st->apMode; d["ip"]=_st->ip;
        d["hostname"]=_st->hostname; d["wifiMode"]=_st->wifiMode;
        d["rssi"]=_st->wifiRSSI; d["otaReady"]=_st->otaReady;
        d["otaPort"]=_st->otaPort; d["otaPhase"]=_st->otaPhase;
        d["lastOtaError"]=_st->lastOtaError;
        String j; serializeJson(d, j);
        r->send(200, "application/json", j);
    });

    // GET Diagnostics
    _srv.on("/api/diag", HTTP_GET, [this](AsyncWebServerRequest *r) {
        JsonDocument d;
        d["fw"] = FW_VERSION;
        d["ip"] = _st->ip;
        d["hostname"] = _st->hostname;
        d["wifiMode"] = _st->wifiMode;
        d["wifiOK"] = _st->wifiOK;
        d["apMode"] = _st->apMode;
        d["rssi"] = _st->wifiRSSI;
        d["otaReady"] = _st->otaReady;
        d["otaPort"] = _st->otaPort;
        d["otaPhase"] = _st->otaPhase;
        d["lastOtaError"] = _st->lastOtaError;
        d["uptimeS"] = _st->uptimeS;
        d["heap"] = _st->heap;
        String j; serializeJsonPretty(d, j);
        r->send(200, "application/json", j);
    });

    // GET Config
    _srv.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *r) {
        JsonDocument d;
        d["fw"]=FW_VERSION;
        JsonArray sl = d["slots"].to<JsonArray>();
        for (int i = 0; i < MAX_SLOTS; i++) {
            JsonObject o = sl.add<JsonObject>();
            o["on"] = _c->slots[i].active;
            o["h"]  = _c->slots[i].hour;
            o["m"]  = _c->slots[i].minute;
            o["g"]  = _c->slots[i].grams;
            o["sv"] = _c->slots[i].servo;
        }
        d["spg"]=_c->stepsPerGram; d["spd"]=_c->stepperSpeed;
        d["spu"]=_c->stepperPulseUS; d["sdi"]=_c->stepperInvertDir;
        d["sds"]=_c->stepperDirSetupUS; d["shm"]=_c->stepperHoldMS;
        d["sbm"]=_c->stepperBlockMA;
        d["bkr"]=_c->blockRetries;
        d["bks"]=_c->blockReverseSteps;
        d["bkp"]=_c->blockMinRotPct;
        d["dfg"]=_c->defaultGrams;
        JsonArray wa = d["wa"].to<JsonArray>();
        for (int i = 0; i < WA_USERS; i++) {
            JsonObject u = wa.add<JsonObject>();
            u["on"] = _c->waUsers[i].active;
            u["ph"] = _c->waUsers[i].phone;
            u["ak"] = _c->waUsers[i].apikey;
        }
        d["s1o"]=_c->s1Open; d["s1c"]=_c->s1Close;
        d["s2o"]=_c->s2Open; d["s2c"]=_c->s2Close;
        d["svs"]=_c->servoSpeedDPS;
        d["feM"]=_c->fillEmptyMM; d["ffM"]=_c->fillFullMM;
        d["irt"]=_c->irThreshold;
        d["tz"]=_c->utcOffset; d["dst"]=_c->dst;
        d["hn"]=_c->hostname;
        String j; serializeJson(d, j);
        r->send(200, "application/json", j);
    });

    // POST Config
    auto bodyHandler = [](AsyncWebServerRequest *r) {};

    _srv.on("/api/config", HTTP_POST, bodyHandler, NULL,
        [this](AsyncWebServerRequest *r, uint8_t *d, size_t l, size_t, size_t) {
        JsonDocument doc; if (deserializeJson(doc, d, l)) { r->send(400); return; }
        if (!doc["slots"].isNull()) {
            JsonArray sl = doc["slots"];
            for (int i = 0; i < min((int)sl.size(), MAX_SLOTS); i++) {
                _c->slots[i].active = sl[i]["on"] | false;
                _c->slots[i].hour   = sl[i]["h"]  | 0;
                _c->slots[i].minute = sl[i]["m"]  | 0;
                _c->slots[i].grams  = sl[i]["g"]  | 20;
                _c->slots[i].servo  = sl[i]["sv"] | 0;
            }
        }
        if (!doc["spg"].isNull()) _c->stepsPerGram = doc["spg"];
        if (!doc["spd"].isNull()) _c->stepperSpeed = constrain((uint16_t)doc["spd"], 100, 10000);
        if (!doc["spu"].isNull()) _c->stepperPulseUS = constrain((uint16_t)doc["spu"], 2, 50);
        if (!doc["sdi"].isNull()) _c->stepperInvertDir = doc["sdi"];
        if (!doc["sds"].isNull()) _c->stepperDirSetupUS = constrain((uint16_t)doc["sds"], 0, 2000);
        if (!doc["shm"].isNull()) _c->stepperHoldMS = constrain((uint16_t)doc["shm"], 0, 5000);
        if (!doc["sbm"].isNull()) _c->stepperBlockMA    = constrain((uint16_t)doc["sbm"], 100, 5000);
        if (!doc["bkr"].isNull()) _c->blockRetries      = constrain((uint8_t)doc["bkr"],  0, 10);
        if (!doc["bks"].isNull()) _c->blockReverseSteps = constrain((uint16_t)doc["bks"], 50, 5000);
        if (!doc["bkp"].isNull()) _c->blockMinRotPct    = constrain((uint8_t)doc["bkp"],  5, 90);
        if (!doc["dfg"].isNull()) _c->defaultGrams      = constrain((uint16_t)doc["dfg"], 1, 500);
        if (!doc["wa"].isNull()) {
            JsonArray wa = doc["wa"];
            for (int i = 0; i < min((int)wa.size(), WA_USERS); i++) {
                _c->waUsers[i].active = wa[i]["on"] | false;
                strlcpy(_c->waUsers[i].phone,  wa[i]["ph"] | "", sizeof(_c->waUsers[i].phone));
                strlcpy(_c->waUsers[i].apikey, wa[i]["ak"] | "", sizeof(_c->waUsers[i].apikey));
            }
        }
        if (!doc["s1o"].isNull()) _c->s1Open   = doc["s1o"];
        if (!doc["s1c"].isNull()) _c->s1Close  = doc["s1c"];
        if (!doc["s2o"].isNull()) _c->s2Open   = doc["s2o"];
        if (!doc["s2c"].isNull()) _c->s2Close  = doc["s2c"];
        if (!doc["svs"].isNull()) _c->servoSpeedDPS = constrain((uint16_t)doc["svs"], 20, 3000);
        if (!doc["feM"].isNull()) _c->fillEmptyMM = doc["feM"];
        if (!doc["ffM"].isNull()) _c->fillFullMM  = doc["ffM"];
        if (!doc["irt"].isNull()) _c->irThreshold  = doc["irt"];
        if (!doc["tz"].isNull())  _c->utcOffset    = doc["tz"];
        if (!doc["dst"].isNull()) _c->dst          = doc["dst"];
        if (!doc["hn"].isNull()) {
            strlcpy(_c->hostname, doc["hn"] | "catfeeder", sizeof(_c->hostname));
        }
        _cm->save(*_c);
        r->send(200, "application/json", "{\"ok\":1}");
    });

    // POST Feed
    _srv.on("/api/feed", HTTP_POST, bodyHandler, NULL,
        [this](AsyncWebServerRequest *r, uint8_t *d, size_t l, size_t, size_t) {
        JsonDocument doc; deserializeJson(doc, d, l);
        feedGrams     = doc["g"] | 20;
        feedServo     = doc["sv"]| 0;
        feedRequested = true;
        r->send(200, "application/json", "{\"ok\":1}");
    });

    // POST Servo test
    _srv.on("/api/sv", HTTP_POST, bodyHandler, NULL,
        [this](AsyncWebServerRequest *r, uint8_t *d, size_t l, size_t, size_t) {
        JsonDocument doc; deserializeJson(doc, d, l);
        uint8_t n = doc["n"] | 1;
        const char *cmd = doc["cmd"] | "";
        if (strcmp(cmd, "open") == 0) {
            if (n == 1) _mo->s1Open(*_c); else _mo->s2Open(*_c);
        } else if (strcmp(cmd, "close") == 0) {
            if (n == 1) _mo->s1Close(*_c); else _mo->s2Close(*_c);
        } else {
            _mo->setAngle(n, doc["a"] | 90);
        }
        r->send(200, "application/json", "{\"ok\":1}");
    });

    // POST Stepper test
    _srv.on("/api/stp", HTTP_POST, bodyHandler, NULL,
        [this](AsyncWebServerRequest *r, uint8_t *d, size_t l, size_t, size_t) {
        JsonDocument doc; deserializeJson(doc, d, l);
        _mo->moveBlocking(doc["s"]|100, *_c);
        r->send(200, "application/json", "{\"ok\":1}");
    });

    // POST Time sync
    _srv.on("/api/time", HTTP_POST, bodyHandler, NULL,
        [this](AsyncWebServerRequest *r, uint8_t *d, size_t l, size_t, size_t) {
        JsonDocument doc; deserializeJson(doc, d, l);
        _se->setTime(doc["y"]|2025, doc["mo"]|1, doc["d"]|1,
                     doc["h"]|0, doc["mi"]|0, doc["s"]|0);
        r->send(200, "application/json", "{\"ok\":1}");
    });

    // POST WiFi
    _srv.on("/api/wifi", HTTP_POST, bodyHandler, NULL,
        [this](AsyncWebServerRequest *r, uint8_t *d, size_t l, size_t, size_t) {
        JsonDocument doc; deserializeJson(doc, d, l);
        strlcpy(_c->ssid, doc["ssid"] | "", sizeof(_c->ssid));
        strlcpy(_c->pass, doc["pw"] | "", sizeof(_c->pass));
        _cm->save(*_c);
        r->send(200, "application/json", "{\"ok\":1,\"msg\":\"Neustart…\"}");
        delay(2000); ESP.restart();
    });

    // POST Reset
    _srv.on("/api/reset", HTTP_POST, [this](AsyncWebServerRequest *r) {
        _cm->defaults(*_c); _cm->save(*_c);
        r->send(200, "application/json", "{\"ok\":1}");
        delay(1500); ESP.restart();
    });

    // GET Event-Log
    _srv.on("/api/log", HTTP_GET, [this](AsyncWebServerRequest *r) {
        JsonDocument d;
        d["count"] = _fl->count;
        JsonArray ev = d["events"].to<JsonArray>();
        // Neueste zuerst
        for (int8_t i = _fl->count - 1; i >= 0; i--) {
            const FeedEvent &e = _fl->get(i);
            JsonObject o = ev.add<JsonObject>();
            o["t"]   = e.timeStr;
            o["a"]   = e.isAuto;
            o["g"]   = e.grams;
            o["sv"]  = e.servo;
            o["db"]  = e.distBefore; o["da"]  = e.distAfter;
            o["fb"]  = e.fillBefore; o["fa"]  = e.fillAfter;
            o["i1p"] = e.ir1Pulses;  o["i2p"] = e.ir2Pulses;
            o["abt"] = e.feedAborted;
            o["bkn"] = e.blockRetryCount;
        }
        String j; serializeJson(d, j);
        r->send(200, "application/json", j);
    });

    // GET PWA-Manifest
    _srv.on("/manifest.json", HTTP_GET, [this](AsyncWebServerRequest *r) {
        String m = F("{\"name\":\"CatFeeder\",\"short_name\":\"CatFeeder\","
                     "\"start_url\":\"/\",\"display\":\"standalone\","
                     "\"background_color\":\"#0f1117\",\"theme_color\":\"#e8564a\","
                     "\"icons\":[{\"src\":\"/icon.svg\",\"type\":\"image/svg+xml\","
                     "\"sizes\":\"any\"}]}");
        r->send(200, "application/manifest+json", m);
    });

    // GET App-Icon (SVG Pfotenabdruck)
    _srv.on("/icon.svg", HTTP_GET, [](AsyncWebServerRequest *r) {
        String s = F("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'>"
                     "<rect width='100' height='100' rx='20' fill='#e8564a'/>"
                     "<circle cx='50' cy='62' r='20' fill='#fff'/>"
                     "<circle cx='25' cy='42' r='11' fill='#fff'/>"
                     "<circle cx='75' cy='42' r='11' fill='#fff'/>"
                     "<circle cx='37' cy='28' r='8' fill='#fff'/>"
                     "<circle cx='63' cy='28' r='8' fill='#fff'/></svg>");
        r->send(200, "image/svg+xml", s);
    });

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin",  "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
}
