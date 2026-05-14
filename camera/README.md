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
#define CAPTURE_UPLOAD_URL "https://tofu.creano.de/api/devices/catfeeder-cam/captures"
#define CAPTURE_FRAME_SIZE FRAMESIZE_VGA
#define CAPTURE_JPEG_QUALITY 14
```

`camera_credentials.h` ist in `.gitignore`.

Die Kamera soll spaeter auch aus fremden WLANs per Public URL hochladen. Darum
setzt die Firmware explizite DNS-Server. Im Status werden `dns1` und `dns2`
mitgesendet; wenn Public-Uploads mit `DNS Failed` scheitern, diese Werte zuerst
pruefen.

HTTPS braucht auf dem ESP32-CAM zusaetzlichen Heap fuer TLS. Deshalb ist die
Default-Aufloesung bewusst konservativ auf VGA gesetzt. Wenn Uploads stabil
laufen, kann spaeter testweise `FRAMESIZE_SVGA` und `CAPTURE_JPEG_QUALITY 12`
genutzt werden.

Bei Public-URL Uploads loggt die Firmware vor dem HTTPS-Upload:

```text
[net] host=... port=443 dns=ok ip=...
[net] tcp ...:443 ok
[capture] upload start heap=... minHeap=... bytes=...
```

Wenn TCP ok ist, aber danach `start_ssl_client: -1` kommt, liegt das Problem
am TLS-Handshake zwischen ESP32 und Reverse Proxy, nicht an DNS oder Routing.

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
