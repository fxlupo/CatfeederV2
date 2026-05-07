# Architektur

## Firmware-Module

- `main.cpp`: Initialisiert Konfiguration, Sensoren, Motoren, Webserver und OTA.
  Fuehrt ausserdem Web-Fuetterungsanforderungen, Scheduler und Monitoring aus.
- `config.*`: Persistente Konfiguration im ESP32-NVS ueber `Preferences`.
- `sensors.*`: Gekapselter Zugriff auf INA219, VL53L0X, AS5600, DS3231 und
  IR-Sensoren.
- `motors.*`: Stepper-Laufsteuerung, Servo-Positionen und blockierender
  Fuetterungsablauf.
- `web.*`: WLAN/AP-Fallback, mDNS, REST-API, SSE-Liveupdates.
- `web_html.cpp`: Eingebettete lokale Weboberflaeche.

## Laufzeitfluss

1. `setup()` laedt Konfiguration und Feed-Counter aus NVS.
2. Sensoren und Motoren werden initialisiert.
3. Webserver startet im WLAN-Client-Modus oder als Setup-Access-Point.
4. ArduinoOTA wird mit dem konfigurierten Hostnamen aktiviert.
5. `loop()` bedient OTA, Motor-Stepper, Sensoraktualisierung, Web/SSE,
   Web-Fuetterungsrequests und den Scheduler.

## Scheduler

Der Scheduler nutzt die DS3231-Uhr. Pro Minute wird geprueft, ob ein aktiver
Slot faellig ist. Faellige Slots werden als `doneToday` markiert und gespeichert,
damit eine Fuetterung nicht mehrfach in derselben Minute wiederholt wird.

Um Mitternacht werden die Tagesflags zurueckgesetzt.

## OTA

OTA ist ueber `ArduinoOTA` vorbereitet. Beim Start eines OTA-Vorgangs werden
Stepper und Servos gestoppt bzw. geloest. Uploads koennen nach WLAN-Setup ueber
den Hostnamen erfolgen, z. B. `catfeeder.local`.

## Monitoring und Benachrichtigung

Das Monitoring ist aktuell lokal vorbereitet:

- `/api/status` liefert Snapshotdaten.
- `/events` liefert SSE-Liveupdates.
- `main.cpp` enthaelt `notifyEvent()` als zentralen Hook fuer Ereignisse.

Aktuelle Ereignisse:

- Boot abgeschlossen
- OTA Start/Ende/Fehler
- Fuetterung abgeschlossen
- Ueberstrom erkannt
- niedriger Fuellstand erkannt

Spaetere Benachrichtigungskanaele koennen an `notifyEvent()` angebunden werden,
z. B. MQTT, Telegram, Webhook oder Push-Service.

## Bekannte technische Punkte

- `Motors::dispense()` ist noch blockierend. Das ist fuer den ersten sauberen
  Stand akzeptabel, sollte spaeter in eine nicht-blockierende State Machine
  ueberfuehrt werden.
- Das Webinterface ist eingebettet und wird aus Flash/PROGMEM ausgeliefert.
- Das Stepper-AS5600-Kalibrierprogramm ist unter `tools/` gesichert und wird
  nicht automatisch gebaut.
