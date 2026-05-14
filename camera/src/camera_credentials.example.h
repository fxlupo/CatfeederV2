#pragma once

// Diese Datei als camera_credentials.h kopieren und lokal anpassen.
// camera_credentials.h ist absichtlich in .gitignore.

#define WIFI_SSID "dein-wlan"
#define WIFI_PASS "dein-passwort"
// Explizite DNS-Server, damit Public URLs auch in fremden WLANs robust
// aufgeloest werden. Bei Bedarf durch lokale DNS-Server ersetzen.
#define WIFI_DNS1 "1.1.1.1"
#define WIFI_DNS2 "8.8.8.8"

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

// HTTPS braucht auf dem ESP32-CAM zusaetzlichen Heap fuer TLS.
// VGA/14 ist bewusst konservativ; spaeter kann man auf SVGA/12 hochgehen.
#define CAPTURE_FRAME_SIZE FRAMESIZE_VGA
#define CAPTURE_JPEG_QUALITY 14
