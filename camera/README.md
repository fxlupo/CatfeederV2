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
#define WIFI_DNS1 "1.1.1.1"
#define WIFI_DNS2 "8.8.8.8"

#define MQTT_HOST "10.18.3.50"
#define MQTT_PORT 1883
#define MQTT_USER "catfeeder_cam"
#define MQTT_PASS "..."

#define CAMERA_DEVICE_ID "catfeeder-cam"
#define LINKED_DEVICE_ID "catfeeder"
#define CAPTURE_UPLOAD_TOKEN "..."
#define CAPTURE_UPLOAD_URL "http://catload.creano.de/api/devices/catfeeder-cam/captures"
#define CAPTURE_FRAME_SIZE FRAMESIZE_VGA
#define CAPTURE_JPEG_QUALITY 14
```

`camera_credentials.h` ist in `.gitignore`.

Die Kamera soll spaeter auch aus fremden WLANs per Public URL hochladen. Darum
setzt die Firmware explizite DNS-Server. Im Status werden `dns1` und `dns2`
mitgesendet; wenn Public-Uploads mit `DNS Failed` scheitern, diese Werte zuerst
pruefen.

Die WebApp bleibt unter `https://tofu.creano.de` erreichbar. Die Kamera nutzt
fuer Uploads bewusst `http://catload.creano.de`, weil ESP32-CAM HTTPS/TLS gegen
Reverse Proxies instabil ist. Der Upload-Endpunkt ist ueber
`X-Capture-Token` geschuetzt.

Bei Public-URL Uploads loggt die Firmware vor dem Upload:

```text
[net] host=... port=80 dns=ok ip=...
[net] tcp ...:80 ok
[capture] upload start heap=... minHeap=... bytes=...
```

Ab Firmware `0.1.3` nutzt der Upload keinen `HTTPClient` mehr, sondern einen
manuellen HTTP/1.1 POST mit `Content-Length` und 1024-Byte-Schreibbloecken.
Das vermeidet instabile Chunk-/Payload-Fehler auf dem ESP32-CAM.

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
  "uploadUrl": "http://catload.creano.de/api/devices/catfeeder-cam/captures",
  "issuedAt": "2026-05-14T10:00:00+02:00"
}
```

Die Kamera haengt selbst Query-Parameter und `X-Capture-Token` an den Upload.
