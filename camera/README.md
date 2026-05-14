# CatFeeder ESP32-CAM

Eigenes PlatformIO-Unterprojekt fuer Iteration 4.

Funktion:

- verbindet sich mit WLAN
- verbindet sich mit Mosquitto
- published `status` und `event`
- subscribed `catfeeder/{cameraId}/cmd/capture`
- nimmt ein JPEG auf
- laedt das JPEG per HTTP POST ans Backend

## Lokale Konfiguration

```sh
cp camera/src/camera_credentials.example.h camera/src/camera_credentials.h
```

Dann Werte anpassen:

```cpp
#define WIFI_SSID "..."
#define WIFI_PASS "..."

#define MQTT_HOST "10.18.3.50"
#define MQTT_PORT 1883
#define MQTT_USER "catfeeder_cam"
#define MQTT_PASS "..."

#define CAMERA_DEVICE_ID "catfeeder-cam"
#define LINKED_DEVICE_ID "catfeeder"
#define CAPTURE_UPLOAD_TOKEN "..."
#define CAPTURE_UPLOAD_URL "https://tofu.creano.de/api/devices/catfeeder-cam/captures"
```

`camera_credentials.h` ist in `.gitignore`.

## Build

```sh
cd camera
pio run
```

## Test per MQTT

Topic:

```text
catfeeder/catfeeder-cam/cmd/capture
```

Payload:

```json
{
  "id": "manual-test-001",
  "type": "capture",
  "reason": "manual-test",
  "deviceId": "catfeeder",
  "correlationId": "manual-test-001",
  "uploadUrl": "https://tofu.creano.de/api/devices/catfeeder-cam/captures",
  "issuedAt": "2026-05-14T10:00:00+02:00"
}
```

Die Kamera haengt selbst Query-Parameter und `X-Capture-Token` an den Upload.
