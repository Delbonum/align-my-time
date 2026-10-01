# Entscheidungen

Die offenen Fragen aus dem Entwurf und wie sie beantwortet wurden.

## 1. Wie kommt die Spur ins Plugin?

**ARA 2 als Hauptweg, Aufnahme als Fallback.**

- Ein normaler Insert sieht nur das Audio, das gerade läuft, nicht die Events der Spur. Über ARA 2 bekommt das Plugin die Events direkt vom Host. Unterstützt wird das von Cubase, Studio One, Logic, Reaper, Cakewalk und weiteren Hosts. Das Plugin fasst alle Events zu einem Clip in Songzeit zusammen und rechnet unterschiedliche Abtastraten um (`plugin/Source/SourceLoader.cpp`).
- Über ARA liest das Plugin auch die **Tempo-Map und die Taktarten des Projekts**. Tempo- und Taktartwechsel werden also berücksichtigt.
- Ohne ARA nimmt das Plugin seinen Eingang auf, während der Host spielt. Getappt wird im selben Durchgang. Tempo und Taktart kommen dann vom Playhead des Hosts (konstantes Tempo). Die Aufnahme wird neben dem Projekt gespeichert, damit sie nach dem Neuladen wieder da ist.

## 2. Was passiert vor dem ersten und nach dem letzten Tap?

- Der **erste Marker landet auf dem nächstgelegenen Projekttakt**. In Schritt 2 lässt sich das korrigieren („Erster Marker = Takt“).
- **Time-Stretch:** Material vor dem ersten Marker (z. B. ein Auftakt) und nach dem letzten (Ausklang) wird mit dem Tempo des angrenzenden Takts weitergedehnt. Ein Auftakt bleibt dadurch musikalisch richtig.
- **Schneiden + Crossfade:** Diese Bereiche bleiben unverändert (1:1) und hängen am ersten bzw. letzten Marker.

## 3. Taktarten

- Die Taktart kommt immer aus dem Projekt. Im Modus „Jede Zählzeit“ zählt das Plugin entlang der Projekt-Taktarten weiter, auch über Wechsel hinweg.
- Der erste Tap gilt als Zählzeit 1.

## 4. Wie wird gerendert?

Ein Plugin kann in Cubase keine Spuren anlegen. Deshalb:

- **Als neue Spur:** Das Plugin schreibt eine 24-bit-WAV-Datei, die du per Drag & Drop aus dem Plugin auf eine neue Spur ziehst. Mit „ab Projektanfang“ beginnt die Datei bei Songposition 0 und gehört an Takt 1, damit ist sie automatisch richtig platziert.
- **In dieser Spur ersetzen:** Die Spur spielt über ARA die angepasste Version. Das ist nicht-destruktiv, jederzeit zurückschaltbar und bleibt im Projekt gespeichert. Zum Festschreiben nutzt du „Render in Place“ in Cubase.

## Weitere Entscheidungen

| Thema | Entscheidung | Warum |
|---|---|---|
| Time-Stretch-Engine | [Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch) (MIT) | Gute Qualität, tonhöhenerhaltend, Lizenz erlaubt kommerziellen Vertrieb (Rubber Band wäre GPL oder kostenpflichtig) |
| Transienten | Rund um jeden Anschlag wird das Original-Audio unverändert und exakt platziert eingesetzt | Verhindert das typische Vorecho des Time-Stretchings, Drums und Anschläge bleiben knackig |
| Qualitätsstufen | Rhythmisch / Melodisch / Komplex (60 / 120 / 160 ms Analysefenster) | Drei verständliche Stufen statt Algorithmus-Namen |
| Schneiden-Modus | Crossfade liegt *vor* jedem Anschlag (Standard 10 ms), Lücken werden ausgeblendet | Anschläge werden nie weichgezeichnet |
| Tap-Eingabe | Leertaste, Mausklick (löst schon beim Drücken aus), MIDI-Note oder Sustain-Pedal | Fußschalter = Hände frei fürs Instrument |
| Tap-Fehler | Doppel-Taps (< 45 % des lokalen Abstands) werden entfernt, Lücken (> 160 %) aufgefüllt; aufgefüllte Marker erscheinen hohl | Ein verpasster Schlag zwingt nicht zum Neustart |
| Tap-Genauigkeit | Einrasten auf den nächsten Anschlag innerhalb ±70 ms | Menschliches Tippen schwankt um ±30–50 ms |
| Einzähler | 2 s Vorlauf statt Klick | Das Tempo der Aufnahme ist vor dem Tappen unbekannt, ein Klick im Projekttempo würde in die Irre führen |
| Vorhören | Eigene Wiedergabe im Plugin (Original / Angepasst, optional mit Klick im Projekttempo) | Der Host-Transport muss nicht bedient werden |

## Offen / nächste Schritte

- **Test in Cubase:** Bisher wurde nur unter Linux gebaut und ohne DAW getestet (simulierter Host). Windows- und macOS-Builds erzeugt die CI. Das ARA-Verhalten in Cubase muss noch praktisch geprüft werden.
- **Leertaste in Cubase:** Je nach Fokus kann Cubase die Leertaste für den eigenen Transport abfangen. Alternativen sind Mausklick auf das TAP-Feld und MIDI-Fußschalter. Eventuell ist eine frei belegbare Tap-Taste nötig.
- **Tap-Latenz kalibrieren:** Die Einstellung `tapOffsetMs` existiert, ist aber noch nicht in der Oberfläche. Das Einrasten auf Anschläge fängt den Versatz in der Praxis meist ab.
- **Ohne ARA:** Ein Offline-Export schneller als Echtzeit kann bei der Aufnahme Blöcke verlieren. Deshalb in Echtzeit abspielen.
