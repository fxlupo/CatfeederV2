# Architektur

## Firmware-Module

- `main.cpp`: App-Orchestrierung. Initialisiert Konfiguration, Sensoren,
  Motoren, Webserver, NTP und OTA. Fuehrt ausserdem Scheduler, Monitoring,
  Web-Fuetterungsrequests, Feed-Logging und Benachrichtigung aus.
- `config.*`: Persistente Konfiguration im ESP32-NVS ueber `Preferences`.
  Felder werden einzeln gespeichert, damit OTA-Updates keine Kalibrierwerte
  durch Struct-Layout-Aenderungen verlieren.
- `sensors.*`: Gekapselter Zugriff auf INA219, VL53L0X, AS5600, DS3231 und
  IR-Sensoren. Normale Statuswerte werden gecacht, Blockadeerkennung kann
  INA219/AS5600 frisch lesen.
- `motors.*`: Stepper, Servos, Selbsttest und Fütterungs-State-Machine.
  Die Stepperphase nutzt bewusst eine enge blockierende Pulse-Schleife fuer
  ruhigen Motorlauf.
- `web.*`: WLAN/AP-Fallback, mDNS, REST-API und SSE-Liveupdates.
- `web_html.cpp`: Eingebettete lokale Weboberflaeche.
- `tools/stepper_as5600_calibration.cpp`: Separates Kalibrier-/Testprogramm,
  wird von PlatformIO nicht als Firmware gebaut.

## Laufzeitfluss

1. `setup()` startet Serial, setzt OTA-Status und laedt Konfiguration aus NVS.
2. Sensoren und Motoren werden initialisiert.
3. `Motors::selfTest()` prueft Stepper und beide Servos mit gespeicherten
   Kalibrierwerten.
4. Statusdaten werden einmal gelesen.
5. Webserver startet im WLAN-Client-Modus oder als Setup-Access-Point.
6. Bei WLAN-Betrieb synchronisiert `syncNTP()` die DS3231-Uhr.
7. ArduinoOTA wird mit dem konfigurierten Hostnamen aktiviert.
8. `loop()` bedient OTA, Motor-State-Machine, Sensorstatus, Web/SSE,
   Web-Fuetterungsrequests, Feed-Abschlusslogik und Scheduler.

Während eines aktiven OTA-Transfers kehrt `loop()` nach `ArduinoOTA.handle()`
frueh zurueck. Sensoren, Web-SSE und Scheduler werden dann pausiert, damit der
OTA-UDP-Datenstrom nicht durch blockierende I2C-Lesezyklen gestoert wird.

## Konfiguration

Die Konfiguration liegt im NVS-Namespace `cf`. Aktuelle Werte werden nicht mehr
als kompletter Struct-Blob gespeichert, sondern unter einzelnen Keys:

- WLAN und Hostname
- Scheduler-Slots
- Servo-Endlagen und Servo-Geschwindigkeit
- Stepper-Geschwindigkeit, Pulsbreite, DIR-Setup, Haltestrom und Richtung
- Blockadeerkennung
- Füllstands- und IR-Grenzen
- WhatsApp-Empfaenger
- Standardmenge fuer manuelles Fuettern

Neue Felder bekommen beim ersten Lesen ihren Default. Bestehende Werte bleiben
bei OTA-Updates erhalten, solange ihre NVS-Keys weiter verwendet werden.

## Scheduler

Der Scheduler nutzt die DS3231-Uhr. Pro Minute wird geprueft, ob ein aktiver
Slot faellig ist. Faellige Slots werden als `doneToday` markiert und im NVS
gespeichert, damit dieselbe Fuetterung nicht mehrfach in einer Minute startet.

Um Mitternacht werden die Tagesflags zurueckgesetzt.

Die Anzahl der Slots ist ueber `MAX_SLOTS` in `src/config.h` konfigurierbar.
Die Web-UI erzeugt die Zeilen dynamisch aus `/api/config` und ist nicht auf eine
hart codierte Slot-Zahl festgelegt.

## OTA

OTA laeuft ueber `ArduinoOTA` auf Port `3232`. Beim Start eines OTA-Vorgangs:

- wird `otaActive` gesetzt,
- Stepper und Servos werden gestoppt bzw. geloest,
- offene SSE-Verbindungen werden geschlossen,
- die Mainloop pausiert alle nicht notwendigen Arbeiten.

Bei OTA-Ende oder OTA-Fehler wird `otaActive` wieder zurueckgesetzt. Fehlerphase
und Fehlergrund sind ueber `/api/diag`, `/api/status` und SSE sichtbar.

Der interne ArduinoOTA-Receive-Timeout ist auf 15 Sekunden erhoeht. Das ist
wichtig, weil das Firmwareimage durch Web-UI und Benachrichtigung knapp 1.17 MB
gross ist und einzelne WLAN-Haenger sonst zum Abbruch fuehren koennen. Das
OTA-Progress-Logging auf Serial ist auf 10-Prozent-Schritte begrenzt.

## Web und API

Die Weboberflaeche wird direkt aus `web_html.cpp` ausgeliefert. Livewerte kommen
per SSE im 2-Sekunden-Takt.

Wichtige API-Endpunkte:

- `GET /api/status`: kompakter System- und Sensorstatus
- `GET /api/diag`: OTA/WLAN/Systemdiagnose
- `GET /api/config`: komplette UI-Konfiguration
- `POST /api/config`: Konfiguration speichern
- `POST /api/feed`: manuelle Fuetterung anfordern
- `POST /api/sv`: Servo testen oder gespeicherte Endlage fahren
- `POST /api/stp`: Stepper-Testfahrt
- `POST /api/time`: RTC setzen
- `POST /api/wifi`: WLAN speichern und neu starten
- `GET /api/log`: RAM-Ringpuffer der letzten Fuetterungen

## Fütterungsablauf

`Motors::dispense(grams, servo, cfg)` startet eine State-Machine. Sie laeuft
ueber `Motors::loop()` und blockiert den Hauptloop nur in der eigentlichen
Stepperphase.

Ablauf:

1. Servos öffnen.
2. 600 ms warten.
3. Stepper blockierend mit gleichmaessigen STEP-Pulsen fahren.
4. 400 ms warten.
5. Beide Servos schließen.
6. 1000 ms warten.
7. Gewaehlte Servos kurz öffnen.
8. 500 ms warten.
9. Beide Servos final schließen.
10. 1000 ms warten.
11. Servos detachen, State zurueck auf `DS_IDLE`.

Die Stepperphase ist absichtlich blockierend. Der getestete TMC2208/NEMA17 laeuft
damit ruhig und reproduzierbar, waehrend Mainloop-getaktete Pulse durch Sensor-
und Webserver-Jitter hoerbar unruhig wurden.

## Blockadeerkennung

Blockadeerkennung ist nur waehrend der Fuetter-Stepperphase aktiv, nicht beim
Selbsttest und nicht bei manuellen Stepper-Testfahrten.

`moveBlocking()` liest alle 64 Steps:

- IR-Digitalflanken fuer das Fuetterungslog
- INA219-Strom ueber `Sensors::readInstant()`
- AS5600-Winkel ueber `Sensors::readInstant()`

Die Erkennung nutzt aktuell OR-Logik:

- zu wenig AS5600-Rotation im Messfenster oder
- Strom oberhalb `stepperBlockMA`

Nach zwei aufeinanderfolgenden auffaelligen Messfenstern gilt die Bewegung als
blockiert. Die Fütter-State-Machine reagiert dann so:

1. Stepperfahrt abbrechen.
2. `blockReverseSteps` rueckwaerts fahren.
3. 300 ms warten.
4. Bis zu `blockRetries` Wiederholversuche.
5. Danach beide Servos final schließen.
6. Fütterung abbrechen und im Log markieren.

Zur Kalibrierung schreibt die Firmware nach aktiver Blockadeerkennung Peak-Strom
und minimale gemessene Rotation auf Serial.

## Feed-Logging

`FeedLog` ist ein RAM-Ringpuffer. Er speichert die letzten `FEED_LOG_SIZE`
Fuetterungen bis zum naechsten Neustart.

Pro Eintrag:

- Zeitstempel
- manuell oder Scheduler
- Menge und Servo-Auswahl
- VL53L0X-Distanz und berechneter Füllstand vor/nach der Fütterung
- IR1/IR2-Flanken waehrend der Stepperphase
- Blockade-Abbruch und Retry-Anzahl

## Monitoring und Benachrichtigung

Lokales Monitoring:

- `/api/status`
- `/api/diag`
- `/events`
- `/api/log`
- Web-UI-Statuskarten

Ereignisse werden zentral ueber `notifyEvent()` seriell protokolliert.

WhatsApp-Benachrichtigungen laufen ueber CallMeBot. Gesendet wird aktuell bei
abgebrochener Fuetterung durch Blockade, an alle in der Web-UI aktivierten
Empfaenger mit Telefonnummer und API-Key.

Der CallMeBot-Aufruf laeuft in einem eigenen FreeRTOS-Task auf Core 0, damit der
blockierende HTTPS-Aufruf den Hauptloop, OTA und Webserver nicht einfriert.

## Hardware- und Speichergrenzen

Die GPIO-Belegung ist durch das Custom-PCB fix. Pin-Aenderungen sind nicht ohne
neue Platinenrevision moeglich.

Der verbaute ESP32 hat 4 MB Flash mit OTA-Partitionen. Durch CallMeBot /
HTTPClient / WiFiClientSecure ist der Flash-Headroom knapp. Neue grosse
Libraries sollten nur nach vorherigem Groessencheck hinzugefuegt werden.

## Bekannte technische Punkte

- Stepper-Testfahrten und Fütter-Stepperphase blockieren bewusst fuer saubere
  Pulse. Fuer spaetere lange Fahrten waere Timer/RMT plus Beschleunigungsrampe
  die robustere Architektur.
- Das Webinterface ist eingebettet und wird aus Flash/PROGMEM ausgeliefert.
  Bei Flash-Druck kann HTML/JS gzip-komprimiert werden.
- `FeedLog` ist nicht persistent und geht bei Neustart verloren.
