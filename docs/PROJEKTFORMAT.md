# Projektformat `.amtp`

Die Standalone-App speichert Projekte als XML-Datei (UTF-8) mit der Endung `.amtp` (**A**lign **M**y **T**ime **P**roject). Geschrieben und gelesen wird sie in `plugin/Source/ProjectFile.cpp`, getestet im End-to-End-Test (Schritt 14).

Das Audio steckt nicht in der Datei, nur Verweise darauf. So bleiben Projekte klein, und die Originaldateien werden nie verändert.

## Beispiel

```xml
<?xml version="1.0" encoding="UTF-8"?>
<AlignMyTimeProject formatVersion="1" appVersion="1.4.0">
  <Audio>
    <File role="main" name="Bass" path="C:\Musik\Session\Audio\Bass.wav" relativePath="Audio/Bass.wav"/>
    <File role="extra" name="Overheads" path="C:\Musik\Session\Audio\Overheads.wav" relativePath="Audio/Overheads.wav"/>
  </Audio>
  <AlignMyTime version="2" tapUnit="1" manualTempo="1" manualBpm="96.0" manualNumerator="4" manualDenominator="4"
               method="0" quality="0" crossfadeMs="10.0" snapToAttacks="1" straighten="0.0" firstBar="-100000"
               step="1" …>
    <Markers>
      <Marker seconds="1.0312" tapped="1.0420" origin="0" snapped="1"/>
      …
    </Markers>
  </AlignMyTime>
</AlignMyTimeProject>
```

## Elemente

### `<AlignMyTimeProject>` (Wurzel)

| Attribut | Bedeutung |
|---|---|
| `formatVersion` | Version dieses Formats, derzeit `1`. Eine App lehnt Dateien mit höherer Version ab, statt sie falsch zu lesen. |
| `appVersion` | Version der App, die die Datei geschrieben hat (nur zur Information). |

### `<Audio>` › `<File>`

Eine Zeile pro Audiodatei.

| Attribut | Bedeutung |
|---|---|
| `role` | `main` = die Hauptspur (getappt und angezeigt), `extra` = weitere Spur (Mehrspur), mit denselben Markern angepasst. |
| `name` | Anzeigename, auch für die Dateinamen beim Export. |
| `path` | Absoluter Pfad beim Speichern. |
| `relativePath` | Pfad relativ zum Ordner der Projektdatei, mit `/` getrennt. |

Beim Öffnen sucht die App jede Datei zuerst über `relativePath` (so kann ein Projektordner verschoben oder kopiert werden), dann über `path`. Fehlt sie an beiden Stellen, werden Marker und Einstellungen trotzdem geladen, und die App nennt die fehlenden Dateien.

### `<AlignMyTime>` (Sitzung)

Derselbe Zustand, den das Plugin im DAW-Projekt speichert (`AlignSession::toValueTree()`), ohne die Liste der weiteren Spuren (die steht unter `<Audio>`).

| Attribut | Bedeutung |
|---|---|
| `version` | Version des Sitzungs-Formats (derzeit `2`). |
| `tapUnit` | Abstand zweier Marker: 0 = 2 Takte, 1 = 1 Takt, 2 = ½ Takt, 3 = 1 Zählzeit, 4 = ½ Zählzeit. |
| `manualTempo`, `manualBpm`, `manualNumerator`, `manualDenominator` | Ziel-Tempo und Taktart. Die Standalone-App nutzt immer das eingegebene Tempo. |
| `firstBar` | Takt (ab 0), auf dem der erste Marker landet; `-100000` = automatisch (nächster Takt). |
| `method` | 0 = Time-Stretch, 1 = Schneiden + Verschieben. |
| `quality` | Time-Stretch: 0 = Rhythmisch, 1 = Melodisch, 2 = Komplex. |
| `crossfadeMs` | Crossfade beim Schneiden (ms). |
| `snapToAttacks` | Marker rasten auf Anschläge ein (0/1). |
| `straighten` | „Unsaubere Taps begradigen“, 0 bis 1. |
| `clickBlend`, `clickInPreview`, `leadIn` | Vorhören: Mix Spur/Klick, Klick an, Vorlauf. |
| `tapOffsetMs` | Wird zu jedem Tap addiert (Latenzausgleich, derzeit immer 0). |
| `destination`, `exportFromProjectStart`, `trackName` | Rendern: Ziel (0 = Datei, 1 = in der Spur ersetzen; im Standalone immer Datei), Datei ab Projektanfang, Spurname. |
| `step` | Angezeigter Schritt: 0 = Tappen, 1 = Prüfen, 2 = Rendern. |
| `replaceActive` | Nur Plugin: „In dieser Spur ersetzen“ aktiv. |

`<Markers>` › `<Marker>`, in zeitlicher Reihenfolge:

| Attribut | Bedeutung |
|---|---|
| `seconds` | Position des Markers in der Aufnahme (Sekunden ab Dateianfang), nach Einrasten und Bearbeiten. |
| `tapped` | Wo der Tap tatsächlich lag. Daraus wird beim Laden neu eingerastet. |
| `origin` | 0 = getappt, 1 = automatisch ergänzt, 2 = von Hand gesetzt oder verschoben. |
| `snapped` | Marker liegt auf einem erkannten Anschlag (0/1). |

## Kompatibilität

- Neue, optionale Attribute dürfen jederzeit dazukommen. Ältere Apps ignorieren sie, neuere setzen fehlende auf den Standardwert.
- Ändert sich die Bedeutung bestehender Elemente, steigt `formatVersion`. Das ist eine inkompatible Änderung und erhöht die Major-Version der App (siehe `CHANGELOG.md`).
