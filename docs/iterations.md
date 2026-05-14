# Iterationen

## 2026-05-14 - ESP32-CAM Upload-Diagnose erweitert

Scope:

- Kamera-Firmware auf `0.1.2` gesetzt.
- Public-Upload diagnostiziert vor HTTP/HTTPS:
  - URL Host/Port
  - DNS Ergebnis
  - TCP-Verbindung zum Zielport
- Negative HTTPClient-Resultate werden als `upload-connect-failed` im Status
  sichtbar.
- Camera-README um TLS-Diagnosehinweise erweitert.

Verifikation:

- Camera-Build lokal pruefen.

## 2026-05-14 - ESP32-CAM HTTPS Upload stabilisiert

Scope:

- Kamera-Firmware auf `0.1.1` gesetzt.
- Default-Capture fuer HTTPS auf `FRAMESIZE_VGA` und JPEG-Qualitaet `14`
  reduziert, damit TLS mehr freien Heap bekommt.
- Framebuffer auf einen Buffer reduziert.
- TLS-Handshake- und HTTP-Timeout erhoeht.
- Upload-Start loggt Heap, Min-Heap und JPEG-Bytes.
- Kamera-Status meldet `minHeap`.

Verifikation:

- Camera-Build lokal pruefen.

## 2026-05-14 - ESP32-CAM Public-URL DNS gehärtet

Scope:

- ESP32-CAM Firmware setzt explizite DNS-Server fuer Public-URL Uploads.
- Kamera-Status meldet `dns1` und `dns2`.
- Beispiel-Credentials und Camera-README dokumentieren `WIFI_DNS1` und
  `WIFI_DNS2`.
- USB-Upload-Port und konservativer Upload-Speed fuer das ESP32-CAM Dev-Board
  in `camera/platformio.ini` gesetzt.

Verifikation:

- Camera-Build lokal pruefen.

## 2026-05-14 - Iteration 4 ESP32-CAM Firmware-Basis

Scope:

- Eigenes PlatformIO-Unterprojekt `camera/` angelegt.
- Firmware fuer AI-Thinker ESP32-CAM vorbereitet.
- Kamera verbindet sich per WLAN und MQTT.
- Kamera subscribed `catfeeder/{cameraId}/cmd/capture`.
- Kamera nimmt JPEG auf und laedt es per HTTP POST ans Backend.
- Kamera sendet Status- und Event-Meldungen.
- Lokale `camera_credentials.h` wird nicht versioniert.

Verifikation:

- Camera-Build lokal pruefen.

## 2026-05-14 - Iteration 4 Capture-Backend und UI-Basis (Platform 0.7.0)

Scope:

- Plattform-Version auf `0.7.0` gesetzt.
- Backend kann ESP32-CAM JPEG-Fotos per HTTP entgegennehmen.
- Neue `captures`-Tabelle speichert Metadaten zu Kamera, Device, Reason,
  Korrelation, Dateipfad, MIME-Type und Dateigröße.
- Capture-Dateien werden in einem Docker-Volume unter `/data/captures`
  gespeichert.
- Neue API-Endpunkte:
  - `POST /api/devices/:cameraId/captures`
  - `GET /api/devices/:id/captures`
  - `GET /api/captures/:id/image`
- Backend sendet MQTT-Capture-Kommandos an `catfeeder/{cameraId}/cmd/capture`
  bei Remote-Fütterung und Feed-Log Events.
- Mosquitto-ACL und Docker Compose kennen einen getrennten Kamera-User.
- React UI zeigt Fotos im Historie-Tab.
- Service-Worker-Cache auf `0.7.0` aktualisiert.

Verifikation:

- Backend- und Frontend-Build lokal pruefen.

## 2026-05-14 - Iteration 4 Camera Capture Pipeline geplant

Scope:

- Iteration 4 als reine Foto-Pipeline geplant.
- Entscheidung: kein HLS/Live-Video in der ersten Kamera-Iteration.
- ESP32-CAM wird als separates Device angebunden:
  - MQTT fuer Status, Events und Capture-Kommandos.
  - HTTP-JPEG-Upload ans Backend.
- Backend-Scope definiert:
  - Capture-Upload-Endpunkt
  - Capture-Bild-Endpunkt
  - `captures`-Tabelle
  - Docker-Volume fuer Bilddaten
  - Audit-Events fuer Capture-Anforderung, Upload und Fehler
- UI-Scope definiert:
  - Fotos in der Feed-Historie
  - Kamera-Status
  - kleine Capture-Galerie
- Sicherheitsrahmen definiert:
  - Kamera bleibt nur ausgehend aktiv.
  - Upload per Token/Shared Secret.
  - Kamera-Credentials getrennt von CatFeeder-Credentials.

Verifikation:

- Plan in `docs/remote-platform-plan.md` dokumentiert.

## 2026-05-14 - Alert- und Push-Regeln gehärtet (Platform 0.6.0)

Scope:

- Plattform-Version auf `0.6.0` gesetzt.
- Alert-Regeln von UI-Online-Status entkoppelt:
  - `DEVICE_OFFLINE_AFTER_MS` bleibt fuer schnellen UI-Status.
  - `ALERT_OFFLINE_AFTER_MS` steuert, ab wann Offline einen Alert/Push erzeugt.
- Neuer Default: Offline-Alert erst nach `180000 ms` statt sofort nach UI-Offline.
- Zustandsbasierte Alerts fuer `fill_low` und `overcurrent`:
  - ein Alert pro aktivem Zustand
  - Reset, wenn Telemetrie wieder normal meldet
  - Cooldown ueber `ALERT_COOLDOWN_MS`, Default `3600000 ms`
- `GET /api/health` meldet die aktiven Alert-Timings.
- Docker Compose und `.env.example` um die neuen Alert-Parameter erweitert.

Verifikation:

- Backend- und Frontend-Build lokal pruefen.

## 2026-05-13 - Audit/Push nachgeschärft (Platform 0.5.2)

Scope:

- Plattform-Version auf `0.5.2` gesetzt.
- Backend- und Frontend-`package-lock.json` auf dieselbe Version wie
  `package.json` gebracht, damit `npm ci`/Docker reproduzierbar bleibt.
- Web-Push räumt abgelaufene Subscriptions nun auch aus Postgres auf, nicht nur
  aus dem In-Memory-State.
- Push-Subscribe, Push-Unsubscribe, Push-Test und abgelaufene Subscriptions
  werden ins Audit-Log geschrieben.
- `clearAlerts()` schreibt jetzt ebenfalls einen Audit-Eintrag.

Verifikation:

- Backend- und Frontend-Build lokal pruefen.

## 2026-05-08 - Push-Strategie vorbereitet (Platform 0.5.1)

Scope:

- Plattform-Version auf `0.5.1` gesetzt.
- `PushService` in `platform/backend/src/push.ts` als eigenständiges Modul.

**Provider: ntfy.sh**
- Kein Infrastructure-Overhead. URL in `.env` als `NTFY_URL=https://ntfy.sh/<kanal>`.
- `createAlert()` sendet automatisch bei level=critical/warning.
- Title, Tags und Priority werden gesetzt.

**Provider: Web Push (VAPID)**
- VAPID-Schlüssel via `npx web-push generate-vapid-keys`, in `.env` als
  `VAPID_PUBLIC_KEY`, `VAPID_PRIVATE_KEY`, `VAPID_CONTACT`.
- Subscriptions in `push_subscriptions` Postgres-Tabelle persistiert.
- Beim Start werden gespeicherte Subscriptions geladen.
- Abgelaufene Subscriptions (HTTP 410) werden automatisch entfernt.

**Service Worker** (`public/sw.js`):
- Cache-Version auf `0.5.1` aktualisiert.
- `push`-Event-Handler: zeigt Notification mit Title/Body/Icon.
- `notificationclick`: bringt App-Fenster in den Vordergrund oder öffnet neues.

**Backend-Endpunkte:**
- `GET  /api/push/config`         – VAPID-Key, Provider-Status, Subscription-Anzahl
- `POST /api/push/subscribe`      – Web-Push-Subscription registrieren
- `DELETE /api/push/subscribe`    – Subscription entfernen
- `POST /api/push/test`           – Testbenachrichtigung an alle aktiven Provider

**Frontend: Push-Bereich in „Kalibrierung"**
- ntfy.sh: Kanal-URL anzeigen, Testbutton.
- Web Push: Status-Badge (Aktiv/Nicht aktiviert/Verweigert), Aktivieren-/Deaktivieren-Button.
- Konfigurationsanleitung wenn Provider nicht gesetzt.
- `urlBase64ToUint8Array()` für VAPID-Key-Konvertierung.

Verifikation:

- Backend- und Frontend-Build erfolgreich.

## 2026-05-08 - Command-/Audit-Log gehärtet (Platform 0.5.0)

Scope:

- Plattform-Version auf `0.5.0` gesetzt.

**audit_log-Tabelle:**
- Felder: device_id, actor (device|remote-ui|system), category
  (feed|config|connectivity|command|alert), action, payload.
- Index auf (device_id, id desc).
- Neue Funktion `auditLog()` wird an allen signifikanten Stellen aufgerufen:
  feed.issued/completed/aborted, config.sent/reported/confirmed/failed,
  command-Timeouts, device.online/offline, alert.created, commands.cleared.

**command_log – permanente History:**
- `clearTerminalCommands()` löscht nicht mehr aus der Datenbank.
- Nur der In-Memory-State wird bereinigt (reine Ansichtsfunktion).
- DB-Records bleiben als lückenloses Audit-Trail erhalten.

**Neue API-Endpunkte:**
- `GET /api/devices/:id/audit`     – Audit-Log, paginiert, optional nach category gefiltert
- `GET /api/devices/:id/commands`  – Command-Verlauf, paginiert, optional nach state gefiltert

**Frontend – History-Tab:**
- „Feed-Events"-Panel (wie bisher) + neues „Audit-Log"-Panel.
- Actor- und Category-Badges mit farblicher Bedeutung.
- Automatisch geladen beim Wechsel auf den History-Tab.
- Aktualisieren-Button.

Verifikation:

- Backend- und Frontend-Build erfolgreich.

## 2026-05-08 - Iteration 3 Config-Formular entkoppelt (Platform 0.4.1)

Scope:

- Plattform-Version auf `0.4.1` gesetzt.
- React UI ueberschreibt lokale Config-Entwuerfe nicht mehr durch Live-Refresh,
  solange ungespeicherte Aenderungen im Formular stehen.
- Config-Formulare verwenden `configDesired` als Bearbeitungsbasis, wenn diese
  vorhanden ist; sonst `configReported`.
- Nach Speichern setzt die UI den Config-Sync lokal sofort auf `pending`, bis
  der naechste ESP-Report eintrifft.
- Zeitplan und Kalibrierung haben zusaetzlich oben einen Speichern-Button,
  damit man auf Mobile nicht erst ans Ende der Seite scrollen muss.

Verifikation:

- Backend- und Frontend-Build lokal pruefen.

## 2026-05-08 - Iteration 3 Config-Sync Basis (Platform 0.4.0)

Scope:

- Plattform-Version auf `0.4.0` gesetzt.
- Backend speichert `config_desired` getrennt von `config_reported`.
- Datenbank-Migration ergaenzt:
  - `devices.config_desired`
  - `devices.config_desired_at`
  - `devices.config_reported_at`
- Backend berechnet pro Device `configSync`:
  - `synced`
  - `pending`
  - `drift`
  - `unknown`
- Firmware-Version `fw` wird beim Config-Vergleich ignoriert, weil sie kein
  konfigurierbarer Zielwert ist.
- React UI zeigt Config-Sync im Dashboard, Zeitplan und Kalibrierung.
- Service-Worker-Cache auf `catfeeder-ui-0.4.0` gesetzt.

Verifikation:

- Backend- und Frontend-Build lokal pruefen.

## 2026-05-08 - Iteration 3 Mobile-Menü korrigiert (Platform 0.3.1)

Scope:

- Plattform-Version auf `0.3.1` gesetzt.
- Mobile/Desktop-Breakpoint von `720px` auf `980px` angehoben, damit iPhone
  und grosse Handy-Viewports konsequent die Bottom-Navigation verwenden.
- Dashboard-Grid-Breakpoint auf `1180px` angehoben, damit das breite
  Vier-Spalten-Layout erst auf echten Desktop-Breiten greift.
- Service-Worker-Cache auf `catfeeder-ui-0.3.1` gesetzt.

Verifikation:

- Code-Check lokal, Docker-Build muss auf dem NAS erfolgen.

## 2026-05-08 - Iteration 3 gestartet: Mobile-first PWA-Basis (Platform 0.3.0)

Scope:

- Plattform-Version auf `0.3.0` gesetzt.
- Backend `/api/health` meldet `platformVersion` jetzt als `0.3.0`.
- React UI nutzt `0.3.0` als sichtbare Plattform-Version im Header.
- PWA-Metadaten ergaenzt:
  - `manifest.webmanifest`
  - Theme-Color
  - iOS/Android WebApp-Meta-Tags
  - App-Icon
  - Service Worker mit kleinem App-Shell-Cache
- Frontend-Dockerfile kopiert `public/`, damit Manifest, Icon und Service
  Worker im Nginx-Image landen.
- UI auf Mobile-first umgestellt:
  - Bottom-Navigation fuer Handy-Bedienung
  - groessere Touch-Ziele
  - iOS Safe-Area-Unterstuetzung
  - sticky Statuskopf
  - einspaltige mobile Panels
  - Desktop-Layout bleibt ab Tablet/Breitbild erhalten

Verifikation:

- Code-Check lokal, Docker-Build muss auf dem NAS erfolgen.

## 2026-05-08 - Iteration 2 abgeschlossen: Docker-Plattform Baseline (1.4.0 / Platform 0.2.0)

Scope:

- Firmware-Version auf `1.4.0` gesetzt.
- Plattform-Version `0.2.0` eingefuehrt.
- Backend liefert `platformVersion` ueber `/api/health`.
- React UI zeigt Firmware- und Plattform-Version im Header.
- MQTT-Firmware um `cmd/config/set` erweitert, damit Backend/UI
  Fuetterungszeiten und Kalibrierwerte persistent an den ESP senden koennen.
- Docker Compose fuer Mosquitto, Postgres, Backend und Frontend angelegt.
- Mosquitto startet mit Usern, Passwort-Datei und dynamisch erzeugten ACLs.
- Backend als Node.js/TypeScript-Service angelegt:
  - MQTT Bridge fuer `catfeeder/#`
  - REST API
  - SSE Live-Stream
  - Device Registry im Speicher
  - Persistenz fuer Telemetrie, Feed-Events, Commands und Alerts in Postgres
  - Feed- und Config-Kommandos Richtung ESP
- React UI angelegt:
  - Dashboard
  - Sofort-Fuetterung
  - Zeitplanverwaltung
  - Kalibrierung
  - Historie
  - Alerts
- Traefik-Override mit Labels nach bestehendem `proxy`-Netz-Muster angelegt.
- `platform/README.md` mit Start- und Betriebsnotizen angelegt.
- Mosquitto-Image auf `eclipse-mosquitto:2` gesetzt, da `2.1` kein gueltiger
  Docker-Hub-Tag ist.
- Mosquitto Passwortdatei und ACL werden nun unter `/mosquitto/data` erzeugt
  und dem Container-User `mosquitto` uebergeben, damit der Broker sie nach dem
  Drop auf den nicht-root User lesen kann.
- Backend nutzt fuer Postgres einzelne Umgebungsvariablen statt
  `DATABASE_URL`, damit Passwoerter mit Sonderzeichen nicht URL-encodiert
  werden muessen.
- Backend-Default fuer `POSTGRES_HOST` ist `db`, passend zum Compose-Service,
  damit der Container nicht versehentlich auf `localhost` verbindet.

Verifikation:

- `pio run -e esp32dev` erfolgreich.
- Docker/Compose konnte in dieser Shell nicht ausgefuehrt werden
  (`docker: command not found`).
- NAS-Test erfolgreich:
  - Firmware `1.4.0` auf dem ESP.
  - Mosquitto, Postgres, Backend und Frontend laufen als Docker-Container.
  - Postgres-Volume wurde fuer den ersten Test per `docker compose down -v`
    neu initialisiert, damit `.env` Passwort und DB-User zusammenpassen.
- Backend setzt `seenAt` nur noch bei echten MQTT-Nachrichten vom ESP, nicht
  beim Lesen der Device-API oder beim Queuen von UI-Kommandos.
- Remote-Plattform empfängt `config/reported`, `status` und `telemetry` vom
  ESP.
- Sofort-Fütterung aus der React-UI getestet:
  - Backend sendet `cmd/feed`.
  - ESP fuehrt 5 g / beide Servos aus.
  - Command geht auf `done`.
  - `feed/log` wird persistent im Backend sichtbar.
- Zeitplan-Aenderung aus der React-UI getestet:
  - Backend sendet `cmd/config/set`.
  - ESP speichert den geaenderten Slot persistent im NVS.
  - `/api/config` auf dem ESP zeigt dieselbe Slot-Konfiguration wie das
    Backend.
  - Der geaenderte Scheduler-Slot hat automatisch eine Fuetterung ausgeloest.
- Backend/UI-Haertung begonnen:
  - API liefert `online` und `ageSeconds` pro Device.
  - Backend erzeugt Offline-Alerts mit Anti-Spam, wenn ein Device laenger als
    30 Sekunden nicht gesehen wurde.
  - Haengende Commands wechseln nach Timeout auf `timeout`.
  - Device-Ansicht hydriert Commands aus Postgres nach Backend-Neustart.
  - React UI zeigt Command-Status und Alerts mit klaren Status-Badges.
- Bedien-Haertung erweitert:
  - Feed- und Config-Kommandos werden bei Offline-Device im Backend abgelehnt.
  - React UI sperrt Feed-Start und Config-Speichern, wenn das Device offline
    ist.
  - Command-Anzeige trennt aktive Commands von juengerer Historie.
  - Terminale Commands und Alerts koennen per API/UI bereinigt werden.

Ergebnis:

- Iteration 2 ist abgeschlossen.
- Iteration 3 startet ab hier offiziell mit Mobile-first/PWA-Fokus fuer
  Android und iOS, robustem Remote-Betrieb, Push und Config-Sync-Haertung.

## 2026-05-08 - MQTT-Grundlage auf ESP umgesetzt (1.3.0)

Scope:

- Firmware-Version auf `1.3.0` gesetzt.
- `PubSubClient` als kleine MQTT-Bibliothek ergaenzt.
- `MqttBridge` eingefuehrt:
  - Reconnect-Loop
  - Status-Publish
  - Telemetrie-Publish
  - `config/reported` Publish
  - Feed-Event Publish
  - Subscribe auf `cmd/feed` und `cmd/config/get`
  - Ack/Result fuer Feed-Kommandos
- MQTT-Konfiguration in NVS, REST-API und lokaler WebUI ergaenzt.
- `/api/diag` um MQTT-Status erweitert.
- Lokaler Scheduler bleibt unveraendert auf dem ESP.
- Remote-Plattform-Plan aktualisiert.

Verifikation:

- `pio run -e esp32dev` erfolgreich.
- `pio run -e esp32dev_ota` erfolgreich.
- ESP verbindet sich mit lokalem Mosquitto.
- Mosquitto empfaengt `config/reported`, `status` und `telemetry`.
- MQTT-Feed-Kommando ueber `catfeeder/catfeeder/cmd/feed` loest Stepper und
  Servoablauf aus.

## 2026-05-07 - Remote-Plattform-Plan angelegt

Scope:

- `docs/remote-platform-plan.md` als lebendes Arbeitsdokument angelegt.
- Zielarchitektur festgelegt:
  `ESP32 -> MQTT/WebSocket ausgehend -> Mosquitto -> Backend -> React UI -> Push`.
- Entscheidung dokumentiert:
  - Scheduler bleibt auf dem ESP.
  - Externe UI darf Futterzeiten anlegen/aendern/loeschen.
  - Keine direkte Internet-Exposition des ESP.
  - Lokale ESP-WebUI wird erst parallel behalten und spaeter auf Minimalmodus
    reduziert.
- Iterationen geplant:
  - Iteration 1: MQTT-Grundlage auf ESP.
  - Iteration 2: Docker-Plattform mit Mosquitto, umfassendem Backend,
    moderner React UI und Push-Service.
  - Iteration 3: Config-Sync, Logs und Remote-Betrieb haerten.
- README auf den Plan verlinkt.

Verifikation:

- Doku-only.

## 2026-05-07 - ArduinoOTA fuer grosse Firmware stabilisiert (1.2.3)

Scope:

- Firmware-Version auf `1.2.3` gesetzt.
- ArduinoOTA internen Receive-Timeout von Default `1000 ms` auf `15000 ms`
  erhoeht. Ursache: bei einem ~1.17 MB Image reicht ein einzelner WLAN-Haenger
  ueber 1 Sekunde fuer einen Abbruch mitten im Transfer.
- OTA-Progress-Logging auf Serial von "jedes Paket" auf 10-Prozent-Schritte
  reduziert, damit der OTA-Transfer nicht durch UART-Ausgaben gestoert wird.
- OTA-Upload-Timeout im PlatformIO-Environment von `60` auf `120` Sekunden
  erhoeht.
- WLAN robuster fuer OTA konfiguriert:
  - `WiFi.persistent(false)`
  - `WiFi.setAutoReconnect(true)`
  - maximale TX-Power
  - WiFi-Sleep bleibt deaktiviert

Hinweis:

- Diese Stabilisierung greift erst, nachdem `1.2.3` einmal auf dem ESP laeuft.
  Wenn die aktuell installierte Firmware OTA nicht zuverlaessig annimmt, muss
  dieser Stand einmal per USB geflasht werden.

Verifikation:

- `pio run -e esp32dev` erfolgreich.
- `pio run -e esp32dev_ota` erfolgreich.

## 2026-05-07 - Servo-Endlage nach Erfolg und Blockade garantiert (1.2.2)

Scope:

- Firmware-Version auf `1.2.2` gesetzt.
- Fütterungs-State-Machine korrigiert:
  - Erfolgreiche Fütterung fährt jetzt kontrolliert:
    `Servo zu -> 1000 ms -> Servo auf -> 500 ms -> Servo final zu -> 1000 ms -> detach`.
  - Die letzte Zu-Phase wartet jetzt `1000 ms` statt `400 ms`, bevor die Servos
    detached werden.
  - Nach Blockade-Abbruch springt die State-Machine nicht mehr direkt nach
    `DS_DONE`, sondern immer zuerst nach `DS_FINAL_CLOSE`.
- Finaler Close fährt immer beide Servos auf Zu, auch wenn nur ein einzelner
  Servo für die Fütterung ausgewählt war.

Erwartetes Verhalten:

- Nach erfolgreicher Fütterung stehen beide Servos geschlossen.
- Nach Blockade-Abbruch stehen beide Servos geschlossen.

Verifikation:

- `pio run -e esp32dev` erfolgreich.
- `pio run -e esp32dev_ota` erfolgreich.

## 2026-05-07 - Dokumentation auf Firmware 1.2.1 geradegezogen

Scope:

- README von fixer Slot-Zahl auf `MAX_SLOTS`-basierte, firmwarekonfigurierbare
  Fuetterungszeiten umgestellt.
- README um aktuellen Stand zu Blockadeerkennung, Feed-Log, WhatsApp und API
  ergaenzt.
- `docs/architecture.md` auf aktuellen Stand gebracht:
  - NVS-Einzelkeys statt Struct-Blob
  - Fütterungs-State-Machine mit bewusst blockierender Stepperphase
  - aktive Blockadeerkennung per AS5600/INA219
  - Feed-Logging
  - OTA-Stabilisierung
  - WhatsApp-Task auf Core 0
- Veraltete Aussagen zu "spaeterer" Blockadeauswertung und noch blockierendem
  Gesamt-Fuetterungsablauf entfernt.

Verifikation:

- `git diff --check` erfolgreich.

## 2026-05-07 - Blockadeerkennung mit Richtungsumkehr (1.1.4)

Scope:

- Firmware-Version auf `1.1.4` gesetzt, Config-Schema auf 7.

**Erkennung:**
- AS5600 (primär): Winkeldelta je 64-Schritt-Yield-Fenster (~53 ms bei 1200 Steps/s)
  gegen Erwartungswert geprüft. NEMA17 direkt auf Stepper-Welle → 64 × 1,8° = 115,2°
  erwartet. Bei `blockMinRotPct=30%` muss mindestens 34,6° Rotation sichtbar sein.
- INA219 (sekundär): Strom > `stepperBlockMA` als Bestätigung.
- Blockade gilt als erkannt wenn **beide** Kriterien in ≥ 2 aufeinanderfolgenden
  Fenstern erfüllt sind (verhindert Falsch-Positive durch Einzelspikes).
- Wrap-Around 359°→0° korrekt behandelt.

**Reaktion (konfigurierbar):**
```
Blockade erkannt → Stop
  → Rückwärts (blockReverseSteps Steps, Default 1000)
  → 300 ms Pause
  → Wiederholung (blockRetries mal, Default 2)
  → Bei Überschreitung: Fütterung abgebrochen + Event-Log-Eintrag
```

**Neue Config-Felder (NVS-persistent):**
- `blockRetries` (bkr): Max. Wiederholversuche, Default 2
- `blockReverseSteps` (bks): Rückwärts-Steps, Default 1000
- `blockMinRotPct` (bkp): Min. Rotation % pro Fenster, Default 30 %
- `stepperBlockMA` bereits vorhanden, jetzt in eigenem UI-Abschnitt

**Architektur:**
- `Sensors::readInstant()` liest INA219 + AS5600 frisch (nicht gecacht) für
  Erkennung während `moveBlocking()`.
- `moveBlocking()` gibt jetzt `bool` zurück (true = OK, false = Blockade).
- Neue State-Machine-States: `DS_BLOCK_REVERSE`, `DS_BLOCK_WAIT`, `DS_BLOCK_RETRY`.
- `motors.begin()` nimmt jetzt `Sensors&` entgegen.
- Blockade-Zählung nur aktiv wenn `_blockDetect = true` (nur in DS_STEPPER,
  nicht beim Selbsttest oder Web-Testfahrt).

**Web-UI:**
- Neuer Abschnitt "Blockadeerkennung" in Kalibrierung.
- Status-Tab: "Fütterung läuft" Banner (gelb) + "Blockade abgebrochen" Alert (rot).
- Log-Tab: Badge "⚠ Blockade" (rot) oder "N× Retry" (gelb) pro Eintrag.
- SSE: `fdi` (dispensing) + `fba` (feed aborted) live.

Kalibrierung:
- `stepperBlockMA` muss durch Testfütterungen ermittelt werden
  (Soll zwischen normalem Laufstrom und Stallstrom liegen).
- `blockMinRotPct=30%` ist ein konservativer Startwert; bei häufigen
  Falsch-Positiven erhöhen, bei verpassten Blockaden verringern.

Verifikation:

- `pio run` erfolgreich für `esp32dev`.
- RAM: 16.2 %, Flash: 77.3 %.

## 2026-05-07 - WhatsApp-Benachrichtigung bei Blockade-Abbruch (1.2.0)

Scope:

- Firmware-Version auf `1.2.0` gesetzt.
- Library: `hafidhh/Callmebot-ESP32` (GitHub) via HTTPS/HTTPClient.
  Aufruf: `Callmebot.whatsappMessage(phone, apikey, message)`.

**Funktionsweise:**
- Bei Blockade-Abbruch (`motors.feedAborted() == true` in `checkFeedComplete()`)
  wird `sendWhatsAppAlert()` aufgerufen.
- Nachrichtentext (ASCII, kein Umlaut):
  `"CatFeeder Blockade! Fuetterung (Xg) nach Y Versuch(en) abgebrochen. Bitte Futterzufuhr pruefen. [DD.MM.YYYY HH:MM:SS]"`
- Gesendet wird an alle aktiven Nutzer mit ausgefüllter Telefonnummer und API-Key.
- Bei AP-Modus (kein WLAN) wird kein Versand unternommen.

**Konfiguration (2 Nutzer, NVS-persistent):**
- `WaUser`: active (bool), phone[20] (+49...), apikey[32]
- NVS-Keys: `w0a/w0p/w0k`, `w1a/w1p/w1k`
- Web-UI: neuer Abschnitt "WhatsApp Benachrichtigungen" in Einstellungen,
  je Nutzer: aktiv-Toggle, Telefonnummer, API-Key.
- API: `wa`-Array in GET/POST `/api/config`.

**CallMeBot-Registrierung:**
1. WhatsApp-Nachricht an +34 644 59 16 58 senden: `I allow callmebot to send me messages`
2. API-Key per Rückricht erhalten.
3. Nummer und Key in der Web-UI eintragen.

**Flash-Anmerkung:**
- HTTPClient + WiFiClientSecure (gezogen durch CallMeBot-Library): +150 KB Flash.
- Flash-Stand: 88.9 % (1165 KB / 1310 KB). Noch ~145 KB Headroom für OTA.
- Keine weiteren großen Libraries mehr hinzufügen ohne vorher zu optimieren.

Verifikation:

- `pio run` erfolgreich für `esp32dev`.
- RAM: 16.8 %, Flash: 88.9 %.

## 2026-05-07 - WhatsApp: Absturz durch blockierenden HTTPS-Aufruf behoben (1.2.1)

Scope:

- Firmware-Version auf `1.2.1` gesetzt.

Ursache:
- `Callmebot.whatsappMessage()` ist ein blockierender HTTPS-Aufruf (WiFiClientSecure
  + mbedTLS). Lief direkt im Haupt-Loop → blockierte ArduinoOTA.handle() und
  hielt Ressourcen (TCP-Verbindungen, Heap-Fragmente) nicht sauber frei.
- Mit der Zeit: Heap-Fragmentierung durch wiederholte SSL-Sessions → ESPAsyncWebServer
  bekommt keine Ressourcen mehr → Web-UI unerreichbar.

Fix:
- `sendWhatsAppAlert()` startet jetzt einen FreeRTOS-Task (`_whatsappTask`) auf
  Core 0 (WiFi-Core) mit 12 KB Stack (ausreichend für mbedTLS/HTTPS).
- Haupt-Loop (Core 1) bleibt vollständig frei während des HTTPS-Aufrufs.
- Task kopiert alle benötigten Daten (Telefon, API-Key, Nachricht) in ein
  heap-alloziertes Struct (`WaTaskArg`), räumt es am Ende selbst auf (`delete`).
- Guard (`_waTaskHandle != NULL`): kein zweiter Task während noch einer läuft.
- Bei Task-Erstellungsfehler (Heap-Mangel) wird Struct sauber freigegeben.

Verifikation:

- `pio run` erfolgreich für `esp32dev`.

## 2026-05-07 - Blockadeerkennung: OR-Logik, Schwellen-Default, fillLow-Spam (1.1.6)

Scope:

- Firmware-Version auf `1.1.6` gesetzt.

**Blockadeerkennung: AND → OR**
- Datenlage aus realen Testfütterungen: Strom ändert sich bei Stall kaum
  (390–487 mA normal, 446–487 mA beim Stall → Δ < 15 %).
  Rotation dagegen springt sauber von ~13.5° auf 0°.
- AND-Bedingung verhinderte Auslösung weil Strom-Threshold nie erreicht wurde.
- Fix: `rotLow || curHigh` (OR). Rotation-Stop allein reicht als Auslöser.
  Strom bleibt als Backup im OR, aber mit deutlich höherem Threshold (z.B. 700+ mA).

**blockMinRotPct Default: 30% → 5%**
- Theoretischer Erwartungswert (115.2°) passt nicht zur Realität (~13.5° gemessen).
  Tatsächlicher Threshold bei 30% war 34.6° > 13.5° → hätte immer geblockt.
  Bei 10% war Threshold 11.5° → Margin von nur 2° (13.5° normal − 11.5°).
  Default jetzt 5% = 5.76° Threshold → klare Trennung (0° geblockt, 13.5° normal).
  Empfehlung: in der Web-UI den passenden Wert für den eigenen Aufbau finden.

**fillLow / Overcurrent Spam behoben**
- `notifyEvent` wurde bei jedem Loop-Durchlauf gefeuert solange der Zustand anhielt.
- Fix: Edge-Detection (steigende Flanke) mit `static bool lastFillLow/Overcurrent`
  in `refreshStatus()`. Benachrichtigung nur einmalig beim Übergang false→true.

Verifikation:

- `pio run` erfolgreich für `esp32dev`.

## 2026-05-07 - Blockadeerkennung: Kalibrier-Logging (1.1.5)

Scope:

- Firmware-Version auf `1.1.5` gesetzt.
- Nach jeder Fütterung (Stepper-Phase mit aktiver Blockadeerkennung) wird
  eine Kalibrier-Zeile auf Serial ausgegeben:
  ```
  [Block] Kalibrierung: Peak-Strom=XXX mA (Schwelle=YYYY mA)  Min-Rotation=ZZ.Z° (Schwelle=34.6°)
  ```
- Gibt dem Anwender die gemessenen Ist-Werte für Peak-Strom und minimales
  Winkeldelta, um `stepperBlockMA` und `blockMinRotPct` gezielt einstellen zu können.
- Kein Logging bei Selbsttest oder Web-Testfahrt (nur wenn `_blockDetect` aktiv).

Verifikation:

- `pio run` erfolgreich für `esp32dev`.

## 2026-05-07 - OTA-Stabilität: drei Ursachen behoben (1.1.3)

Scope:

- Firmware-Version auf `1.1.3` gesetzt.

Ursachen und Fixes:

**Bug 1 – Kritisch: `otaActive` blieb nach OTA-Fehler dauerhaft `true`**
- `onError`-Callback setzte `otaActive` nie zurück.
- Folge: Main Loop lief nach einem fehlgeschlagenen OTA-Versuch dauerhaft
  im `if (otaActive) return` — Web-UI tot, Sensoren eingefroren, OTA nicht
  mehr erreichbar → einziger Ausweg war USB-Flash.
- Fix: `otaActive = false` als erstes Statement in `onError`.

**Bug 2 – WiFi Power-Save-Modus aktiv**
- ESP32 schaltet das WiFi-Radio im Standard periodisch ab (Modem Sleep).
- Erzeugt Latenz-Spikes von 10–100 ms direkt im OTA-Datenstrom → zufällige
  Timeouts und Abbrüche an wechselnden Positionen.
- Fix: `WiFi.setSleep(false)` nach erfolgreichem Connect und im AP-Modus.

**Bug 3 – Offene SSE-Verbindungen während OTA-Transfer**
- ESPAsyncWebServer läuft auf eigenem FreeRTOS-Task und konkuriert auch
  während OTA um WiFi-Stack-Ressourcen.
- Fix: `web.closeSSE()` in `onStart` schließt alle SSE-Clients vor dem
  Transfer.

**Zusatz: OTA-Timeout erhöht**
- `upload_flags = --timeout=60` im `esp32dev_ota`-Environment.
- Verhindert vorzeitige Upload-Abbrüche bei temporär langsamer WiFi-Verbindung.

Verifikation:

- `pio run` erfolgreich für `esp32dev`.

## 2026-05-07 - IR-Impulszählung und Sensorübersicht (1.1.2)

Scope:

- Firmware-Version auf `1.1.2` gesetzt.

**IR-Impulszählung während Fütterung:**
- `Motors::moveBlocking()` zählt Flanken (HIGH↔LOW) auf `PIN_IR1_D0` und
  `PIN_IR2_D0` im `yield()`-Fenster alle 64 Schritte (~53 ms bei 1200 Steps/s).
- Flag `_countIR` stellt sicher, dass nur während `dispense()` gezählt wird —
  kein Zählen beim Selbsttest oder Web-Testfahrt.
- Zähler werden in `dispense()` auf 0 zurückgesetzt, in `DS_STEPPER` aktiviert
  und nach `moveBlocking()` deaktiviert.

**Event-Log:**
- `FeedEvent` ersetzt `ir1Before/2Before/ir1After/ir2After` durch
  `ir1Pulses` / `ir2Pulses` (Flanken-Zähler während Stepper-Lauf).
- REST-API `/api/log`: Keys `i1p` / `i2p` statt bisheriger IR-Felder.
- Log-Tab: zeigt "IR1 Impulse" / "IR2 Impulse" pro Fütterung.

**Sensorübersicht:**
- IR1 / IR2 mit LED-Statusanzeige (grün = HIGH, gelb = LOW) direkt in der
  Sensoren-Karte (neben INA219, VL53, AS5600, DS3231).
- Neue CSS-Klasse `.led.y` (gelb/warning) für Zustandsanzeige.
- Neue JS-Funktion `irled()` für farbige IR-Statusaktualisierung via SSE.

Verifikation:

- `pio run` erfolgreich für `esp32dev`.

## 2026-05-07 - NVS-Speicherung auf Einzel-Keys umgestellt (1.1.1)

Scope:

- Firmware-Version auf `1.1.1` gesetzt.

Ursache:
- `save()` speicherte den gesamten Config-Struct als rohen Byte-Blob (`putBytes`).
- Jede Änderung am Struct (neue Felder, Änderung von MAX_SLOTS o.ä.) verschob
  die Byte-Offsets aller nachfolgenden Felder.
- Servo-Kalibrierung liegt nach dem Slots-Array → ging bei jeder Firmware-
  Änderung verloren, weil die Bytes auf falsche Felder gemappt wurden.
- Das bisherige Schema-Versions-Prüfung half nicht, da der Struct bereits
  falsch gelesen war, bevor die Migration griff.

Fix:
- `save()` und `load()` speichern/lesen jeden Wert unter einem eigenen
  NVS-Schlüssel (`putUChar`, `putUShort`, `putBool`, `putString` etc.).
- Struct-Layout-Änderungen sind damit vollständig irrelevant für die Persistenz.
- Neue Felder bekommen beim ersten Lesen automatisch ihren Default (da Key
  nicht vorhanden → Preferences gibt den angegebenen Default zurück).
- `doneToday` pro Slot wird jetzt ebenfalls persistent gespeichert.
- Alter Blob-Key `cfg` und `schema` werden beim ersten Start bereinigt.
- Schema-Versionierung ist nicht mehr nötig und entfällt aus der Ladelogik.

⚠️ Einmalige Konsequenz:
- Beim ersten Flash dieser Version gehen gespeicherte Kalibrierungswerte
  verloren (alter Blob ist nicht mehr lesbar). Danach bleiben alle Werte
  bei jedem OTA-Update dauerhaft erhalten.

Verifikation:

- `pio run` erfolgreich für `esp32dev`.

## 2026-05-07 - NTP-Sync, PWA-Icon, Default-Menge, Event-Log (1.1.0)

Scope:

- Firmware-Version auf `1.1.0` gesetzt, Config-Schema auf 6.

**NTP-Synchronisation:**
- `syncNTP()` in `main.cpp`: nach WiFi-Connect wird `configTime()` mit dem
  konfigurierten UTC-Offset + DST aufgerufen.
- Nach erfolgreicher NTP-Abfrage wird die DS3231-RTC automatisch gesetzt.
- Timeout 8 s; bei AP-Modus (kein WLAN) übersprungen.

**PWA / App-Icon:**
- `/icon.svg` Route: Pfotenabdruck-Icon (SVG, roter Hintergrund).
- `/manifest.json` Route: PWA-Manifest mit Name, Farben, Display-Modus.
- HTML-Head: `<link rel="icon">`, `<link rel="apple-touch-icon">`,
  `<link rel="manifest">`, `<meta name="apple-mobile-web-app-capable">`,
  `<meta name="theme-color">`.
- Anzeige als eigenständige App beim Hinzufügen zum Home-Screen (iOS/Android).

**Standard-Fütterungsmenge:**
- `Config::defaultGrams` (NVS-persistent, Default 20 g, 1–500 g).
- REST-API: Schlüssel `dfg` in GET/POST `/api/config`.
- Einstellungen-Tab: neues Feld "Standardmenge (g)".
- Status-Tab: Mengenfeld wird beim Laden der Config auf `defaultGrams` gesetzt.

**Event-Log:**
- `FeedEvent`-Struct: Zeitstempel, Auto/Manuell, Gramm, Servo,
  VL53L0X (mm + %) vor/nach, IR1/IR2 Analog vor/nach.
- `FeedLog`-Ringpuffer (20 Einträge, RAM, nicht persistent).
- `startFeed()` sichert Sensordaten vor Fütterung.
- `checkFeedComplete()` ergänzt Sensordaten nach Fütterung und fügt Eintrag ein.
- REST-API: GET `/api/log` liefert alle Einträge neueste zuerst.
- Neuer "Log"-Tab in der Web-UI: Karten-Layout mit Füllstand, VL53-mm, IR-Werten.

Verifikation:

- `pio run` erfolgreich für `esp32dev`.
- RAM: 16.2 %, Flash: 76.8 %.

## 2026-05-07 - Stepper-Klopfen durch Step-Bursts behoben (1.0.9)

Scope:

- Firmware-Version auf `1.0.9` gesetzt.
- `DS_STEPPER`-State der Fütterungs-State-Machine nutzt jetzt `moveBlocking()`
  statt `run()` + `loop()`.

Ursache:
- Der nicht-blockierende `run()` + `loop()`-Ansatz setzt voraus, dass
  `motors.loop()` regelmäßig aufgerufen wird.
- `sensors.update()` blockiert alle 500 ms für ~33–50 ms (VL53L0X `rangingTest()`
  im LONG_RANGE-Modus ist synchron).
- In dieser Zeit akkumulieren sich ~40 fällige Schritte (33 ms / 0,83 ms/Step
  bei 1200 Steps/s). Beim nächsten `loop()`-Aufruf feuern diese als Burst →
  Klopfen und unruhiger Lauf.

Lösung:
- `moveBlocking()` läuft in einer eigenen engen Timing-Schleife, vollständig
  unabhängig vom äußeren Loop-Takt. Kein Akkumulieren, kein Burst.
- `yield()` alle 64 Schritte hält den WiFi-Stack am Laufen.
- Alle anderen State-Machine-States (Servo-Wartezeiten) bleiben nicht-blockierend.
- Das ursprüngliche ruhige Stepper-Verhalten ist wiederhergestellt.

Verifikation:

- `pio run` erfolgreich für `esp32dev`.
- RAM: 15.9 %, Flash: 75.0 %.

## 2026-05-07 - Nicht-blockierende Fütterungs-State-Machine (1.0.8)

Scope:

- Firmware-Version auf `1.0.8` gesetzt.
- `Motors::dispense()` ist nicht mehr blockierend. Der gesamte Fütterungsablauf
  läuft jetzt als State Machine in `Motors::loop()` über folgende Zustände:
  `DS_SERVO_OPEN → DS_STEPPER → DS_STEPPER_WAIT → DS_SERVO_CLOSE
  → DS_SHAKE_OPEN → DS_SHAKE_CLOSE → DS_DONE → DS_IDLE`
- `motors.dispensing()` ersetzt den bisherigen `feedInProgress`-Flag in `main.cpp`.
- `startFeed()` löst die Fütterung aus und kehrt sofort zurück.
- `checkFeedComplete()` erkennt die Flanke `dispensing: true→false` und bucht
  den Feed-Counter sowie das Notify-Event.
- Scheduler und Web-Request-Handler prüfen `motors.dispensing()` statt einer
  lokalen Variable.
- `selfTest()` bleibt blockierend (läuft vor WLAN/Web-Start, kein Konflikt).
- `motors.stop()` setzt `_dispState` auf `DS_IDLE` zurück.
- Der ESP32 ist während einer Fütterung vollständig responsiv für Web-UI,
  OTA und Sensor-Updates.

Verifikation:

- `pio run` erfolgreich für `esp32dev`.
- RAM: 15.9 %, Flash: 75.0 %.

## 2026-05-07 - OTA-Geschwindigkeit verbessert

Scope:

- I2C-Takt von 100 kHz auf 400 kHz erhöht (4× schnellere Sensor-Reads).
- `otaActive`-Flag eingeführt: `loop()` bricht nach `ArduinoOTA.handle()`
  sofort ab, solange ein OTA-Transfer läuft. VL53L0X-Blocking (~33–50 ms
  alle 500 ms) blockiert damit den OTA-UDP-Handler nicht mehr.
- RTC-Uhrzeit (`hour`, `minute`) wird in `sensors.update()` gecacht.
  `sensors.hour()` und `sensors.minute()` lesen nur noch den Cache, kein
  I2C-Read pro Loop-Durchlauf mehr.

Ursachenanalyse:
- VL53L0X `rangingTest()` im LONG_RANGE-Modus: synchron-blocking ~33–50 ms,
  jede 500 ms → OTA-ACKs wurden verzögert.
- `schedulerLoop()` rief `sensors.hour()` / `sensors.minute()` auf jedem
  Loop-Durchlauf auf, je ~2 ms I2C @ 100 kHz → ~4 ms Blocking pro Tick.

Verifikation:

- `pio run` erfolgreich für `esp32dev` und `esp32dev_ota`.

## 2026-05-07 - Selbsttest, Fütterungsablauf und Blockierstrom-Konfiguration (1.0.7)

Scope:

- Firmware-Version auf `1.0.7` gesetzt.
- `Motors::selfTest()` eingeführt: Stepper 50 Schritte vor/zurück, danach
  Servo 1 und Servo 2 jeweils einmal auf/zu. Wird einmalig in `setup()` aufgerufen.
- Fütterungsablauf in `Motors::dispense()` präzisiert:
  1. Servos öffnen (600 ms)
  2. Stepper laufen (Gramm × Steps/g)
  3. Servos schließen
  4. 1 Sekunde warten
  5. Nachklappen: Servos einmal auf/zu zum Abklopfen von Futterresten
  6. Servos detachen
- `Config::stepperBlockMA` (Strom-Schwellwert für Stall-Erkennung) als neues
  Konfigurationsfeld eingeführt. Default: 1500 mA.
- Schema-Version auf 5 gesetzt; Migration setzt fehlende Felder auf Defaults.
- Web-UI: neues Eingabefeld „Blockierstrom (mA)" im Stepper-Abschnitt.
- REST-API: `sbm`-Schlüssel in GET/POST `/api/config` ergänzt.
- README und Architektur-Dokumentation um Selbsttest und Fütterungsablauf erweitert.

Hinweis:
- `stepperBlockMA` ist konfigurierbar und wird im NVS gespeichert.
  Die aktive Auswertung (Stall-Stop) folgt in einer späteren Iteration.

Verifikation:

- `pio run` erfolgreich für `esp32dev`.
- `pio run -e esp32dev_ota` erfolgreich.



## 2026-05-07 - Struktur bereinigt und Firmware-Orchestrierung hergestellt

Scope:

- Projekt in PlatformIO-konforme `src/`-Struktur gebracht.
- Bisheriges AS5600-Stepper-Kalibrierprogramm nach `tools/` verschoben.
- Neue Firmware-`main.cpp` fuer Webserver, OTA, Scheduler, Monitoring und
  Fuetterungskoordination angelegt.
- Sensor-Layer auf `Adafruit_AS5600` und `RTClib` angepasst.
- `RTClib` als Dependency ergaenzt.
- ArduinoJson-Deprecation-Warnungen in der Config-API bereinigt.
- README und Architektur-Dokumentation aktualisiert.

Verifikation:

- `pio run` erfolgreich.
- Speicherstand: RAM ca. 15.8 %, Flash ca. 31.0 %.

Naechste sinnvolle Schritte:

- Futterablauf in eine nicht-blockierende State Machine umbauen.
- Web UI fuer umfassendere Konfigurationen erweitern.
- OTA optional auch als Browser-Upload anbieten.
- Benachrichtigungskanal konkret festlegen und implementieren.
- Git initialisieren, initial commit erstellen und nach GitHub pushen.

## 2026-05-07 - Lokale WLAN-Defaults fuer OTA-Test vorbereitet

Scope:

- `upload_port` und `monitor_port` auf `/dev/cu.wchusbserial110` gesetzt.
- Optionalen lokalen Header `src/wifi_credentials.h` eingefuehrt.
- `src/wifi_credentials.h` wird von Git ignoriert, damit echte WLAN-Daten nicht
  im Repository landen.
- `src/wifi_credentials.example.h` als Vorlage angelegt.
- Config-Defaults uebernehmen lokale WLAN-Daten beim ersten Start.
- Wenn im NVS eine leere SSID liegt, werden die lokalen WLAN-Defaults nachtraeglich
  uebernommen und gespeichert.

Verifikation:

- `pio run` erfolgreich.
- USB-Upload versucht, aber `/dev/cu.wchusbserial110` war auf dem Mac nicht
  vorhanden. Sichtbare Ports: `/dev/cu.Bluetooth-Incoming-Port`,
  `/dev/cu.debug-console`.

## 2026-05-07 - OTA-Diagnose erweitert

Scope:

- Eigenes PlatformIO-Environment `esp32dev_ota` fuer OTA-Uploads angelegt.
- OTA-Port explizit auf `3232` gesetzt.
- OTA-Callbacks um Phase, Fortschritt und Fehlertext erweitert.
- `/api/status` um WLAN- und OTA-Diagnosefelder erweitert.
- `/api/diag` als lesbaren Diagnose-Endpunkt ergaenzt.
- SSE-Liveupdates transportieren ebenfalls IP, Hostname, WLAN-Modus und
  OTA-Status.

Neue Diagnosefelder:

- `ip`
- `hostname`
- `wifiMode`
- `rssi`
- `otaReady`
- `otaPort`
- `otaPhase`
- `lastOtaError`

Verifikation:

- `pio run` erfolgreich fuer `esp32dev` und `esp32dev_ota`.

## 2026-05-07 - OTA-faehige Partitionstabelle

Scope:

- Ursache fuer haengende OTA-Uploads eingegrenzt: `huge_app.csv` ist fuer
  ArduinoOTA ungeeignet, weil keine regulaeren OTA-App-Slots zur Verfuegung
  stehen.
- Partitionstabelle auf `default.csv` umgestellt.
- Testversion auf `1.0.1-ota-test` gesetzt, damit ein erfolgreicher OTA-Test
  eindeutig ueber `/api/diag` sichtbar ist.
- OTA-Environment meldet dem ESP die Host-IP des Macs explizit mit
  `-I 10.18.3.111`.
- `default_envs = esp32dev` gesetzt, damit ein normales `pio run` nur das
  USB-Environment baut.

Wichtig:

- Die geaenderte Partitionstabelle muss einmal per USB geflasht werden. Danach
  koennen regulaere Firmware-Updates per OTA getestet werden.

Verifikation:

- `pio run` erfolgreich mit `default.csv`.
- Firmwaregroesse: ca. 979 kB von 1.31 MB pro OTA-Slot.

## 2026-05-07 - OTA validiert

Scope:

- OTA nach USB-Flash der OTA-faehigen Partitionstabelle erfolgreich getestet.
- Testversion `1.0.1-ota-test` wieder auf saubere Firmware-Version `1.0.1`
  gesetzt.
- Feste Mac-Host-IP aus dem OTA-Environment entfernt, damit das Projekt nicht
  an einen konkreten Entwicklungsrechner gebunden ist.

Verifikation:

- `/api/diag` zeigte `otaReady: true` und `fw: 1.0.1-ota-test` nach USB-Flash.
- OTA-Upload ueber `esp32dev_ota` lief erfolgreich durch.

## 2026-05-07 - Hardware-Pins und Servo-Kalibrierung bereinigt

Scope:

- Nicht verbaute Endschalter aus aktiver Firmware, Statusdaten, SSE und WebUI
  entfernt.
- Pinmap auf aktuelle Hardware angepasst:
  - VL53L0X XSHUT: GPIO 16
  - IR1 D0/A0: GPIO 39 / GPIO 36
  - IR2 D0/A0: GPIO 35 / GPIO 34
- Servo-Geschwindigkeit als `servoSpeedDPS` in die Konfiguration aufgenommen.
- WebUI-Kalibrierung erweitert:
  - Servo 1/2 Winkel per Slider testen
  - aktuelle Sliderposition als Offen/Zu speichern
  - gespeicherte Offen/Zu-Endlagen fahren
  - Servo-Geschwindigkeit in Grad/s speichern
- Servo-Open/Close-Fahrten nutzen jetzt weiche Positionsschritte anhand der
  konfigurierten Geschwindigkeit.
- Firmware-Version auf `1.0.2` gesetzt.

Verifikation:

- `pio run` erfolgreich fuer `esp32dev`.
- `pio run -e esp32dev_ota` erfolgreich.

## 2026-05-07 - Stepper-Ansteuerung mit Testbaustelle abgeglichen

Scope:

- Stepper-Testlogik aus `CatFeeder_Testbaustelle` mit der aktuellen Firmware
  verglichen.
- Relevante Unterschiede:
  - Testbaustelle faehrt den Stepper blockierend mit festen STEP-Abstaenden.
  - Aktuelle Firmware fuhr Web-Testfahrten kooperativ im Mainloop und konnte
    bei Sensor-/Weblast Step-Pulse nachholen, was unruhige Abstaende erzeugt.
  - Testbaustelle nutzte `10 us` STEP-Puls und `300 us` DIR-Setup; aktuelle
    Firmware nutzte `5 us` Puls und kein DIR-Setup vor dem ersten STEP.
- STEP-Puls-Default auf `10 us` und DIR-Setup-Default auf `300 us` gesetzt.
- Stepper-Testfahrten und Fuetterfahrt laufen jetzt deterministisch blockierend,
  angelehnt an die Testbaustellen-Funktion.
- Haltestrom nach Stepperlauf konfigurierbar gemacht; Default `0 ms`, damit der
  Treiber nach der Bewegung sofort geloest wird.
- WebUI um DIR-Setup und Haltestrom erweitert.
- Firmware-Version auf `1.0.6` gesetzt.

Verifikation:

- `pio run -e esp32dev` erfolgreich.
- `pio run -e esp32dev_ota` erfolgreich.

## 2026-05-07 - Kalibrierwerte und Geschwindigkeit nachgeschärft

Scope:

- Firmware-Version auf `1.0.3` gesetzt.
- Config-Schema-Version separat im NVS gespeichert, ohne die `Config`-Struktur
  unnoetig aufzublaehen.
- Servo-Defaultgeschwindigkeit auf `1000 Grad/s` gesetzt.
- Servo-Fahrten ab `1000 Grad/s` springen direkt auf die Zielposition, darunter
  wird weich gerampt.
- Servo-Geschwindigkeit in der WebUI auf `20..3000 Grad/s` erweitert.
- Stepper-Defaultgeschwindigkeit auf `1200 Steps/s` gesetzt.
- Stepper-Geschwindigkeit in der WebUI auf `100..10000 Steps/s` erweitert.
- Stepper-Test speichert die aktuellen UI-Kalibrierwerte vor der Testfahrt,
  damit eine geaenderte Geschwindigkeit sofort wirkt.

Persistenz:

- WebUI-Konfiguration wird im ESP32-NVS gespeichert und bleibt bei normalen
  OTA-Firmwareupdates erhalten.
- Nur groessere Struktur-/Schemaaenderungen oder ein Werksreset koennen Defaults
  neu setzen.

Verifikation:

- `pio run` erfolgreich fuer `esp32dev`.
- `pio run -e esp32dev_ota` erfolgreich.

## 2026-05-07 - Stepper-Kalibrierung erweitert

Scope:

- Firmware-Version auf `1.0.4` gesetzt.
- Stepper-Parameter in der WebUI erweitert:
  - Test-Steps
  - Grundgeschwindigkeit in Steps/s
  - STEP-Pulsbreite in us
  - Richtungsinvertierung
  - Steps pro Gramm
- Firmware nutzt die gespeicherten Stepper-Parameter fuer Testfahrten und
  Fuetterungsfahrten.
- Header der WebUI zeigt jetzt die laufende Firmware-Version oben rechts an.
- Config-Ladung toleriert kleinere gespeicherte Config-Bloecke und ergaenzt
  neue Felder mit Defaults, damit OTA-Updates weniger Kalibrierwerte verlieren.

Verifikation:

- `pio run` erfolgreich fuer `esp32dev`.
- `pio run -e esp32dev_ota` erfolgreich.

## 2026-05-07 - Stepper-Speed-Limit im Mainloop behoben

Scope:

- Ursache fuer scheinbar wirkungslose Stepper-Geschwindigkeit gefunden:
  `delay(5)` im Hauptloop begrenzte Web-Testfahrten effektiv auf ca. 200
  Motorloop-Aufrufe pro Sekunde.
- `delay(5)` durch `yield()` ersetzt.
- `Motors::loop()` arbeitet jetzt bis zu 16 faellige Step-Pulse pro Aufruf ab,
  damit eingestellte Steps/s auch bei Webserver-/Sensorlast besser erreicht
  werden.
- Firmware-Version auf `1.0.5` gesetzt.

Verifikation:

- `pio run` erfolgreich fuer `esp32dev`.
- `pio run -e esp32dev_ota` erfolgreich.
