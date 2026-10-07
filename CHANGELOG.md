# Changelog – Align My Time

Versionen nach [Semantic Versioning](https://semver.org/lang/de/): **MAJOR.MINOR.PATCH**.
PATCH = Fehlerbehebungen, MINOR = neue Funktionen, MAJOR = inkompatible Änderungen (z. B. gespeicherte Projekte).

## 1.5.0 – 2026-10-07

### Neu
- **Tempo-Map exportieren (MIDI):** dritte Option unter „Ergebnis landet …“ (Standalone auch **Datei › Tempo-Map exportieren …**, `Strg+Shift+E`). Statt die Aufnahme anzupassen, folgt das Projekt ihr: Die MIDI-Datei enthält Tempo und Taktart der Aufnahme, jeder Marker liegt genau auf seinem Takt. Die Zeit vor dem ersten Marker wird zu ganzen Takten, bei Bedarf mit Auftakt in Sechzehnteln.
- **Exportformat wählbar:** 16 bit (mit TPDF-Dither), 24 bit oder 32 bit float; Samplerate unverändert oder 44,1 / 48 / 88,2 / 96 kHz (beim Heruntersetzen mit Anti-Aliasing-Filter). Wird mit dem Projekt gespeichert.
- **Tap-Ausgleich** unter Einstellungen › Bedienung (−100 bis +100 ms) für Taps, die regelmäßig zu spät oder zu früh kommen. Align My Time misst den Versatz aus den eingerasteten Taps des Projekts, **Übernehmen** stellt ihn ein. Gilt für alle Projekte (die bisher nie benutzte Projekteinstellung `tapOffsetMs` entfällt).
- **Plugin-Fenster in der Größe veränderbar** (50 bis 200 %, Seitenverhältnis bleibt). Die Oberfläche skaliert als Ganzes; die Größe wird gemerkt, für Plugin und App getrennt.

### Geändert
- Die Anschläge (Transienten) einer neu geladenen Spur werden im Hintergrund erkannt, das Fenster bleibt dabei bedienbar („Anschläge werden erkannt …“ neben der Quelle). Marker rasten danach ein; ein Rendern wartet so lange.

## 1.4.0 – 2026-10-07

### Neu
- **Standalone-App mit Menüleiste:** Datei | Bearbeiten | Ansicht | Hilfe, mit Tastenkürzeln (`Strg+N`, `Strg+O`, `Strg+S`, `Strg+E` …). Die englische Leiste „Audio input is muted …“ und der Button „Options“ sind weg.
- **Projekte (`.amtp`)** in der Standalone-App: neu, öffnen, speichern, speichern unter, zuletzt geöffnet. Ein Projekt verweist auf die Audiodateien (absolut und relativ zum Projekt, ein Projektordner lässt sich also verschieben) und enthält Marker, Raster, Ziel-Tempo und alle Einstellungen. Ungespeicherte Änderungen zeigt ein `*` im Fenstertitel; vor „Neu“, „Öffnen“ und „Beenden“ fragt die App nach. Format: `docs/PROJEKTFORMAT.md`.
- **Audio & MIDI** als eigener Bereich in den Einstellungen (Standalone), auf Deutsch. Es gibt nur noch ein Einstellungen-Fenster. Direkt erreichbar über **Bearbeiten › Audio- und MIDI-Einstellungen …**
- **Handbuch** in der App: **Hilfe › Handbuch**, `F1` oder das **?** oben rechts (auch im Plugin), auf Deutsch und Englisch. **Hilfe › Tastenkürzel** springt zur Übersicht, **Hilfe › Credits** öffnet die Einstellungen bei den Credits. Als Datei: `docs/HANDBUCH.md` bzw. `docs/MANUAL.md`.
- **Rückgängig / Wiederherstellen** (`Strg+Z`, `Strg+Y` bzw. `Strg+Shift+Z`) für Marker, Raster, ersten Takt, Begradigen, Einrasten, Verfahren und Ziel-Tempo. Ein ganzer Tap-Durchgang und ein ganzes Ziehen sind je ein Schritt.
- Standalone: **Ergebnis exportieren** fragt mit einem normalen Speichern-Dialog nach dem Ort.
- Standalone: Das Fenster ist größenveränderbar (der Inhalt skaliert mit) und startet auf kleinen Bildschirmen passend verkleinert.

### Behoben
- `Shift+←`/`Shift+→` in „Prüfen“ verschiebt den Marker jetzt wirklich um 1 ms (bisher passierte gar nichts).
- „Marker bearbeiten“ und „Stopp“ erschienen in der englischen Oberfläche auf Deutsch.
- Dialoge von JUCE (z. B. „OK“/„Cancel“) erscheinen in der eingestellten Sprache.

### Intern
- Doppelten Resampling-Code zusammengelegt, ungenutzte Funktionen und die nie verwendete Einstellung `muteOriginal` entfernt, `tools/__pycache__` aus Git entfernt.
- Der End-to-End-Test verändert die Einstellungen des Benutzers nicht mehr (eigene Einstellungsdatei) und deckt Rückgängig, Projektdateien, Menüleiste und Handbuch ab.

## 1.3.0 – 2026-10-06

### Neu
- **Unsaubere Taps begradigen** in „Prüfen“: stufenloser Regler von „Aus“ bis 100 %. Die Marker werden zu einem gleichmäßigen Tempoverlauf gezogen. Einzelne verrutschte Taps werden so korrigiert, Tempoänderungen (z. B. ein Ritardando) bleiben erhalten. Die Einstellung ist nicht-destruktiv: Die getappten Marker bleiben gespeichert, „Aus“ (oder Doppelklick auf den Regler) stellt sie exakt wieder her. Von Hand gesetzte Marker bleiben, wo sie sind.
  - Mit „An Transienten einrasten“ helfen die erkannten Anschläge (Hitpoints) mit: Marker auf einem passenden Anschlag bleiben dort. Taps, die so weit danebenlagen, dass sie nicht eingerastet sind, und Marker auf dem falschen Anschlag (Ghost-Note, Flam) landen auf dem tatsächlichen Schlag.
- **Mehrspur** (z. B. alle Mikrofone einer Schlagzeugaufnahme): Weitere Spuren werden mit denselben Markern angepasst und bleiben phasengleich. Getappt, eingerastet und vorgehört wird auf der Summe aller Spuren.
  - Mit ARA: Button „Mehrspur“ › Spuren des Projekts ankreuzen, auf denen Align My Time ebenfalls als ARA-Erweiterung läuft. „In dieser Spur ersetzen“ ersetzt dann auch dort.
  - Überall (auch Standalone): weitere Audiodateien hinzufügen, oder mehrere Dateien auf einmal ins Fenster ziehen.
  - „Als neue Spur“ schreibt eine WAV-Datei pro Spur, alle gleich lang und ab derselben Position. Die Kachel zieht alle Dateien auf einmal in die DAW.

## 1.2.1 – 2026-10-05

### Geändert
- Schreibweise des Namens überall: **Align My Time** (Plugin-, App- und Ordnernamen, Oberfläche, Doku).

## 1.2.0 – 2026-10-05

### Neu
- **Zoom und Scrollen** in „Prüfen“: Mausrad oder Trackpad-Pinch zoomt um die Mausposition, Shift+Mausrad oder seitliches Wischen scrollt; dazu Scrollleiste und die Buttons −, + und „Alles“. Beim Abspielen blättert die Ansicht mit.
- **Abspielen ab einer Stelle:** Klick in die Wellenform spielt ab dort (Original oder angepasst). Marker verschiebt man mit **Strg+Ziehen**; in den Einstellungen lässt sich das umkehren.
- **„1 und 3“** als dritte Option bei „Ich tippe auf …“.
- **Tap-Taste:** zusätzlich „Strg+Leertaste“ und „Eigene Taste“ (beliebige Taste festlegen).

### Geändert
- **Tab ist die Standard-Tap-Taste**, weil die Leertaste in DAWs meist den Transport steuert.
- Einstellungen mit Navigation links (Allgemein, Bedienung, Credits) und Inhalt rechts.
- Einstellungs-Button als Zahnrad; schärferes Logo oben links.

## 1.1.1 – 2026-10-01

### Geändert
- Neues Programm-Icon (Standalone-App unter Windows/macOS) und dasselbe Logo oben links im Plugin-Fenster.

## 1.1.0 – 2026-10-01

### Neu
- **Tap-Taste wählbar** (Leertaste, Tab, T, Eingabe) unter Einstellungen. Unter Windows wird die Taste systemweit abgefragt, damit Taps auch ankommen, wenn der Host die Taste abfängt. **Strg+Leertaste tappt nie** und bleibt dem Host, z. B. für Cubases Start/Stop.
- **Manuelles Ziel-Tempo und Taktart** (Klick auf die Tempo-Anzeige oben rechts). Das Standalone nutzt immer das manuelle Tempo.
- **Audiodateien laden** (WAV, AIFF, FLAC, Ogg, MP3) per Button oder Drag & Drop. Im Standalone ist das die Quelle, im Plugin eine Alternative zur Spur („Zurück zur Spur“ wechselt zurück).
- **Einstellungen** (Zahnrad oben rechts): Sprache Deutsch/Englisch, Tap-Taste, **Credits** mit Version und Entwickler.
- Standalone: Ergebnis „Als Datei speichern“; die Leertaste startet und stoppt die Wiedergabe, wenn eine andere Tap-Taste gewählt ist.

### Geändert
- Die ARA-Factory-ID hängt nicht mehr von der Version ab, damit gespeicherte Projekte bei Updates zuordenbar bleiben.

## 1.0.1 – 2026-10-01

### Behoben
- Gesperrte Buttons erklären den Grund. Die Leertaste geht an den Host, solange noch keine Spur da ist, und der Fokus wird nicht mehr festgehalten.
- Insert-Modus: Jeder Abspieldurchgang ergänzt die Aufnahme, Taps eines zweiten Durchgangs gehen nicht mehr verloren, „Aufnahme verwerfen“ fragt nach.
- ARA: lädt die Events automatisch, sobald Cubase sie freigibt.

### Neu
- Tap-Abstand (2 Takte … ½ Zählzeit) mit Vorschlag per Klick, wenn anders gezählt wurde.
- Mix-Regler Spur ↔ Klick beim Vorhören.

## 1.0.0 – 2026-10-01

- Erste Version: Tappen → Prüfen → Rendern, ARA 2 und Insert-Aufnahme, Time-Stretch (tonhöhenerhaltend) oder Schneiden + Crossfade, Export als WAV oder Ersetzen in der Spur.
