# Remote-Plattform-Plan

Status: Iteration 3 gestartet
Ziel: Dieser Plan wird waehrend der Umsetzung laufend aktualisiert und bleibt
der fuehrende Implementierungsplan fuer Remote-Zugriff, externe UI und Backend.

## Entscheidung

Die lokale ESP-WebUI wird mittelfristig reduziert. Die vollwertige Bedienung,
Konfiguration, Historie, Benachrichtigung und Remote-Steuerung wandern in eine
Docker-basierte Plattform.

Zielarchitektur:

```text
ESP32
  -> ausgehend MQTT/WebSocket
  -> MQTT Broker
  -> Backend-Service
  -> React Dashboard
  -> Push-Service
```

Der ESP wird nicht direkt aus dem Internet erreichbar gemacht. Es gibt kein
Portforwarding und keine Abhaengigkeit von fester IPv4 oder DDNS.

Der Scheduler bleibt auf dem ESP. Die externe UI darf Futterzeiten anlegen,
aendern und loeschen, aber die zeitgesteuerte Ausfuehrung erfolgt lokal auf dem
Geraet. Damit funktionieren geplante Fuetterungen auch ohne Internet.

## Zielzustand

### ESP32

Aufgaben:

- Sensoren lesen
- Stepper und Servos steuern
- lokale Fütter-State-Machine ausfuehren
- Scheduler lokal ausfuehren
- Blockadeerkennung lokal ausfuehren
- Config persistent im NVS halten
- Status, Telemetrie, Events und Config-Status nach extern melden
- Remote-Kommandos validieren und lokal ausfuehren

Die lokale WebUI bleibt zunaechst erhalten. Spaeter wird sie auf ein minimales
Setup-/Notfallinterface reduziert:

- WLAN-Setup
- MQTT/Backend-Setup
- Diagnose
- lokaler Notfall-Feed
- OTA-Freigabe
- minimaler Status

### Docker-Plattform

Ziel-Compose:

```text
docker-compose.yml
├── mqtt        Mosquitto
├── backend     API, MQTT-Bridge, Auth, Config, Logs, Push
├── frontend    React/Vite UI, ausgeliefert per Nginx oder Backend
├── db          Postgres oder SQLite, je nach Zielhosting
└── push        optional eigener Worker oder Backend-Modul
```

Iteration 2 soll nicht nur ein Demo-Dashboard sein, sondern ein brauchbarer
Plattform-Schnitt:

- Mosquitto
- umfassendes Backend
- moderne umfassende React UI
- Push-Service

## Sicherheitsmodell

Grundsatz:

- ESP verbindet sich nur ausgehend.
- React UI spricht nicht direkt mit dem ESP.
- React UI spricht nicht direkt mit Admin-MQTT-Credentials.
- Backend ist die Kontrollschicht.

Empfohlene erste Stufe:

- MQTT Broker nur mit User/Pass und ACLs.
- Pro Device eigene MQTT-Credentials.
- Backend hat Service-Credentials fuer alle erlaubten Topics.
- Frontend nutzt HTTPS/WebSocket zum Backend.
- Backend prueft Auth und Rechte.

Spaetere Haertung:

- MQTT over TLS
- Device-Zertifikate oder rotierbare Device-Tokens
- Audit-Log fuer alle Remote-Kommandos
- Rate Limits fuer Feed-Kommandos
- Rollen: Admin, Viewer, Operator

## MQTT Topic-Struktur

Basis:

```text
catfeeder/{deviceId}/...
```

Status und Telemetrie:

```text
catfeeder/{deviceId}/status
catfeeder/{deviceId}/telemetry
catfeeder/{deviceId}/health
catfeeder/{deviceId}/event
catfeeder/{deviceId}/feed/log
catfeeder/{deviceId}/config/reported
```

Kommandos:

```text
catfeeder/{deviceId}/cmd/feed
catfeeder/{deviceId}/cmd/config/set
catfeeder/{deviceId}/cmd/config/get
catfeeder/{deviceId}/cmd/time/sync
catfeeder/{deviceId}/cmd/servo
catfeeder/{deviceId}/cmd/stepper
catfeeder/{deviceId}/cmd/ota/enable
catfeeder/{deviceId}/cmd/restart
```

Antworten:

```text
catfeeder/{deviceId}/cmd/ack
catfeeder/{deviceId}/cmd/result
```

Backend-intern / UI:

```text
catfeeder/{deviceId}/config/desired
```

## Nachrichtenformate

Alle Kommandos bekommen eine eindeutige ID.

Beispiel Feed-Kommando:

```json
{
  "id": "cmd-20260507-001",
  "type": "feed",
  "grams": 20,
  "servo": 0,
  "source": "remote-ui",
  "issuedAt": "2026-05-07T19:30:00+02:00"
}
```

Ack:

```json
{
  "id": "cmd-20260507-001",
  "ok": true,
  "state": "accepted",
  "message": ""
}
```

Result:

```json
{
  "id": "cmd-20260507-001",
  "ok": true,
  "state": "done",
  "eventId": "feed-20260507-193000"
}
```

Status:

```json
{
  "fw": "1.2.3",
  "uptimeS": 12345,
  "heap": 210000,
  "wifiRssi": -48,
  "ip": "10.18.3.106",
  "dispensing": false,
  "feedAborted": false,
  "otaReady": true
}
```

Telemetry:

```json
{
  "busV": 24.1,
  "currentMA": 54,
  "powerMW": 1300,
  "fillMM": 42,
  "fillPct": 63,
  "angleDeg": 245.4,
  "ir1Analog": 123,
  "ir2Analog": 115,
  "ir1Digital": false,
  "ir2Digital": true
}
```

Feed-Event:

```json
{
  "eventId": "feed-20260507-193000",
  "type": "feed_done",
  "ts": "2026-05-07T19:30:00+02:00",
  "automatic": false,
  "grams": 20,
  "servo": 0,
  "fillBefore": 65,
  "fillAfter": 58,
  "distBefore": 38,
  "distAfter": 45,
  "ir1Pulses": 12,
  "ir2Pulses": 10,
  "aborted": false,
  "retries": 0
}
```

## Config-Sync

Die Plattform nutzt Desired/Reported Config.

Prinzip:

1. Backend speichert `desired config`.
2. ESP erhaelt `cmd/config/set`.
3. ESP validiert und speichert lokal im NVS.
4. ESP publisht `config/reported`.
5. Backend markiert Config als synchron, wenn reported dem desired Stand
   entspricht.

Wichtige Regel:

Der Scheduler bleibt auf dem ESP. Futterzeiten sind Teil der Config und werden
ueber die UI bearbeitet, aber lokal ausgefuehrt.

Config-Bereiche:

- Feed-Slots
- Standard-Futtermenge
- Servo-Endlagen
- Servo-Geschwindigkeit
- Stepper-Speed, Pulse, DIR-Setup, Haltestrom
- Steps pro Gramm
- Blockadeerkennung
- Füllstandsgrenzen
- IR-Schwelle
- Zeitzone / Sommerzeit
- MQTT/Device-Identitaet
- Benachrichtigungseinstellungen, soweit lokal noetig

## Backend

Aufgaben:

- Authentifizierung der UI
- Device Registry
- MQTT-Verbindung und Topic-Routing
- Command-Ack/Result-Tracking
- Desired/Reported Config
- Persistentes Feed-Log
- Telemetrie-Historie
- Alert-Regeln
- Push-Service
- Health-/Online-Status
- API fuer React UI

Moeglicher Stack:

- Node.js mit TypeScript und Fastify/NestJS
- MQTT.js
- Prisma
- Postgres
- WebSocket/SSE fuer Live-UI

Alternative:

- Python FastAPI
- gmqtt oder paho-mqtt
- SQLAlchemy
- Postgres

Startempfehlung:

Node.js/TypeScript passt gut zu React und vermeidet zwei Sprachwelten im
Dashboard-Teil.

## React UI

Ziel: moderne umfassende Bedienoberflaeche, nicht nur Statusseite.

Hauptbereiche:

- Dashboard
  - Online/Offline
  - aktueller Feed-State
  - Füllstand
  - Strom/Spannung
  - letzte Fuetterung
  - Warnungen
- Sofort-Fütterung
  - Menge
  - Servo-Auswahl
  - Bestätigung vor Ausloesung
- Zeitplan
  - Slots anlegen, aendern, loeschen
  - aktiv/inaktiv
  - Menge und Servo
  - Sync-Status desired/reported
- Kalibrierung
  - Servo-Endlagen
  - Stepper-Test
  - Steps pro Gramm
  - Blockade-Schwellwerte
  - Füllstandsgrenzen
- Historie
  - Feed-Events
  - Blockaden
  - Retries
  - IR-Impulse
  - Füllstand vor/nach
- Alerts
  - Push-Regeln
  - Empfaenger
  - letzte Benachrichtigungen
- System
  - Firmware-Version
  - Uptime/Heap/RSSI
  - OTA-Fenster
  - Restart-Kommando
  - Device-Konfiguration

UI-Prinzip:

- Keine direkte ESP-IP noetig.
- Live-Status ueber Backend-WebSocket/SSE.
- Kritische Aktionen mit Confirm und sichtbarem Ack/Result.
- Offline-Zustand klar anzeigen.

## Push-Service

Erste Ausbaustufe:

- Backend erzeugt Alerts.
- Push-Kanaele werden serverseitig angebunden.

Moegliche Kanaele:

- WhatsApp/CallMeBot weiterverwenden
- Telegram Bot
- Pushover
- ntfy
- E-Mail
- Web Push

Empfehlung:

Fuer den Anfang `ntfy` oder Telegram verwenden. CallMeBot kann bleiben, ist aber
durch HTTPS/mbedTLS auf dem ESP teuer. Mittelfristig sollte Push vom Backend
gesendet werden, nicht vom ESP.

## Iterationsplan

### Iteration 1 - MQTT-Grundlage auf ESP

Status: umgesetzt und lokal getestet

Ziel:

- ESP kann ausgehend MQTT sprechen.
- Bestehende lokale WebUI bleibt unveraendert nutzbar.
- Scheduler bleibt lokal.

Umfang:

- MQTT Library waehlen und integrieren.
- Config-Felder ergaenzen:
  - enabled
  - host
  - port
  - username
  - password/token
  - deviceId
  - TLS aktiv/inaktiv
- MQTT-Verbindung mit Reconnect.
- Publish:
  - `status`
  - `telemetry`
  - `event`
  - `config/reported`
- Subscribe:
  - `cmd/feed`
  - `cmd/config/get`
- Ack/Result-Mechanismus fuer Kommandos.
- Serial-Diagnose und `/api/diag` um MQTT-Status erweitern.
- Dokumentation aktualisieren.

Aktueller Implementierungsstand:

- MQTT Library: `PubSubClient`.
- TLS ist als Config-Feld vorbereitet, aber in Iteration 1 noch nicht aktiv.
- MQTT-Konfiguration ist in NVS, REST-API und lokaler WebUI vorhanden.
- ESP publisht:
  - `status`
  - `telemetry`
  - `config/reported`
  - `event`
  - `feed/log`
- ESP subscribed:
  - `cmd/feed`
  - `cmd/config/get`
- Feed-Kommandos nutzen Ack/Result.
- Aenderungen der `deviceId` werden ohne Neustart in neue MQTT-Topics
  uebernommen.
- Scheduler bleibt lokal.

Akzeptanz:

- Mosquitto lokal empfaengt Status/Telemetry. Erledigt am 2026-05-08.
- Ein MQTT-Feed-Kommando loest eine Fütterung aus. Erledigt am 2026-05-08.
- ESP bestaetigt das Kommando mit Ack und Result.
- Fütterung per lokalem Scheduler funktioniert weiterhin ohne MQTT.

### Iteration 2 - Docker-Plattform

Status: abgeschlossen am 2026-05-08

Ziel:

- Vollstaendiger lokaler/remote-faehiger Docker-Stack.

Umfang:

- `docker-compose.yml`
- Mosquitto mit Usern und ACLs.
- Backend-Service:
  - MQTT Bridge
  - REST API
  - Live WebSocket/SSE zur UI
  - Device Registry
  - Command Tracking
  - Config Desired/Reported
  - Persistentes Feed-Log
  - Telemetrie-Speicherung
  - Push-Service-Grundlage
- React UI:
  - Dashboard
  - Sofort-Fütterung
  - Zeitplanverwaltung
  - Kalibrierung
  - Historie
  - Alerts
  - System/Device
- Datenbank:
  - Start mit Postgres, falls dauerhaft auf Server/NAS geplant.
  - SQLite nur, wenn es bewusst klein und single-host bleiben soll.
- Push-Service:
  - erster Kanal: noch zu entscheiden.

Akzeptanz:

- UI zeigt Live-Status vom ESP.
- UI kann Futterzeiten aendern, Backend sendet Config an ESP.
- UI kann manuelle Fütterung ausloesen.
- Feed-Events erscheinen persistent in der Historie.
- Blockade-Abbruch erzeugt Alert.

Aktueller Implementierungsstand:

- `docker-compose.yml` fuer Mosquitto, Postgres, Backend und Frontend.
- `docker-compose.traefik.yml` mit optionalen Traefik-Labels fuer das externe
  `proxy`-Netz.
- Mosquitto erzeugt Passwort-Datei und ACLs beim Containerstart aus `.env`.
- Backend:
  - MQTT Subscribe auf Status, Telemetrie, Config, Feed-Events und Command
    Ack/Result.
  - REST API fuer Devices, Feed, Config, Historie und Telemetrie.
  - SSE fuer Live-Updates zur UI.
  - Postgres-Persistenz fuer Telemetrie, Feed-Events, Commands und Alerts.
- React UI:
  - Dashboard, Sofort-Fuetterung, Zeitplan, Kalibrierung, Historie, Alerts.
- Firmware `1.4.0` unterstuetzt `cmd/config/set` fuer Remote-Konfiguration.
- NAS-Laufzeittest erfolgreich:
  - Firmware `1.4.0` ist auf dem ESP.
  - Mosquitto, Postgres, Backend und Frontend laufen als Docker-Container.
  - Fuer den ersten Test wurde das Postgres-Volume neu initialisiert, damit die
    `.env` Zugangsdaten sicher zum Datenbank-User passen.
- Live-Daten laufen vom ESP ueber Mosquitto ins Backend:
  - `config/reported`
  - `status`
  - `telemetry`
- Sofort-Fütterung aus der React-UI wurde erfolgreich getestet:
  - Command `cmd/feed`
  - ESP fuehrt mechanisch aus
  - `cmd/result` meldet `done`
  - Feed-Event erscheint in der Historie.
- Zeitplan-Aenderung aus der React-UI wurde erfolgreich getestet:
  - Command `cmd/config/set`
  - ESP speichert persistent
  - Backend und ESP-REST zeigen dieselbe Slot-Konfiguration
  - Scheduler hat den geaenderten Slot automatisch ausgefuehrt.
- Erste Haertung:
  - Backend berechnet `online`/`ageSeconds`.
  - Offline-Alerts werden erzeugt.
  - Command-Timeouts werden sichtbar.
  - UI zeigt Command-Status und Alerts klarer.
- Bedien-Haertung:
  - Offline-Devices bekommen keine Feed-/Config-Kommandos.
  - UI deaktiviert kritische Aktionen offline.
  - Erledigte Commands und Alerts koennen bereinigt werden.
- Plattform-Version `0.2.0`:
  - Backend liefert `platformVersion` ueber `/api/health`.
  - React UI zeigt Firmware- und Plattform-Version im Header.

### Iteration 3 - Config, Logs und Remote-Betrieb haerten

Status: gestartet am 2026-05-08

Ziel:

- Remote-Betrieb ist robust, nachvollziehbar und sicher.
- UI wird Mobile-first als WebApp/PWA fuer Android und iOS weiterentwickelt,
  weil der CatFeeder hauptsaechlich am Handy bedient wird.
- Push-Benachrichtigungen werden als Mobile-WebApp-Thema mitgedacht.

Aktueller Implementierungsstand:

- Plattform-Version `0.3.0`.
- PWA-Grundlage:
  - Web-App-Manifest
  - Android/iOS Meta-Tags
  - App-Icon
  - Service Worker mit kleinem App-Shell-Cache
- Mobile-first UI-Basis:
  - Bottom-Navigation auf kleinen Viewports
  - groessere Touch-Flaechen
  - Safe-Area fuer iOS
  - einspaltige Panels als Standard
  - Tablet/Desktop erweitert das Layout progressiv
- Plattform-Version `0.3.1`:
  - Breakpoint fuer Desktop-Navigation auf `980px` angehoben, damit iPhones
    nicht mehr die zu breite Top-Navigation bekommen.
- Plattform-Version `0.4.0`:
  - Backend speichert Desired Config separat von Reported Config.
  - Backend berechnet `configSync` als `synced`, `pending`, `drift` oder
    `unknown`.
  - UI zeigt Config-Sync-Status in Dashboard, Zeitplan und Kalibrierung.
- Plattform-Version `0.4.1`:
  - UI schuetzt lokale Config-Entwuerfe vor Live-Refresh-Overwrite.
  - Config-Formulare arbeiten bevorzugt auf `configDesired`.
  - Mobile Config-Seiten haben einen Speichern-Button direkt oben.
- Plattform-Version `0.5.0`:
  - Persistentes `audit_log` fuer Feed-, Config-, Command-, Alert- und
    Connectivity-Ereignisse.
  - Terminale Commands werden nur aus der UI-Ansicht bereinigt, nicht aus der
    Datenbank-Historie.
- Plattform-Version `0.5.1`:
  - Push-Grundlage mit ntfy.sh und Browser Web Push via VAPID.
  - Push-UI im Kalibrierungsbereich.
- Plattform-Version `0.5.2`:
  - Lockfiles korrigiert.
  - Abgelaufene Web-Push-Subscriptions werden auch aus Postgres entfernt.
  - Push-Subscribe/Test/Unsubscribe und Alert-Clearing schreiben Audit-Events.
- Plattform-Version `0.6.0`:
  - Alert-Regeln von UI-Online-Status entkoppelt.
  - Offline-Push/Alert erst nach `ALERT_OFFLINE_AFTER_MS`.
  - `fill_low` und `overcurrent` sind zustandsbasiert mit Cooldown, um
    Push-Spam zu vermeiden.

Umfang:

- Vollstaendiger Desired/Reported Config-Sync.
- Konflikterkennung:
  - Config im Backend aelter als Device?
  - Device offline waehrend Config-Aenderung?
  - Command abgelaufen?
- Persistentes Event- und Audit-Log.
- Alert-Regeln:
  - Blockade
  - Füllstand niedrig
  - Device offline
  - Sensorfehler
  - OTA-Fehler
- UI fuer Alert-Konfiguration.
- Mobile-first UI-Haertung:
  - kleine Viewports zuerst
  - Touch-Bedienung
  - PWA-Manifest
  - installierbare WebApp
  - sinnvolle Push-Strategie fuer Android/iOS
- Device-Token-Rotation.
- Backup/Restore fuer Config.
- ESP-WebUI auf Minimalmodus vorbereiten.

Akzeptanz:

- Remote-Konfigurationsaenderungen sind nachvollziehbar.
- UI zeigt klar, ob Config synchron ist.
- Offline-Geraet verliert keine geplanten lokalen Fuetterungen.
- Alerts sind reproduzierbar und nicht spammy.

## Spaetere Optionen

- OTA ueber Backend orchestrieren, aber Upload weiter lokal freigeben.
- Firmware-Artefakte im Backend verwalten.
- Mehrere CatFeeder-Devices.
- Grafana/Prometheus fuer Langzeitmetriken.
- Lokaler Edge-Gateway-Modus fuer Haushalte ohne direkten MQTT-Zugang vom ESP.

## Offene Entscheidungen

- MQTT Library auf ESP:
  - `PubSubClient`: einfach, synchron, bewaehrt.
  - `AsyncMqttClient`: besser passend zu Async-Webserver, aber mehr Komplexitaet.
- TLS direkt auf dem ESP:
  - sicherer, aber mehr RAM/Flash und Zertifikatsmanagement.
  - fuer erste lokale Tests eventuell ohne TLS im LAN/VPN.
- Backend-Sprache:
  - Node.js/TypeScript empfohlen.
  - FastAPI als Alternative.
- Datenbank:
  - Postgres empfohlen fuer dauerhaften Betrieb.
  - SQLite fuer einfache Single-Host-Installation.
- Push-Kanal:
  - Telegram, ntfy, Pushover, Web Push oder CallMeBot.
- Externe Erreichbarkeit des Backends:
  - Cloudflare Tunnel
  - VPS
  - Tailscale/ZeroTier
  - anderer Reverse Proxy

## Aktueller naechster Schritt

Naechster Block in Iteration 3:

1. Alert-Regeln auf dem NAS mit echtem Offline-Test pruefen.
2. Push auf NAS mit ntfy.sh testen.
3. Browser-Push auf Android und iOS-PWA gezielt testen.
4. Audit-Ansicht filtern/suchbar machen.
