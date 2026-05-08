# CatFeeder Remote Platform

Docker-Stack fuer Iteration 2:

- Mosquitto mit Usern und ACLs
- Backend mit MQTT-Bridge, REST-API, SSE und Postgres-Persistenz
- React UI hinter Nginx
- optionaler Traefik-Override fuer externen HTTPS-Zugriff

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

## Traefik

Wenn Traefik bereits das externe Docker-Netz `proxy` nutzt:

```sh
docker compose -f docker-compose.yml -f docker-compose.traefik.yml up --build -d
```

Relevante `.env` Werte:

```text
PUBLIC_HOST=catfeeder.mydomain.de
TRAEFIK_CERTRESOLVER=http
```

Die Labels werden nur auf den Frontend-Container gesetzt. Das Frontend proxyt
`/api` intern an das Backend.

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
```
