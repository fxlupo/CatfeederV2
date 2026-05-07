# Iterationen

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
