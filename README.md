# Align My Time

Ein DAW-Plugin (VST3 / AU, mit ARA 2) und eine Standalone-App, die eine frei eingespielte Spur aufs Projekttempo (oder ein frei eingegebenes Tempo) bringt, und zwar ohne Warp-Tabellen oder Hitpoint-Dialoge. Aktuelle Version: siehe [CHANGELOG.md](CHANGELOG.md).

1. **Tappen:** Die ganze Spur einmal anhören und im Takt eine Taste drücken (Leertaste oder eine andere Tap-Taste, Mausklick oder MIDI-Fußschalter), auf jede Eins oder auf jede Zählzeit.
2. **Prüfen:** Marker kontrollieren und verschieben. Die Tempokurve zeigt Ausreißer, „Unsaubere Taps begradigen“ gleicht sie auf Wunsch aus. Hier wählst du Time-Stretch oder Schneiden + Verschieben und hörst vorher/nachher an.
3. **Rendern:** Ergebnis als neue Spur (WAV per Drag & Drop) oder direkt in der Spur ersetzen (nicht-destruktiv).

| 1 · Tappen | 2 · Prüfen | 3 · Rendern |
|---|---|---|
| ![Tappen](docs/screenshots/1-tappen.png) | ![Prüfen](docs/screenshots/2-pruefen.png) | ![Rendern](docs/screenshots/3-rendern.png) |

| Standalone (Englisch) | Einstellungen | Handbuch |
|---|---|---|
| ![Standalone](docs/screenshots/4-standalone-englisch.png) | ![Einstellungen](docs/screenshots/5-einstellungen.png) | ![Handbuch](docs/screenshots/6-handbuch.png) |

Die Screenshots erzeugt der End-to-End-Test automatisch (siehe unten).

**Ausführliche Anleitung:** [Handbuch](docs/HANDBUCH.md) (englisch: [Manual](docs/MANUAL.md)). Es steckt auch in der App: **Hilfe › Handbuch**, `F1` oder das **?** oben rechts.

## Benutzung in Cubase

1. Die Events der Spur auswählen und dann **Audio › Erweiterungen › Align My Time** wählen (ARA). Alle Events der Spur landen im Plugin als *ein* großes Event.
2. **Leertaste** startet die Wiedergabe im Plugin mit 2 s Vorlauf (oder du spielst in Cubase ab und tappst mit). Danach jede Eins (oder jede Zählzeit) mittippen. Verpasste Schläge und Doppel-Taps werden automatisch korrigiert. Jeder Marker rastet auf den nächsten Anschlag ein (±70 ms).
3. **Prüfen:** Klick in die Wellenform spielt ab dort. Marker mit **Strg+Ziehen** verschieben (umkehrbar unter Einstellungen › Bedienung), mit ←/→ um 5 ms verschieben (mit Shift um 1 ms) oder per Doppelklick neu setzen. „Ab hier neu tappen“ wiederholt nur den Rest. Unter „1 Marker =“ stellst du um, wie weit zwei Taps auseinander liegen (z. B. ½ Takt, wenn du auf 1 und 3 getippt hast). Das Plugin schlägt das auch selbst vor. Unter „Erster Marker = Takt“ legst du fest, auf welchem Projekttakt der erste Marker landet.
4. **Rendern:**
   - *Als neue Spur*: Das Plugin schreibt eine WAV-Datei nach `Musik/Align My Time/`. Zieh die Kachel auf eine neue Spur. Mit „ab Projektanfang“ gehört die Datei an Takt 1.
   - *In dieser Spur ersetzen*: Die Spur spielt die angepasste Version. Das lässt sich jederzeit zurückschalten. Zum Festschreiben nutzt du Cubases „Render in Place“.

**Tap-Taste:** Standard ist **Tab**, weil die Leertaste in Cubase den Transport steuert. Unter ⚙ *Einstellungen › Bedienung* wählst du Leertaste, Tab, T, Eingabe, Strg+Leertaste oder eine beliebige eigene Taste. Willst du in Cubase mit der Leertaste tappen, legst du dort unter *Studio › Tastaturbefehle › Transport* bei „Start/Stop“ statt der Leertaste Strg+Leertaste fest.

**Zoom in „Prüfen“:** Mausrad (oder Pinch) zoomt, Shift+Mausrad scrollt, „Alles“ zeigt wieder die ganze Aufnahme.

**Ziel-Tempo:** Ein Klick auf die Tempo-Anzeige oben rechts öffnet die Einstellung. Dort übernimmst du das Projekttempo oder gibst Tempo und Taktart von Hand ein.

**Unsaubere Taps begradigen:** In „Prüfen“ zieht der Regler (Aus bis 100 %) die Marker zu einem gleichmäßigen Tempoverlauf. Einzelne verrutschte Taps werden korrigiert, Tempoänderungen bleiben erhalten. Ist „An Transienten einrasten“ an, entscheiden die erkannten Anschläge mit: Korrigierte Marker landen auf dem tatsächlichen Schlag, Marker auf einem passenden Anschlag bleiben, wo sie sind. Die getappten Marker bleiben dabei gespeichert: „Aus“ (Doppelklick auf den Regler) stellt sie exakt wieder her.

**Mehrspur (z. B. Schlagzeug):** Über „Mehrspur“ kommen weitere Spuren dazu, die mit denselben Markern angepasst werden und phasengleich bleiben. Mit ARA kreuzt du dort die anderen Spuren an, auf denen Align My Time ebenfalls als ARA-Erweiterung läuft. „In dieser Spur ersetzen“ wirkt dann auf alle. Alternativ (und im Standalone) fügst du Audiodateien hinzu oder ziehst mehrere Dateien auf einmal ins Fenster. Getappt und eingerastet wird auf der Summe aller Spuren. „Als neue Spur“ schreibt eine Datei pro Spur, alle ab derselben Position. Für Schlagzeug ist „Schneiden + Verschieben“ die sicherste Wahl: Dann bleiben die Spuren sample-genau phasengleich.

**Audiodatei statt Spur:** Mit „Datei laden …“ oder per Drag & Drop einer Audiodatei ins Fenster arbeitest du mit einer Datei statt mit der Spur. „Zurück zur Spur“ wechselt zurück.

**Ohne ARA** (andere DAWs, oder als normaler Insert) setzt du das Plugin als Insert auf die Spur und spielst das Projekt einmal ab. Das Plugin nimmt die Spur dabei auf, und du kannst im selben Durchgang mittappen. Jedes weitere Abspielen in Schritt 1 ergänzt die Aufnahme. Oben links zeigt „ARA“ oder „Insert“, in welchem Modus das Plugin gerade läuft.

**Standalone-App:** Audiodatei laden (WAV, AIFF, FLAC, Ogg, MP3), Tempo oben rechts eingeben, tappen, prüfen, als WAV exportieren. Die Menüleiste (Datei | Bearbeiten | Ansicht | Hilfe) bietet Projekte (`.amtp`: neu, öffnen, speichern, zuletzt geöffnet), Rückgängig/Wiederherstellen, die Schritte, Zoom, Sprache, Handbuch und Credits. Das Projektformat steht in [docs/PROJEKTFORMAT.md](docs/PROJEKTFORMAT.md).

**Rückgängig:** `Strg+Z` / `Strg+Y` (auch im Plugin) für Marker, Raster, Begradigen, Verfahren und Ziel-Tempo.

**Einstellungen** (Zahnrad oben rechts, im Standalone auch **Bearbeiten › Einstellungen …**): Sprache (Deutsch/Englisch), Tap-Taste, im Standalone **Audio & MIDI** (Ausgang, MIDI-Eingang für den Fußschalter) und Credits mit der aktuellen Version.

Die Entscheidungen hinter dem Konzept stehen in [docs/ENTSCHEIDUNGEN.md](docs/ENTSCHEIDUNGEN.md), lokales Weiterentwickeln in [docs/ENTWICKLUNG.md](docs/ENTWICKLUNG.md).

## Bauen

Voraussetzungen: CMake ≥ 3.24 und ein C++17-Compiler. JUCE 8, das ARA SDK 2.2 und Signalsmith Stretch lädt CMake automatisch herunter.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Die Plugins liegen danach unter `build/plugin/AlignMyTime_artefacts/Release/` (VST3, AU auf macOS, Standalone). Zum Testen kopierst du das `.vst3`-Bundle nach `C:\Program Files\Common Files\VST3` (Windows) bzw. `~/Library/Audio/Plug-Ins/VST3` (macOS).

Fertige Builds für Windows, macOS und Linux erzeugt GitHub Actions bei jedem Push. Du findest sie als Artefakte am Workflow-Lauf.

Nur den DAW-unabhängigen Kern bauen und testen (geht schnell):

```bash
cmake -S . -B build-core -DAMT_BUILD_PLUGIN=OFF && cmake --build build-core && ctest --test-dir build-core
```

### Tests

- `tests/CoreTests.cpp` deckt Tempo-Map (inklusive Tempo- und Taktartwechsel), Tap-Bereinigung, Transienten-Einrasten, Warp-Map und beide Render-Verfahren ab. Gemessen wird, dass jeder Schlag nach dem Ausrichten auf dem Projektraster liegt (< 1 ms) und dass die Tonhöhe erhalten bleibt.
- `plugin/tests/PluginSmokeTest.cpp` simuliert einen Host: Eine schwankend eingespielte Spur läuft durchs Plugin, ein „Fußschalter“ tappt per MIDI mit, danach folgen Rendern, Ersetzen in der Spur, Speichern und Laden, Mehrspur (Phasengleichheit), das Begradigen der Taps, Rückgängig, Projektdateien und Menüleiste der Standalone-App sowie das Handbuch. Mit einem Ordner als Argument speichert der Test die Screenshots: `xvfb-run ./build/plugin/AlignMyTimeSmokeTest docs/screenshots`.

## Aufbau

```
core/      DAW-unabhängig, ohne JUCE: Tempo-Map, Marker, Transienten, Warp-Map, Renderer
plugin/    JUCE-Plugin: ARA-Anbindung, Sitzung, Vorhören, Export, Projektdateien, Oberfläche (Source/ui),
           Standalone-App mit Menüleiste (Source/standalone)
docs/      Handbuch (Deutsch/Englisch, auch in die App eingebaut), Projektformat, Entscheidungen
tests/     Kern-Tests
```

## Lizenzen

- **JUCE 8**: AGPLv3 oder kommerzielle JUCE-Lizenz. Für einen Closed-Source-Vertrieb brauchst du eine JUCE-Lizenz.
- **ARA SDK**: Apache 2.0.
- **Signalsmith Stretch**: MIT.
- **VST** ist eine Marke der Steinberg Media Technologies GmbH. Für den Vertrieb von VST3-Plugins gilt das Steinberg-Lizenzabkommen.
