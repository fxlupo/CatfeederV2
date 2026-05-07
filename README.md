# CatFeeder ESP32 Firmware

Zeitgesteuerter Katzenfutter-Automat mit lokaler Weboberflaeche, OTA,
Scheduler, Sensor-Monitoring und vorbereiteten Benachrichtigungs-Hooks.

## Status

- Zielplattform: ESP32
- Buildsystem: PlatformIO / Arduino Framework
- Firmware-Version: `1.0.0`
- Web UI: lokal ueber HTTP, im Setup-Fall als Access Point
- OTA: ArduinoOTA ueber WLAN/mDNS vorbereitet
- Scheduler: bis zu 8 taegliche Fuetterungszeiten
- Monitoring: Sensorstatus, Strom, Fuellstand, Endschalter, IR, Uptime, Heap

## Projektstruktur

```text
catfeeder/
├── platformio.ini
├── README.md
├── docs/
│   ├── architecture.md
│   └── iterations.md
├── src/
│   ├── main.cpp          # App-Orchestrierung, OTA, Scheduler, Monitoring
│   ├── pins.h            # GPIO-Definitionen
│   ├── config.h/.cpp     # Konfiguration, NVS, Defaultwerte
│   ├── sensors.h/.cpp    # INA219, VL53L0X, AS5600, DS3231, IR, Endschalter
│   ├── motors.h/.cpp     # Stepper + Servos + Fuetterungsablauf
│   ├── web.h/.cpp        # WLAN, Webserver, REST-API, SSE
│   └── web_html.cpp      # Eingebettetes Webinterface
└── tools/
    └── stepper_as5600_calibration.cpp
```

Das Kalibrierprogramm liegt bewusst unter `tools/`, damit PlatformIO die echte
Firmware aus `src/` baut.

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
- Zeiten: bis zu 8 taegliche Fuetterungszeiten
- Kalibrierung: Servo-Winkel, Stepper-Test, Steps pro Gramm, Fuellstandsgrenzen
- Einstellungen: WLAN, RTC-Sync, Zeitzone, Hostname, Werksreset

## REST API

| Methode | Endpunkt | Zweck |
| --- | --- | --- |
| `GET` | `/` | Webinterface |
| `GET` | `/api/status` | Status als JSON |
| `GET` | `/api/config` | Konfiguration lesen |
| `POST` | `/api/config` | Konfiguration speichern |
| `POST` | `/api/feed` | Sofort-Fuetterung anfordern |
| `POST` | `/api/sv` | Servo-Testwinkel setzen |
| `POST` | `/api/stp` | Stepper-Test starten |
| `POST` | `/api/time` | RTC synchronisieren |
| `POST` | `/api/wifi` | WLAN speichern und neu starten |
| `POST` | `/api/reset` | Werkseinstellungen |
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
| 32 | S1_OPEN | INPUT_PULLUP, LOW = ausgelöst |
| 33 | S1_CLOSE | INPUT_PULLUP, LOW = ausgelöst |
| 13 | S2_OPEN | INPUT_PULLUP, LOW = ausgelöst |
| 14 | S2_CLOSE | INPUT_PULLUP, LOW = ausgelöst |
| 34 | IR1_A0 | Analog, input-only |
| 35 | IR2_A0 | Analog, input-only |
| 2 | IR1_D0 | Digital |
| 4 | IR2_D0 | Digital |
| 5 | VL53_XSHUT | LOW = Standby |

I2C-Geraete:

- INA219 `0x40`
- AS5600 `0x36`
- DS3231 `0x68`
- VL53L0X `0x29`

## Versionierung

Jede fachliche Iteration soll separat committed werden. Laufende Notizen stehen
in `docs/iterations.md`.
