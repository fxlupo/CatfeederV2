# Iterationen

## 2026-05-07 - Selbsttest, Fütterungsablauf und Blockierstrom-Konfiguration

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
  Die aktive Auswertung (Stall-Stop) folgt mit der nicht-blockierenden State-Machine.

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
