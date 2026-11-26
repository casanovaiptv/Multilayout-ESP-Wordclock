# Multilayout ESP Wordclock 4.4.0-dev.2

Diese Entwicklungsfirmware gehört zum Branch `dev/wordclock-ui-hardware`. Die ZIP-Datei enthält den Testinstaller, die Browser-Demo, die Flash-Dateien für alle drei Zielplattformen und die zugehörigen SHA-256-Prüfsummen.

## Firmware-Weboberfläche

Öffne nach dem Start die IP-Adresse deiner Wortuhr im Browser. Die neue Oberfläche verwendet ein dunkles Design und dieselben Einstellungen wie zuvor. Der Sekundenring steht unter **Anzeige** direkt unter **Minuten**. Unter **Hardware** findest du Strombegrenzung und WPS. Das Captive Portal für die WLAN-Einrichtung und der OTA-Updater auf Port 81 bleiben verfügbar.

### Sekundenring

Der Sekundenring ist für jedes Frontlayout konfigurierbar. Die Anzahl reicht von 1 bis 200. Bei einer frischen Konfiguration ist der Ring mit 60 LEDs, Start-Offset 8 und der Animation **Umlaufender Sektor** eingeschaltet. Die Standardrichtung entspricht einer im Uhrzeigersinn angeschlossenen LED-Kette. Beim Einschalten eines zuvor ausgeschalteten Rings wird bei Bedarf die Punktanimation gewählt. Die Punkt-, Sektor- und wechselnde Sektoranimation sowie die separate Sekundenanzeige bleiben verfügbar.

Schließe den Ring am Ende der LED-Kette an. Die Firmware bestimmt seinen Anfang aus den letzten verwendeten physischen LED-Indizes der Matrix und den für das Layout verfügbaren Minutenvarianten. Bei Fronten mit sieben Minuten-LED-Positionen werden diese auch bei Auswahl von vier Minuten-LEDs reserviert. Die Layout- und Auflösungseinstellungen bestimmen die Verkabelung der Matrix. Die Anzahl des Rings ist unabhängig davon.

Die alten Rahmen-Layouts 13 und 18 werden zu den Frontlayouts 12 und 17 migriert. **Nach der aktualisierten Vorgabe werden beide alten Ringvarianten auf 60 LEDs und Offset 8 gesetzt.** Bei der originalen 10×11-Verdrahtung sitzen die Minuten-LEDs bei Indizes 110–113 und der Ring beginnt bei Index 114. Wenn die Matrix durch höhere Auflösung oder zusätzliche LEDs darüber hinausreicht, beginnt der Ring hinter der Matrix und den Minutenpositionen. Bei physisch weiterhin vorhandenen 48 oder 120 LEDs kannst du die passende Anzahl nach dem Update wieder einstellen. Überzählige LEDs eines früheren 120er-Rings erhalten Schwarz. Bestehende Fronten ohne Ring behalten ihren ausgeschalteten Ring. Andere EEPROM-Einstellungen und die WLAN-Zugangsdaten werden bei der Migration übernommen.

Zum Kalibrieren die Ring-Einstellungen speichern und **Kalibrieren** wählen. Ausschließlich die aktuelle Nullposition leuchtet. Plus und Minus durchlaufen die physischen Ringpositionen mit Überlauf am Ende. **Offset speichern** übernimmt die Position dauerhaft und setzt den Anzeigemodus fort. **Abbrechen**, das Verlassen des Anzeige-Menüs, eine unterbrochene Browser-Verbindung oder 120 Sekunden ohne Bedienung beenden die Vorschau ohne Übernahme.

### Strombegrenzung

Bei einer frischen Konfiguration ist die Strombegrenzung eingeschaltet. Voreingestellt sind 2,0 A Netzteilstrom und 0,3 A Reserve für ESP und weitere Verbraucher. Aus Netzteilstrom abzüglich Reserve ergibt sich das LED-Budget. Die Oberfläche zeigt den geschätzten LED-Strom der ausgegebenen Helligkeit und meldet einen aktiven Eingriff.

Das Rechenmodell verwendet 1 mA Ruhestrom pro konfigurierter LED-Position sowie bis zu 20 mA pro RGB- bzw. RGBW-Kanal. Ein gemeinsamer Faktor skaliert die Kanalwerte unmittelbar vor der Ausgabe. Die Dimmung greift sofort ein, die Helligkeit steigt bei freiem Budget schrittweise wieder an. Die eingestellte Wunschhelligkeit bleibt gespeichert. Auch Animationen, Kalibrierung und WPS-Anzeige laufen über diese Ausgabe.

Das ist eine rechnerische Schätzung. Reale Ströme hängen unter anderem vom LED-Typ und der Versorgungsspannung ab. Auch dunkle Pixel ziehen Ruhestrom. Dimensioniere Netzteil, Leitungen und Absicherung unabhängig von der Software.

### WLAN per WPS

Beim klassischen ESP32 ist GPIO 19 für neue Konfigurationen voreingestellt. Die WPS-Funktion ist zunächst ausgeschaltet. Beim ESP8266 und ESP32-C3 ist anfangs kein GPIO zugewiesen. Vor dem Aktivieren muss dort ein freier GPIO aus der Tabelle gewählt werden. Die Browser-Demo simuliert die Vorgabe des klassischen ESP32.

Verbinde einen Taster zwischen einem freien, zulässigen GPIO und GND. Der GPIO verwendet den internen Pull-up. Wähle und speichere den GPIO in der Oberfläche. Die zulässigen GPIOs der Build-Ziele sind unten aufgeführt. Zugewiesene GPIOs für LED-Daten, Taster oder I²C sind gesperrt. Wird später ein bereits zugewiesener WPS-GPIO für eine andere Hardwarefunktion belegt, deaktiviert die Firmware den WPS-Taster.

| Build-Ziel | Zulässige GPIOs vor Prüfung der Belegung |
| --- | --- |
| ESP8266 NodeMCU v2 | 4, 5, 12–14 |
| ESP32 Dev Module | 4, 13–14, 16–19, 21–23, 25–27, 32–33 |
| ESP32-C3 DevKitC-02 | 1, 3–7, 10 |

1. Halte den Taster drei Sekunden gedrückt, bis die Matrix blau leuchtet.
2. Starte WPS am Router. Für die Kopplung stehen bis zu 120 Sekunden zur Verfügung.
3. Nach erfolgreicher Kopplung hat die Uhr bis zu 20 Sekunden Zeit, eine IP-Adresse zu erhalten. Nach erfolgreicher Verbindung werden die Zugangsdaten gespeichert und die Matrix leuchtet zwei Sekunden grün.
4. Bei einem Fehler leuchtet die Matrix kurz rot. Die Uhr versucht, das vorherige WLAN wiederzuverwenden. Falls nach 20 Sekunden keine Verbindung besteht, startet das vorhandene Captive Portal.

## Entwicklung und Paket

Mit Node.js, PlatformIO und Python aus dem Repository-Verzeichnis:

```sh
tools/development/test_hardware.sh
pio run -e ESP8266 -e ESP32 -e ESP32C3
python tools/development/package.py
python -m http.server 8080 --directory dist/development
```

Der lokale Installer läuft unter `http://localhost:8080/`, die Demo unter `http://localhost:8080/demo/index.html`. Für USB-Installation auf einer externen Adresse ist HTTPS erforderlich. Chrome oder Edge am Desktop stellen Web Serial bereit. Bei einem normalen Update die Option zum Löschen des Geräts ausgeschaltet lassen. Eine vollständige Löschung entfernt Konfiguration und WLAN-Daten.

Die Browser-Demo verwendet dieselbe gebaute Oberfläche wie der ESP. Ein lokaler WebSocket-Ersatz simuliert die Hauptfunktionen. Die Einstellungen werden im Browser gespeichert und der Wortuhr-Displaybereich zeigt ein Beispiel für die jeweils gewählte Matrixgröße. Demo und Testinstaller funktionieren unabhängig von der produktiven Projektseite.

Der Workflow **Wordclock development package** baut nur für den Entwicklungsbranch. Sein Artefakt `wordclock-development` enthält das fertige statische Paket, das auch separat gehostet werden kann. Die Plattform ist im Installer auszuwählen. ESP Web Tools prüft beim Verbinden außerdem die Chipfamilie.

| Plattform | Images und Flash-Adressen |
| --- | --- |
| ESP8266 | `firmware.bin` bei `0x0000` |
| ESP32 | `bootloader.bin` bei `0x1000`, `partitions.bin` bei `0x8000`, `boot_app0.bin` bei `0xE000`, `firmware.bin` bei `0x10000` |
| ESP32-C3 | `bootloader.bin` bei `0x0000`, `partitions.bin` bei `0x8000`, `boot_app0.bin` bei `0xE000`, `firmware.bin` bei `0x10000` |

## Testbericht

### Automatisiert

- C++-Hosttests mit UndefinedBehaviorSanitizer für Layout-IDs, Matrixauflösungen, Verdrahtungsoptionen, Minutenpositionen, EEPROM-Migration und Integritätsprüfung des neuen Datensatzes. Die Tests prüfen auch Doppeltbelegungen innerhalb der Matrix und die Grenzen aller Ringgrößen.
- Ringtests für beide Laufrichtungen, Offset-Überlauf, deaktivierten Ring, Minutenwechsel sowie Punkt-, Sektor- und wechselnde Sektoranimation mit 1, 48, 60, 120 und 200 LEDs.
- Strommodell mit wechselnden RGBW-Farbwerten, Helligkeiten und Budgets einschließlich eines Budgets unterhalb des rechnerischen Ruhestroms.
- WPS-Zustandsautomat mit nachgebildeter ESP8266-SDK-Schnittstelle für langen Tastendruck, Erfolg, Fehler, Startfehler, Kopplungs- und Verbindungszeitüberschreitung, GPIO-Kollision und Anforderung des Captive-Portal-Fallbacks.
- PlatformIO kompiliert und linkt die Firmware für die drei in `platformio.ini` definierten Boards. Das ESP8266-Ziel nutzt Arduino Core 3.1.2, die ESP32-Ziele Arduino Core 2.0.17. Die benötigten ESP32/C3-WPS-Funktionen sind in beiden verlinkten SDKs vorhanden.

- Chromium bei 390, 768 und 1366 Pixeln Breite. Geprüft wurden Navigation, Layoutwechsel, Helligkeit und Farben, Kalibrierung mit Speichern und Abbrechen, deaktivierter Ring, Stromlimit, WPS-GPIO und simulierter Timeout sowie Laden der gespeicherten Werte nach einem Reload.
- Der Installer wählt die passenden Dateien für alle drei Zielplattformen aus. Typkennung, Offset, Partitionsgrenzen und Versionsstring werden beim Packen geprüft. Der Browsertest prüft außerdem Fehlermeldungen für fehlende Firmware und Manifeste ohne verpflichtenden Löschdialog.

| Ziel | RAM laut PlatformIO | Flash laut PlatformIO |
| --- | ---: | ---: |
| ESP8266 | 53.332 / 81.920 Byte (65,1 %) | 660.659 / 1.044.464 Byte (63,3 %) |
| ESP32 | 57.120 / 327.680 Byte (17,4 %) | 1.235.501 / 1.310.720 Byte (94,3 %) |
| ESP32-C3 | 49.652 / 327.680 Byte (15,2 %) | 1.207.672 / 1.310.720 Byte (92,1 %) |

Die Firmware liefert die Oberfläche als gzip-komprimiertes HTML aus dem Flash aus. Der Browsertest kann mit installiertem Playwright und Chromium über `node tools/development/test_browser.mjs` wiederholt werden. Bei einer separaten Playwright-Installation kann `PLAYWRIGHT_MODULE` auf den absoluten Pfad zu `playwright/index.mjs` gesetzt werden, `CHROMIUM_PATH` auf den Browser.

### Auf echter Hardware ausstehend

- Update einer bestehenden Installation ohne Flash-Löschung auf jedem Board, insbesondere EEPROM und WLAN-Zugangsdaten nach dem Neustart.
- Physische Reihenfolge der Ring- und Minuten-LEDs, beide Laufrichtungen und Kalibrierung, auch mit externen Minuten-LEDs und erhöhten Matrixauflösungen.
- Stromverbrauch per Messgerät, Farbverhältnisse, Helligkeitsverlauf und Flackerfreiheit an realen RGB- und RGBW-Ketten.
- WPS-Taster mit einem WPS-fähigen Router für alle drei Boards. Kopplung, Verbindungsfehler, Statusfarben, Reconnect, dauerhafte Zugangsdaten und echtes Captive Portal testen.
- Neuinstallation und Update per USB mit ESP Web Tools auf jedem Board. Dazu gehören Boot-ROM-Erkennung, USB-Treiber und ein Probelauf ohne Flash-Löschung.
- Freier Laufzeit-Heap bei großen Layouts und aktiver Strombegrenzung. Der Strombegrenzer benötigt bei tatsächlicher Skalierung einen Sicherungspuffer von 3 Byte je RGB- bzw. 4 Byte je RGBW-Pixel im Ausgabepuffer.
