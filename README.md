# CatFeeder ESP32 Firmware

Zeitgesteuerter Katzenfutter-Automat mit lokaler Weboberflaeche, OTA,
Scheduler, Sensor-Monitoring und Blockade-Benachrichtigung.

## Status

- Zielplattform: ESP32
- Buildsystem: PlatformIO / Arduino Framework
- Firmware-Version: `1.3.0`
- Web UI: lokal ueber HTTP, im Setup-Fall als Access Point
- OTA: ArduinoOTA ueber WLAN/mDNS vorbereitet
- Scheduler: taegliche Fuetterungszeiten, Anzahl per Firmware-Konstante
  `MAX_SLOTS` konfigurierbar
- Monitoring: Sensorstatus, Strom, Fuellstand, IR, Uptime, Heap
- Blockadeerkennung: AS5600-Rotation plus INA219-Strom, mit Rueckwaertsfahrt
  und Wiederholversuchen
- Benachrichtigung: WhatsApp via CallMeBot bei abgebrochener Fuetterung
- Remote-Plattform: MQTT-Grundlage fuer externes Backend in Arbeit

## Projektstruktur

```text
catfeeder/
├── platformio.ini
├── README.md
├── docs/
│   ├── architecture.md
│   ├── iterations.md
│   └── remote-platform-plan.md
├── src/
│   ├── main.cpp          # App-Orchestrierung, OTA, Scheduler, Monitoring
│   ├── pins.h            # GPIO-Definitionen
│   ├── config.h/.cpp     # Konfiguration, NVS, Defaultwerte
│   ├── sensors.h/.cpp    # INA219, VL53L0X, AS5600, DS3231, IR
│   ├── motors.h/.cpp     # Stepper + Servos + Fuetterungsablauf
│   ├── web.h/.cpp        # WLAN, Webserver, REST-API, SSE
│   └── web_html.cpp      # Eingebettetes Webinterface
└── tools/
    └── stepper_as5600_calibration.cpp
```

Das Kalibrierprogramm liegt bewusst unter `tools/`, damit PlatformIO die echte
Firmware aus `src/` baut.

Der Remote-Plattform-Plan fuer MQTT, Backend, React UI und Push-Service wird in
`docs/remote-platform-plan.md` als lebendes Arbeitsdokument gepflegt.

## Build

```sh
pio run
```

Auf dem aktuellen Volume kann PlatformIO Schreibrechte fuer `.pio` brauchen. In
diesem Fall den Build aus einer Shell mit passenden Rechten starten.

## Upload

```sh
pio run --target upload
```

Nach dem ersten WLAN-Setup kann OTA genutzt werden:

```sh
pio run --target upload --upload-port catfeeder.local
```

## Erststart

1. Firmware flashen.
2. ESP32 startet bei fehlender WLAN-Konfiguration den AP `CatFeeder-Setup`.
3. Passwort: `katze1234`
4. Browser oeffnen: `http://192.168.4.1`
5. WLAN konfigurieren und Neustart abwarten.
6. Danach lokal erreichbar unter `http://catfeeder.local`.

## Web UI

Tabs:

- Status: Live-Dashboard, Sofort-Fuettern, Sensoren, Fuellstand, Strom, System
- Zeiten: taegliche Fuetterungszeiten, Menge und Servo-Auswahl
- Kalibrierung: Servo-Winkel, Servo-Geschwindigkeit, Stepper-Test,
  Steps pro Gramm, Blockadeerkennung, Fuellstandsgrenzen
- Einstellungen: Standardmenge, WLAN, RTC/NTP-Zeit, Zeitzone, WhatsApp,
  Hostname, Werksreset
- Log: letzte Fuetterungen mit Menge, Fuellstand, IR-Impulsen und Blockade-Status

Die Anzahl der Zeitplaetze ist kein festes Produktlimit der UI, sondern wird in
der Firmware ueber `MAX_SLOTS` bestimmt. Wird dieser Wert in `src/config.h`
angepasst, liefert `/api/config` entsprechend mehr oder weniger Slots aus.

## Selbsttest beim Start

Beim Einschalten führt die Firmware automatisch einen Selbsttest durch:

1. **Stepper** – 50 Schritte vorwärts, dann 50 Schritte zurück (prüft Treiber und Verkabelung)
2. **Servo 1** – Offen-Position anfahren, dann Zu-Position
3. **Servo 2** – Offen-Position anfahren, dann Zu-Position

Der Test nutzt die gespeicherten Kalibrierwerte (Winkel, Geschwindigkeit, Schritte).
Schlägt ein Servo oder der Stepper nicht an, ist dies am seriellen Monitor erkennbar.

## Fütterungsablauf

Jede Fütterung (manuell oder per Zeitplan) folgt diesem Ablauf:

1. Klappe(n) öffnen – Servos fahren auf die konfigurierte Offen-Position
2. Stepper läuft – fördert die eingestellte Menge (Gramm × Steps/g)
3. Klappe(n) schließen – Servos fahren auf die Zu-Position
4. 1 Sekunde warten – Futter setzt sich
5. Nachklappen – Servos einmal kurz auf/zu zum Abklopfen von Futterresten
6. Servos werden abgeschaltet (kein Haltestrom)

Während der Stepperphase wird die Bewegung blockierend mit gleichmäßigen
STEP-Pulsen gefahren. Das ist bewusst so gehalten, weil der verbaute
TMC2208/NEMA17 damit deutlich ruhiger läuft als mit Mainloop-getakteten Pulsen.
Parallel werden IR-Flanken gezählt und, bei Fütterungen, AS5600/INA219 für die
Blockadeerkennung ausgewertet.

## Blockadeerkennung

Eine Blockade wird während der Fütter-Stepperphase erkannt. Die Prüfung kombiniert:

- AS5600: zu wenig Rotation pro Messfenster
- INA219: Strom oberhalb der konfigurierten Schwelle

Die relevanten Werte sind in der Web-UI unter Kalibrierung einstellbar:

- Strom-Schwelle `stepperBlockMA`
- minimale Rotation `blockMinRotPct`
- Rueckwaerts-Schritte `blockReverseSteps`
- maximale Wiederholversuche `blockRetries`

Bei erkannter Blockade stoppt die Fütterung, fährt rueckwaerts, wartet kurz und
versucht erneut. Nach zu vielen Fehlversuchen wird die Fütterung abgebrochen,
im Log markiert und optional per WhatsApp gemeldet.

## REST API

| Methode | Endpunkt | Zweck |
| --- | --- | --- |
| `GET` | `/` | Webinterface |
| `GET` | `/api/status` | Status als JSON |
| `GET` | `/api/diag` | OTA/WLAN/Systemdiagnose |
| `GET` | `/api/config` | Konfiguration lesen |
| `POST` | `/api/config` | Konfiguration speichern |
| `POST` | `/api/feed` | Sofort-Fuetterung anfordern |
| `POST` | `/api/sv` | Servo-Testwinkel setzen oder gespeicherte Endlage fahren |
| `POST` | `/api/stp` | Stepper-Test starten |
| `POST` | `/api/time` | RTC synchronisieren |
| `POST` | `/api/wifi` | WLAN speichern und neu starten |
| `POST` | `/api/reset` | Werkseinstellungen |
| `GET` | `/api/log` | Fuetterungslog aus dem RAM |
| `SSE` | `/events` | Live-Updates im 2-Sekunden-Takt |

## Pin-Belegung

| GPIO | Funktion | Bemerkung |
| --- | --- | --- |
| 21 | SDA | I2C Bus |
| 22 | SCL | I2C Bus |
| 25 | STEP | Stepper-Treiber |
| 26 | DIR | Stepper-Treiber |
| 27 | EN_DRV | LOW = aktiv |
| 18 | SERVO1 | PWM |
| 19 | SERVO2 | PWM |
| 39 | IR1_D0 | Digital, input-only |
| 36 | IR1_A0 | Analog ADC1, input-only |
| 35 | IR2_D0 | Digital, input-only |
| 34 | IR2_A0 | Analog ADC1, input-only |
| 16 | VL53_XSHUT | LOW = Standby |

I2C-Geraete:

- INA219 `0x40`
- AS5600 `0x36`
- DS3231 `0x68`
- VL53L0X `0x29`

## Hardware-Einschränkungen

### Custom-PCB — keine Pin-Änderungen möglich

Die Firmware läuft auf einer **individuell angefertigten Platine** mit fest
verdrahteten GPIO-Belegungen. Alle Pin-Zuweisungen in `pins.h` sind durch das
PCB-Layout fixiert und können nicht ohne neue Platinenrevision geändert werden.

Konsequenz für Board-Upgrades: ESP32-Varianten mit mehr Flash oder RAM, die
intern andere GPIOs belegen (z.B. ESP32-WROVER-E belegt GPIO 16/17 für PSRAM),
sind **kein Drop-in-Ersatz** — GPIO 16 wird für `VL53_XSHUT` benötigt.
Ein Board-Wechsel würde ein neues PCB-Layout erfordern.

### Flash-Speicher — Headroom beachten

Der Flash-Speicher des verbauten ESP32 beträgt 4 MB. Mit der aktuellen Firmware
inkl. CallMeBot-Library (HTTPClient + WiFiClientSecure) ergibt sich:

| | Größe |
| --- | --- |
| OTA-Slot gesamt | 1.280 KB |
| Firmware aktuell | ~1.165 KB (~89 %) |
| Freier Headroom | ~115 KB |

**Entwicklungsregeln für neue Features:**

- Keine weiteren großen Libraries mehr hinzufügen (HTTPClient hat alleine ~150 KB gekostet)
- Bei kritischem Headroom: LTO (Link-Time-Optimization) aktivieren — spart
  typisch 10–15 % ohne Funktionsverlust (`build_flags = -Os -flto`)
- `web_html.cpp` ist der größte Einzelbrocken — bei Bedarf kann HTML/JS
  komprimiert (gzip) und per `Content-Encoding` ausgeliefert werden

## Versionierung

Jede fachliche Iteration soll separat committed werden. Laufende Notizen stehen
in `docs/iterations.md`.
