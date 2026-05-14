#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ctype.h>
#include <esp_camera.h>
#include <esp_timer.h>

#if __has_include("camera_credentials.h")
#include "camera_credentials.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASS
#define WIFI_PASS ""
#endif

#ifndef WIFI_DNS1
#define WIFI_DNS1 "1.1.1.1"
#endif

#ifndef WIFI_DNS2
#define WIFI_DNS2 "8.8.8.8"
#endif

#ifndef MQTT_HOST
#define MQTT_HOST ""
#endif

#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif

#ifndef MQTT_USER
#define MQTT_USER ""
#endif

#ifndef MQTT_PASS
#define MQTT_PASS ""
#endif

#ifndef CAMERA_DEVICE_ID
#define CAMERA_DEVICE_ID "catfeeder-cam"
#endif

#ifndef LINKED_DEVICE_ID
#define LINKED_DEVICE_ID "catfeeder"
#endif

#ifndef CAPTURE_UPLOAD_TOKEN
#define CAPTURE_UPLOAD_TOKEN ""
#endif

#ifndef CAPTURE_UPLOAD_URL
#define CAPTURE_UPLOAD_URL ""
#endif

#ifndef CAPTURE_FRAME_SIZE
#define CAPTURE_FRAME_SIZE FRAMESIZE_VGA
#endif

#ifndef CAPTURE_JPEG_QUALITY
#define CAPTURE_JPEG_QUALITY 14
#endif

#define FW_VERSION "0.1.1"
#define MQTT_STATUS_INTERVAL_MS 10000UL
#define MQTT_RECONNECT_INTERVAL_MS 3000UL
#define WIFI_RECONNECT_INTERVAL_MS 5000UL

// AI Thinker ESP32-CAM Pinout
static const int PIN_PWDN = 32;
static const int PIN_RESET = -1;
static const int PIN_XCLK = 0;
static const int PIN_SIOD = 26;
static const int PIN_SIOC = 27;
static const int PIN_Y9 = 35;
static const int PIN_Y8 = 34;
static const int PIN_Y7 = 39;
static const int PIN_Y6 = 36;
static const int PIN_Y5 = 21;
static const int PIN_Y4 = 19;
static const int PIN_Y3 = 18;
static const int PIN_Y2 = 5;
static const int PIN_VSYNC = 25;
static const int PIN_HREF = 23;
static const int PIN_PCLK = 22;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

static String topicStatus;
static String topicEvent;
static String topicCapture;
static unsigned long lastStatusAt = 0;
static unsigned long lastMqttReconnectAt = 0;
static unsigned long lastWifiReconnectAt = 0;
static uint32_t capturesOk = 0;
static uint32_t capturesFailed = 0;
static String lastError;
static bool wifiConfigApplied = false;

String jsonString(JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  return out;
}

String urlEncode(const String& value) {
  String encoded;
  const char* hex = "0123456789ABCDEF";
  for (size_t i = 0; i < value.length(); i++) {
    const char c = value[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      encoded += '%';
      encoded += hex[(c >> 4) & 0x0F];
      encoded += hex[c & 0x0F];
    }
  }
  return encoded;
}

String addQuery(String url, const String& key, const String& value) {
  if (value.length() == 0) return url;
  url += (url.indexOf('?') >= 0) ? '&' : '?';
  url += key;
  url += '=';
  url += urlEncode(value);
  return url;
}

void publishJson(const String& topic, JsonDocument& doc, bool retained = false) {
  if (!mqtt.connected()) return;
  const String payload = jsonString(doc);
  mqtt.publish(topic.c_str(), payload.c_str(), retained);
}

void publishEvent(const char* type, const String& commandId, const String& message = "") {
  JsonDocument doc;
  doc["type"] = type;
  doc["fw"] = FW_VERSION;
  doc["cameraId"] = CAMERA_DEVICE_ID;
  doc["deviceId"] = LINKED_DEVICE_ID;
  doc["commandId"] = commandId;
  doc["message"] = message;
  doc["uptimeS"] = millis() / 1000;
  publishJson(topicEvent, doc);
}

void publishStatus(bool retained = false) {
  JsonDocument doc;
  doc["fw"] = FW_VERSION;
  doc["cameraId"] = CAMERA_DEVICE_ID;
  doc["deviceId"] = LINKED_DEVICE_ID;
  doc["ip"] = WiFi.localIP().toString();
  doc["wifiRssi"] = WiFi.RSSI();
  doc["dns1"] = WiFi.dnsIP(0).toString();
  doc["dns2"] = WiFi.dnsIP(1).toString();
  doc["mqttConnected"] = mqtt.connected();
  doc["heap"] = ESP.getFreeHeap();
  doc["minHeap"] = ESP.getMinFreeHeap();
  doc["psram"] = psramFound();
  doc["uptimeS"] = millis() / 1000;
  doc["capturesOk"] = capturesOk;
  doc["capturesFailed"] = capturesFailed;
  doc["lastError"] = lastError;
  publishJson(topicStatus, doc, retained);
}

bool setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = PIN_Y2;
  config.pin_d1 = PIN_Y3;
  config.pin_d2 = PIN_Y4;
  config.pin_d3 = PIN_Y5;
  config.pin_d4 = PIN_Y6;
  config.pin_d5 = PIN_Y7;
  config.pin_d6 = PIN_Y8;
  config.pin_d7 = PIN_Y9;
  config.pin_xclk = PIN_XCLK;
  config.pin_pclk = PIN_PCLK;
  config.pin_vsync = PIN_VSYNC;
  config.pin_href = PIN_HREF;
  config.pin_sccb_sda = PIN_SIOD;
  config.pin_sccb_scl = PIN_SIOC;
  config.pin_pwdn = PIN_PWDN;
  config.pin_reset = PIN_RESET;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = CAPTURE_FRAME_SIZE;
  config.jpeg_quality = CAPTURE_JPEG_QUALITY;
  config.fb_count = 1;
  config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  const esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    lastError = "camera-init-failed";
    Serial.printf("[camera] init failed: 0x%x\n", err);
    return false;
  }
  return true;
}

bool postFrame(camera_fb_t* frame, String uploadUrl, const String& reason, const String& correlationId, const String& feedEventId) {
  uploadUrl = addQuery(uploadUrl, "deviceId", LINKED_DEVICE_ID);
  uploadUrl = addQuery(uploadUrl, "reason", reason);
  uploadUrl = addQuery(uploadUrl, "correlationId", correlationId);
  uploadUrl = addQuery(uploadUrl, "feedEventId", feedEventId);

  HTTPClient http;
  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  bool begun = false;

  if (uploadUrl.startsWith("https://")) {
    secureClient.setInsecure();
    secureClient.setTimeout(20000);
    secureClient.setHandshakeTimeout(30);
    begun = http.begin(secureClient, uploadUrl);
  } else {
    plainClient.setTimeout(20000);
    begun = http.begin(plainClient, uploadUrl);
  }

  if (!begun) {
    lastError = "http-begin-failed";
    return false;
  }

  http.addHeader("Content-Type", "image/jpeg");
  if (String(CAPTURE_UPLOAD_TOKEN).length() > 0) {
    http.addHeader("X-Capture-Token", CAPTURE_UPLOAD_TOKEN);
  }
  http.setReuse(false);
  http.setTimeout(30000);

  Serial.printf("[capture] upload start heap=%u minHeap=%u bytes=%u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap(), frame->len);
  const int code = http.POST(frame->buf, frame->len);
  const String response = http.getString();
  http.end();

  Serial.printf("[capture] upload http=%d bytes=%u\n", code, frame->len);
  if (code < 200 || code >= 300) {
    lastError = "upload-http-" + String(code);
    Serial.println(response);
    return false;
  }
  return true;
}

void captureAndUpload(JsonDocument& command) {
  const String commandId = command["id"] | "";
  const String reason = command["reason"] | "manual";
  const String correlationId = command["correlationId"] | commandId;
  const String feedEventId = command["feedEventId"] | "";
  String uploadUrl = command["uploadUrl"] | CAPTURE_UPLOAD_URL;

  if (uploadUrl.length() == 0) {
    capturesFailed++;
    lastError = "missing-upload-url";
    publishEvent("capture_failed", commandId, lastError);
    publishStatus();
    return;
  }

  publishEvent("capture_started", commandId);

  camera_fb_t* frame = esp_camera_fb_get();
  if (!frame) {
    capturesFailed++;
    lastError = "frame-failed";
    publishEvent("capture_failed", commandId, lastError);
    publishStatus();
    return;
  }

  const bool ok = postFrame(frame, uploadUrl, reason, correlationId, feedEventId);
  esp_camera_fb_return(frame);

  if (ok) {
    capturesOk++;
    lastError = "";
    publishEvent("capture_uploaded", commandId);
  } else {
    capturesFailed++;
    publishEvent("capture_failed", commandId, lastError);
  }
  publishStatus();
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  if (String(topic) != topicCapture) return;

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    lastError = "json-parse-failed";
    publishEvent("capture_failed", "", lastError);
    return;
  }

  const String type = doc["type"] | "";
  if (type != "capture") return;
  captureAndUpload(doc);
}

void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastWifiReconnectAt < WIFI_RECONNECT_INTERVAL_MS) return;
  lastWifiReconnectAt = millis();

  Serial.printf("[wifi] connecting to %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  if (!wifiConfigApplied) {
    IPAddress dns1;
    IPAddress dns2;
    dns1.fromString(WIFI_DNS1);
    dns2.fromString(WIFI_DNS2);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, dns1, dns2);
    wifiConfigApplied = true;
    Serial.printf("[wifi] dns %s / %s\n", dns1.toString().c_str(), dns2.toString().c_str());
  }
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void ensureMqtt() {
  if (WiFi.status() != WL_CONNECTED || mqtt.connected()) return;
  if (millis() - lastMqttReconnectAt < MQTT_RECONNECT_INTERVAL_MS) return;
  lastMqttReconnectAt = millis();

  const String clientId = String("catfeeder-camera-") + CAMERA_DEVICE_ID;
  Serial.printf("[mqtt] connecting %s\n", MQTT_HOST);
  if (!mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
    Serial.printf("[mqtt] failed state=%d\n", mqtt.state());
    return;
  }

  mqtt.subscribe(topicCapture.c_str(), 1);
  publishEvent("camera_online", "");
  publishStatus(true);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  topicStatus = String("catfeeder/") + CAMERA_DEVICE_ID + "/status";
  topicEvent = String("catfeeder/") + CAMERA_DEVICE_ID + "/event";
  topicCapture = String("catfeeder/") + CAMERA_DEVICE_ID + "/cmd/capture";

  Serial.printf("\nCatFeeder CAM %s\n", FW_VERSION);
  setupCamera();

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setBufferSize(2048);

  ensureWifi();
}

void loop() {
  ensureWifi();
  ensureMqtt();
  mqtt.loop();

  if (mqtt.connected() && millis() - lastStatusAt > MQTT_STATUS_INTERVAL_MS) {
    lastStatusAt = millis();
    publishStatus(true);
  }
}
