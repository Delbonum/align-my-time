# Changelog

Versionen nach [Semantic Versioning](https://semver.org/lang/de/): **MAJOR.MINOR.PATCH**.
PATCH = Fehlerbehebungen, MINOR = neue Funktionen, MAJOR = inkompatible Änderungen (z. B. gespeicherte Projekte).

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
