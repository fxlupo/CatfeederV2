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
