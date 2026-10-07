# Lokal weiterentwickeln

Alles, was du brauchst, liegt im Repository. Abhängigkeiten (JUCE 8, ARA SDK, Signalsmith Stretch) lädt CMake beim ersten Konfigurieren selbst herunter. Es gibt keine Zugangsdaten, keine `.env`-Dateien und keine Dateien außerhalb von Git, die du mitnehmen müsstest.

## Einrichten

**Windows**
1. [Visual Studio 2022](https://visualstudio.microsoft.com/) (Community reicht) mit der Workload „Desktopentwicklung mit C++“ installieren. CMake ist darin enthalten.
2. [Git](https://git-scm.com/) installieren, dann klonen und bauen:
   ```powershell
   git clone https://github.com/Delbonum/align-my-time.git
   cd align-my-time
   cmake -S . -B build
   cmake --build build --config Release --target AlignMyTime_VST3
   ```
3. Das Plugin liegt dann unter `build\plugin\AlignMyTime_artefacts\Release\VST3\Align My Time.vst3`. Kopiere es nach `C:\Program Files\Common Files\VST3` (oder setze in `plugin/CMakeLists.txt` `COPY_PLUGIN_AFTER_BUILD TRUE`, dann kopiert der Build selbst; dafür braucht Visual Studio Administratorrechte).

Alternativ öffnest du den Ordner direkt in Visual Studio („Ordner öffnen“). VS erkennt das CMake-Projekt automatisch.

**macOS**
1. Xcode (aus dem App Store) und CMake installieren (`brew install cmake`).
2. Klonen und bauen wie oben. Für ein Xcode-Projekt: `cmake -S . -B build -G Xcode`.
3. VST3 nach `~/Library/Audio/Plug-Ins/VST3`, AU nach `~/Library/Audio/Plug-Ins/Components` kopieren.

Der erste Build dauert einige Minuten (JUCE). Danach geht es inkrementell schnell.

## Was nicht in Git gehört (und warum das okay ist)

| Was | Wo | Bemerkung |
|---|---|---|
| Build-Ordner | `build*/` | Wird jederzeit neu erzeugt (in `.gitignore`) |
| Heruntergeladene Abhängigkeiten | `build/_deps/` | Lädt CMake neu; offline? Siehe unten |
| Exportierte WAVs des Plugins | `Musik/Align My Time/` | Nutzerdaten, nicht Teil des Projekts |
| Projekte der Standalone-App | Standard: `Dokumente/Align My Time/` | Nutzerdaten (`.amtp`) |
| Aufnahmen im Insert-Modus | `%APPDATA%\Align My Time\Captures` bzw. `~/Library/Application Support/Align My Time/Captures` | Gehören zu Cubase-Projekten, können gelöscht werden, wenn das Projekt weg ist |

Offline bauen: Abhängigkeiten einmal klonen und CMake per `-DFETCHCONTENT_SOURCE_DIR_JUCE=…`, `-DFETCHCONTENT_SOURCE_DIR_ARA_SDK=…`, `-DFETCHCONTENT_SOURCE_DIR_SIGNALSMITH-STRETCH=…` und `-DFETCHCONTENT_SOURCE_DIR_SIGNALSMITH-LINEAR=…` darauf zeigen lassen.

## Testen

- **Schnell (ohne JUCE):** `cmake -S . -B build-core -DAMT_BUILD_PLUGIN=OFF && cmake --build build-core && ctest --test-dir build-core`
- **Komplett:** `ctest --test-dir build -C Release`. Der End-to-End-Test braucht unter Linux einen (virtuellen) Bildschirm: `xvfb-run ctest …`.
- **Standalone-App:** `build/plugin/AlignMyTime_artefacts/Release/Standalone/`. Eine eigene App (`plugin/Source/standalone/StandaloneApp.cpp`, `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1`) statt JUCEs Standard-Fenster: Menüleiste, Projektdateien, Audio/MIDI in den Einstellungen. Menü, Befehle und Projekte stecken in `AppController` (im gemeinsamen Code, damit der End-to-End-Test sie prüfen kann).
- **Plugin-Validierung:** [pluginval](https://github.com/Tracktion/pluginval) prüft das VST3 auf typische Host-Probleme.
- **Debuggen in Cubase:** Debug-Build erstellen (`--config Debug`), das Debug-`.vst3` installieren und in Visual Studio / Xcode „An Prozess anhängen“ → Cubase.

## Aufbau in Kürze

- `core/`: reines C++17 ohne JUCE. Tempo-Map, Taps → Marker, Transienten, Warp-Map, Renderer. Hier lohnt es sich zuerst einen Test zu schreiben (`tests/CoreTests.cpp`).
- `plugin/Source/AlignSession.*`: Zustand der drei Schritte (Marker, Einstellungen, Rendern im Hintergrund, Speichern).
- `plugin/Source/PluginProcessor.*`: Audio-Thread, Host-Anbindung, Insert-Aufnahme, MIDI-Taps.
- `plugin/Source/PlaybackRenderer.*` / `SourceLoader.*` / `DocumentController.*`: ARA.
- `plugin/Source/ProjectFile.*`: Projektdateien `.amtp` der Standalone-App (Format: `docs/PROJEKTFORMAT.md`).
- `plugin/Source/standalone/`: Menüleiste und Befehle (`AppController`), das Fenster (`StandaloneApp.cpp`, nur im Standalone-Target).
- `plugin/Source/ui/`: eine Datei pro Seite. Texte sind Deutsch, als UTF-8 im Quelltext und in `tr ("…")` verpackt; die englische Fassung steht in `Translations.cpp` (`tools/check_translations.py` prüft das, `tools/update_translations.py` ergänzt neue Einträge aus einer JSON-Datei).
- `docs/HANDBUCH.md` / `docs/MANUAL.md`: das Handbuch. Es wird in die App eingebaut (`ui/ManualView`), Änderungen dort erscheinen also nach dem nächsten Build auch unter **Hilfe › Handbuch**. Jedes Kapitel (`## …`) braucht eine `<!-- id: … -->`-Zeile, in beiden Sprachen dieselben.

## Mit Claude weiterarbeiten

Die Datei `CLAUDE.md` im Wurzelverzeichnis fasst Aufbau und Konventionen zusammen. Claude Code liest sie automatisch, egal ob lokal oder im Web. Der ursprüngliche Oberflächen-Entwurf liegt als Design-Canvas auf claude.ai (Link im Chat der ersten Sitzung). Er ist nicht Teil des Repos und für die Entwicklung nicht nötig.
