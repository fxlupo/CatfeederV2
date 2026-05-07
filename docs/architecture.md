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

## Selbsttest

Beim Start ruft `setup()` `Motors::selfTest()` auf:

1. Stepper 50 Schritte vor, 200 ms Pause, 50 Schritte zurück
2. Servo 1 öffnen → Servo 1 schließen
3. Servo 2 öffnen → Servo 2 schließen

Der Test nutzt die gespeicherten Kalibrierwerte und gibt Ergebnis auf Serial aus.

## Fütterungsablauf

`Motors::dispense(grams, servo, cfg)` führt folgende Schritte aus:

1. Servos öffnen (600 ms Wartezeit)
2. Stepper blockierend laufen lassen (`grams × stepsPerGram` Schritte)
3. 400 ms Pause
4. Servos schließen
5. 1000 ms Pause (Futter setzt sich)
6. Servos erneut öffnen (500 ms)
7. Servos schließen (400 ms) – Nachklappen zum Abklopfen von Futterresten
8. Servos detachen (kein Haltestrom)

## Blockierschutz

`Config::stepperBlockMA` speichert den Strom-Schwellwert (mA), ab dem der Stepper als
blockiert gilt. Der Wert ist über die Web-UI und die REST-API (`sbm`) konfigurierbar.
Die aktive Auswertung erfolgt in der späteren nicht-blockierenden State-Machine;
der Konfigwert ist bereits vorhanden und wird im NVS gespeichert.

## Bekannte technische Punkte

- `Motors::dispense()` ist noch blockierend. Das ist fuer den ersten sauberen
  Stand akzeptabel, sollte spaeter in eine nicht-blockierende State Machine
  ueberfuehrt werden.
- Das Webinterface ist eingebettet und wird aus Flash/PROGMEM ausgeliefert.
- Das Stepper-AS5600-Kalibrierprogramm ist unter `tools/` gesichert und wird
  nicht automatisch gebaut.
