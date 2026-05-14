# CatFeeder Remote Platform

Docker-Stack fuer Iteration 2:

- Mosquitto mit Usern und ACLs
- Backend mit MQTT-Bridge, REST-API, SSE und Postgres-Persistenz
- React UI hinter Nginx
- optionaler Traefik-Override fuer externen HTTPS-Zugriff
- Capture-Speicher fuer ESP32-CAM Fotos

Postgres-Zugangsdaten werden als einzelne Umgebungsvariablen an das Backend
gegeben, nicht als zusammengesetzte URL. Dadurch funktionieren Passwoerter mit
Sonderzeichen ohne URL-Encoding.

## Start lokal

```sh
cp .env.example .env
docker compose up --build
```

Die UI ist danach lokal erreichbar:

```text
http://localhost:8080
```

Das Backend ist direkt erreichbar:

```text
http://localhost:3000/api/health
```

## ESP MQTT-Konfiguration

In der lokalen ESP-WebUI:

- MQTT aktiv: an
- Host: IP oder DNS-Name des Docker-Hosts
- Port: `1883`
- Device ID: Wert aus `MQTT_DEVICE_ID`, default `catfeeder`
- User: Wert aus `MQTT_DEVICE_USER`, default `catfeeder`
- Passwort: Wert aus `MQTT_DEVICE_PASS`
- TLS: aus

Der Scheduler bleibt auf dem ESP. Die Plattform sendet geaenderte
Fuetterungszeiten per MQTT `cmd/config/set` an die Firmware. Der ESP speichert
die Konfiguration danach persistent im NVS.

## ESP32-CAM Foto-Pipeline

Iteration 4 startet bewusst mit Fotos statt Live-Video. Die Kamera bleibt nur
ausgehend aktiv:

1. Backend sendet ein MQTT-Kommando auf `catfeeder/{MQTT_CAMERA_ID}/cmd/capture`.
2. ESP32-CAM nimmt ein JPEG auf.
3. ESP32-CAM lädt das JPEG per HTTP `POST` ans Backend hoch.
4. Backend speichert Datei und Metadaten und zeigt Fotos in der Historie.

Relevante `.env` Werte:

```text
MQTT_CAMERA_ID=catfeeder-cam
MQTT_CAMERA_USER=catfeeder_cam
MQTT_CAMERA_PASS=catfeeder-camera-change-me
CAMERA_UPLOAD_TOKEN=catfeeder-camera-token-change-me
UPLOAD_HOST=catload.creano.de
PUBLIC_BASE_URL=http://catload.creano.de
```

Upload-Endpunkt:

```text
POST /api/devices/:cameraId/captures?deviceId=catfeeder&reason=feed_done&correlationId=...
Content-Type: image/jpeg
X-Capture-Token: <CAMERA_UPLOAD_TOKEN>
```

Bildabruf:

```text
GET /api/captures/:id/image
GET /api/devices/:id/captures
```

## Traefik

Wenn Traefik bereits das externe Docker-Netz `proxy` nutzt:

```sh
docker compose -f docker-compose.yml -f docker-compose.traefik.yml up --build -d
```

Relevante `.env` Werte:

```text
PUBLIC_HOST=tofu.creano.de
UPLOAD_HOST=catload.creano.de
PUBLIC_BASE_URL=http://catload.creano.de
TRAEFIK_CERTRESOLVER=http
```

Traefik-Routing:

- `https://tofu.creano.de` -> Frontend/WebApp
- `http://catload.creano.de/api/devices/{cameraId}/captures` -> Backend Upload

Der Kamera-Upload ist bewusst HTTP, weil ESP32-CAM HTTPS/TLS mit Reverse Proxy
instabil ist. Der Upload bleibt ueber `X-Capture-Token` geschuetzt. Die UI und
alle Browser-Zugriffe bleiben auf HTTPS.

Sicherheitsregeln fuer Uploads:

- `CAMERA_UPLOAD_TOKEN` muss gesetzt sein, sonst nimmt das Backend keine
  Capture-Uploads an.
- ESP32-CAM sendet den Token als `X-Capture-Token`.
- Leere Uploads und Payloads ohne JPEG-Startmarker `FF D8` werden abgelehnt.

## API-Auszug

```text
GET  /api/health
GET  /api/events
GET  /api/devices
GET  /api/devices/:id
GET  /api/devices/:id/config
PUT  /api/devices/:id/config
POST /api/devices/:id/feed
GET  /api/devices/:id/feed-log
GET  /api/devices/:id/telemetry
GET  /api/devices/:id/captures
GET  /api/captures/:id/image
POST /api/devices/:cameraId/captures
```

## MQTT Topics

Backend subscribed:

```text
catfeeder/+/status
catfeeder/+/telemetry
catfeeder/+/config/reported
catfeeder/+/feed/log
catfeeder/+/event
catfeeder/+/cmd/ack
catfeeder/+/cmd/result
```

Backend published:

```text
catfeeder/{deviceId}/cmd/feed
catfeeder/{deviceId}/cmd/config/get
catfeeder/{deviceId}/cmd/config/set
catfeeder/{cameraId}/cmd/capture
```
