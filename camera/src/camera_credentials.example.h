#pragma once

// Diese Datei als camera_credentials.h kopieren und lokal anpassen.
// camera_credentials.h ist absichtlich in .gitignore.

#define WIFI_SSID "dein-wlan"
#define WIFI_PASS "dein-passwort"

#define MQTT_HOST "10.18.3.50"
#define MQTT_PORT 1883
#define MQTT_USER "catfeeder_cam"
#define MQTT_PASS "catfeeder-camera-change-me"

#define CAMERA_DEVICE_ID "catfeeder-cam"
#define LINKED_DEVICE_ID "catfeeder"

// Muss zu CAMERA_UPLOAD_TOKEN im Docker/.env passen.
#define CAPTURE_UPLOAD_TOKEN "catfeeder-camera-token-change-me"

// Fallback, falls das MQTT-Kommando keine uploadUrl enthaelt.
// Fuer lokalen Betrieb kann hier auch http://<NAS-IP>:3001/... stehen.
#define CAPTURE_UPLOAD_URL "https://tofu.creano.de/api/devices/catfeeder-cam/captures"
