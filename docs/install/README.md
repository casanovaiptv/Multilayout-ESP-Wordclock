# Webinstaller – Entwicklungsstand

Diese Seite ist eine **noch nicht flashfähige** Oberfläche für die Multilayout-ESP-Wordclock.

## Geplante Unterstützung

- ESP32 (`env:ESP32`)
- ESP32-C3 (`env:ESP32C3`)
- ESP8266 (`env:ESP8266`), nach separater Kompatibilitätsprüfung

## Nächste Schritte

1. `platformio.ini` und `erase_before_upload.py` so prüfen bzw. ändern, dass normale Firmware-Updates **kein vollständiges Flash-Erase** auslösen.
2. Alle drei PlatformIO-Umgebungen bauen und die tatsächlichen Binärdateien sowie Flash-Offsets ermitteln.
3. Für ESP32 und ESP32-C3 passende ESP Web Tools-Manifeste mit realen, veröffentlichten Binärdateien erzeugen.
4. Browserinstallation und Erhalt gespeicherter Einstellungen auf echter Hardware testen.
5. Erst nach erfolgreichen Tests den Installationsknopf aktivieren und GitHub Pages einrichten.

**Wichtig:** Keine Platzhalter-Manifeste mit nicht existierenden Firmware-URLs veröffentlichen. Ein Webinstaller darf erst dann eine Installation anbieten, wenn alle Images geprüft sind.

## Vorschau

Die statische Oberfläche liegt unter `docs/install/index.html`. Sie lässt sich lokal im Browser ansehen; die Schaltfläche zum Flashen ist absichtlich deaktiviert.
