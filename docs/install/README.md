# ESP32 Webinstaller

Diese Variante unterstützt zunächst nur **klassische ESP32-Boards** mit dem PlatformIO-Profil `ESP32` (`esp32dev`).

## Automatischer Build

Der Workflow `.github/workflows/firmware.yml` baut die Firmware und lädt das Artefakt `wordclock-esp32-webinstaller` hoch. Das Paket enthält:

- `index.html` – Webinstaller mit ESP Web Tools
- `manifest.json` – ESP32 Flash-Offsets
- `bootloader.bin`, `partitions.bin`, `boot_app0.bin`, `firmware.bin`
- `SHA256SUMS.txt` – Prüfsummen des Pakets

Das Packaging-Skript `docs/install/package_esp32.py` prüft, ob alle Images vorhanden und nicht leer sind, die Partitionstabelle plausibel ist und keine Images die nächste Flash-Region überlappen. Es **flasht keine Hardware**.

## Test des Artefakts

1. GitHub Actions öffnen und den erfolgreichen Lauf für den Branch `feature/wordclock-webinstaller` auswählen.
2. Das Artefakt `wordclock-esp32-webinstaller` herunterladen und entpacken.
3. Dateien lokal per HTTP bereitstellen, z. B. im entpackten Verzeichnis mit `python -m http.server 8000`.
4. Die Seite unter `http://localhost:8000` in Chrome oder Edge aufrufen.
5. **Vor einem Flash-Versuch** auf einem entbehrlichen Test-ESP32 Firmware-Layout, Chiptyp, Pinbelegung und Flash-Offsets prüfen. Bestehende Einstellungen vorher sichern.

Die Installer-Schaltfläche wird nur eingeblendet, wenn Manifest und vier Firmware-Dateien vom Server erreichbar sind. Das ist **keine** vollständige Hardware- oder Sicherheitsprüfung.

## Noch nicht freigegeben

- GitHub Pages ist als manuelles Preview-Deployment vorbereitet; zuerst Hardwaretests durchführen.
- Im Entwicklungsbranch wurde der automatische Aufruf von `erase_before_upload.py` aus `platformio.ini` entfernt. Der Erhalt der Einstellungen muss trotzdem auf Hardware geprüft werden.
- Ein Webinstaller mit Bootloader/Partitionstabelle ist zunächst für die **Erstinstallation** vorgesehen. Für Updates mit Erhalt der Einstellungen ist ein gesondert getesteter Pfad erforderlich.
- ESP32-C3 und ESP8266 sind nicht Bestandteil dieses Pakets.

## GitHub Pages (Testveröffentlichung)

Nach erfolgreichem Hardwaretest in Settings → Pages als Source **GitHub Actions** wählen. Die Deployment-Umgebung `github-pages` muss den Entwicklungsbranch zulassen. Den Workflow auf diesem Branch manuell mit `publish_preview: true` starten, sofern GitHub ihn zur manuellen Ausführung anbietet. GitHub verlangt für workflow_dispatch gegebenenfalls, dass die Workflow-Datei auch im Default-Branch vorhanden ist; in diesem Fall erst den Workflow per geprüftem Pull Request übernehmen.

Die Deployment-URL steht nach erfolgreichem Lauf im Job `pages`. Es wird keine Veröffentlichung bei einem normalen Push ausgelöst.
