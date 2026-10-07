# Entscheidungen

Die offenen Fragen aus dem Entwurf und wie sie beantwortet wurden.

## 1. Wie kommt die Spur ins Plugin?

**ARA 2 als Hauptweg, Aufnahme als Fallback.**

- Ein normaler Insert sieht nur das Audio, das gerade läuft, nicht die Events der Spur. Über ARA 2 bekommt das Plugin die Events direkt vom Host. Unterstützt wird das von Cubase, Studio One, Logic, Reaper, Cakewalk und weiteren Hosts. Das Plugin fasst alle Events zu einem Clip in Songzeit zusammen und rechnet unterschiedliche Abtastraten um (`plugin/Source/SourceLoader.cpp`).
- Über ARA liest das Plugin auch die **Tempo-Map und die Taktarten des Projekts**. Tempo- und Taktartwechsel werden also berücksichtigt.
- Ohne ARA (Plugin als normaler Insert) nimmt das Plugin seinen Eingang auf, während Cubase in Schritt 1 spielt. Getappt wird im selben Durchgang. **Jeder Abspieldurchgang ergänzt die Aufnahme**: Wer zu früh stoppt oder mittendrin startet, spielt einfach den fehlenden Teil noch einmal ab. Taps eines Durchgangs ersetzen die Marker ab dessen Startposition, davor bleibt alles erhalten. Tempo und Taktart kommen dann vom Playhead des Hosts (konstantes Tempo). Die Aufnahme wird neben dem Projekt gespeichert, damit sie nach dem Neuladen wieder da ist.
- Mit ARA versucht das Plugin jede Sekunde erneut, die Events zu laden, bis Cubase sie freigibt. Ein Klick auf „Spur neu laden“ ist dafür nicht nötig.

## 2. Was passiert vor dem ersten und nach dem letzten Tap?

- Der **erste Marker landet auf dem nächstgelegenen Projekttakt**. In Schritt 2 lässt sich das korrigieren („Erster Marker = Takt“).
- **Time-Stretch:** Material vor dem ersten Marker (z. B. ein Auftakt) und nach dem letzten (Ausklang) wird mit dem Tempo des angrenzenden Takts weitergedehnt. Ein Auftakt bleibt dadurch musikalisch richtig.
- **Schneiden + Crossfade:** Diese Bereiche bleiben unverändert (1:1) und hängen am ersten bzw. letzten Marker.

## 3. Taktarten und Tap-Abstand

- Die Taktart kommt immer aus dem Projekt. Im Modus „Jede Zählzeit“ zählt das Plugin entlang der Projekt-Taktarten weiter, auch über Wechsel hinweg.
- Der erste Tap gilt als Zählzeit 1.
- **Anders gezählt als gedacht?** In Schritt 2 lässt sich der Abstand zwischen zwei Markern umstellen: 2 Takte, 1 Takt, ½ Takt, 1 Zählzeit oder ½ Zählzeit. Passt das getappte Tempo deutlich besser zu einem anderen Abstand (z. B. auf 1 und 3 getippt, aber „Jede Eins“ gewählt), zeigt das Plugin einen Hinweis mit Ein-Klick-Lösung („Als ½ Takt werten“). Am Projekttempo muss man nichts ändern.

## 4. Wie wird gerendert?

Ein Plugin kann in Cubase keine Spuren anlegen. Deshalb:

- **Als neue Spur:** Das Plugin schreibt eine 24-bit-WAV-Datei, die du per Drag & Drop aus dem Plugin auf eine neue Spur ziehst. Mit „ab Projektanfang“ beginnt die Datei bei Songposition 0 und gehört an Takt 1, damit ist sie automatisch richtig platziert.
- **In dieser Spur ersetzen:** Die Spur spielt über ARA die angepasste Version. Das ist nicht-destruktiv, jederzeit zurückschaltbar und bleibt im Projekt gespeichert. Zum Festschreiben nutzt du „Render in Place“ in Cubase.

## 5. Bedienung, Sprachen, Version (ab 1.1.0)

- **Tap-Taste:** Ein Plugin kann die Tastaturbefehle von Cubase nicht ändern. Darum ist ab 1.2.0 **Tab** Standard; wählbar sind Leertaste, Tab, T, Eingabe, Strg+Leertaste und eine eigene Taste. Strg-Kombinationen, die nicht die Tap-Taste sind, gehen immer an den Host, und unter Windows fragt das Plugin die Tap-Taste systemweit ab (`GetAsyncKeyState`). Damit kommen Taps auch an, wenn Cubase die Taste für sich behält, und das auf etwa 2 ms genau. Unter macOS zählen nur Tasten, die das Plugin-Fenster erhält.
- **Ziel-Tempo:** Standardmäßig das Projekttempo, wahlweise manuell (Tempo + Taktart). Das Standalone hat kein Projekt und nutzt immer das manuelle Tempo.
- **Audiodateien:** Im Standalone die einzige Quelle, im Plugin eine Alternative (Datei liegt ab Songposition 0). Standard im Plugin bleibt ARA bzw. die Aufnahme.
- **Sprache:** Deutsch und Englisch, umschaltbar ohne Neustart. Gespeichert pro Rechner (nicht pro Projekt).
- **Version:** Semantic Versioning, sichtbar unter Einstellungen › Credits, Historie in `CHANGELOG.md`.

## 6. Taps begradigen und Mehrspur (ab 1.3.0)

**Begradigen** ist eine Einstellung, keine Aktion. Die getappten (und eingerasteten) Marker bleiben unverändert gespeichert. Angepasst wird mit den begradigten Positionen, und „Aus“ stellt jederzeit den Originalzustand her.

- **Verfahren:** Für jeden Marker sagt eine Ausgleichskurve durch seine Nachbarn (±4 Marker, er selbst zählt nicht mit) voraus, wo er liegen müsste. In der Mitte ist das eine Parabel, damit ein Ritardando nicht „hinterherhinkt“, an den Rändern eine Gerade. Nahe Nachbarn zählen mehr, und in einem zweiten Durchgang werden klare Ausreißer ignoriert (robuste Regression). Ein einzelner schlechter Tap verbiegt die Kurve also nicht. Der Marker wandert um die gewählte Stärke (25–100 %) in Richtung dieser Vorhersage.
- **Gemessen wird auf dem Projektraster** (in Viertelnoten, nicht in „Marker Nr.“). Ein 3/4-Takt zwischen 4/4-Takten wird deshalb nicht „wegbegradigt“.
- **Von Hand gesetzte Marker** bleiben, wo sie sind, und dienen ihren Nachbarn als verlässliche Stützstellen.
- **Hitpoints** (wenn „An Transienten einrasten“ an ist): Ein Marker auf einem Anschlag, der zur Kurve passt, bleibt dort. Der Anschlag ist der tatsächliche Schlag und damit ein besserer Beleg als jede Kurve. Marker ohne Anschlag und klare Ausreißer (z. B. auf einer Ghost-Note oder einem Flam eingerastet) zielen auf den deutlichsten Anschlag nahe der vorhergesagten Position, sonst auf die Kurve. Das Suchfenster ist ein Achtel des Markerabstands (höchstens 25 ms), damit eine benachbarte 16tel nicht für den Schlag gehalten wird. Marker auf einem Anschlag zählen bei der Kurve doppelt so viel wie Marker ohne. Wer den Groove bewusst glätten will, schaltet das Einrasten aus.
- **Stufenloser Regler** statt fester Stufen: Beim Ziehen sieht man live, wie Marker und Tempokurve wandern.
- 100 % heißt: Die Marker liegen auf dem gleichmäßigen Tempoverlauf. Was der Musiker gegenüber diesem Verlauf vor- oder zurückliegt, bleibt dann im Ergebnis erhalten. Das ist bewusst so, denn das ist der Groove, nicht der Tap-Fehler.

**Mehrspur:** Alle Spuren teilen sich Marker und Warp-Map. Gerendert wird jede Spur einzeln mit derselben Warp-Map, denselben geschützten Anschlägen und demselben Bereich. Die Ergebnisse sind deshalb gleich lang und beginnen am selben Sample.

- Getappt, eingerastet und vorgehört wird auf der **Summe** aller Spuren. Bei Schlagzeug rasten die Marker so auf Kick, Snare und Overheads zugleich ein.
- **Schneiden + Verschieben** ist sample-genau phasengleich, weil es nur verschiebt und überblendet. Time-Stretch ist es an den geschützten Anschlägen (die bei allen Spuren identisch sind). Dazwischen arbeitet der Phase-Vocoder je Spur, was bei stark übersprechenden Mikrofonen minimal abweichen kann.
- **Mit ARA** sieht das Plugin nur Spuren, auf denen Align My Time als ARA-Erweiterung läuft. Die Instanz, in der getappt wird, „leitet“. Sie veröffentlicht über den gemeinsamen ARA-Document-Controller die angepasste Version jeder verknüpften Spur, und die Instanz auf dieser Spur spielt sie bei „In dieser Spur ersetzen“ ab. Gespeichert wird die Verknüpfung über den Spurnamen (ARA kennt keine dauerhafte Spur-ID). Wird eine Spur umbenannt, muss man sie neu ankreuzen.
- **Ohne ARA** (Insert-Modus) kann ein Plugin die anderen Spuren nicht sehen. Dort kommen weitere Spuren als Audiodateien dazu. Sie gibt es nur als neue Spur, nicht als „ersetzen“.

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
| Vorhören | Eigene Wiedergabe im Plugin (Original / Angepasst, optional mit Klick im Projekttempo, Mix-Regler Spur ↔ Klick) | Der Host-Transport muss nicht bedient werden |
| Leertaste | Tappt bzw. startet die Plugin-Wiedergabe. Solange noch keine Spur da ist, geht sie an Cubase (startet dort die Wiedergabe). Das Plugin holt sich den Tastaturfokus nur, wenn man hineinklickt | Cubase bleibt bedienbar, sobald man außerhalb des Plugins klickt |
| Gesperrte Buttons | Neben jedem ausgegrauten „Weiter“ steht der Grund (z. B. „Spiele die Spur erst einmal in Cubase ab“) | Kein Rätselraten |

## Offen / nächste Schritte

- **Test in Cubase:** Bisher wurde nur unter Linux gebaut und ohne DAW getestet (simulierter Host). Windows- und macOS-Builds erzeugt die CI. Das ARA-Verhalten in Cubase muss noch praktisch geprüft werden. Das gilt besonders für die Mehrspur-Verknüpfung über ARA (1.3.0), die der simulierte Host nicht abdeckt.
- **Leertaste in Cubase:** Während Cubase spielt und das Plugin den Fokus hat, tappt die Leertaste; gestoppt wird dann über Cubase (Klick ins Projekt oder Transportfeld). Eventuell ist zusätzlich eine frei belegbare Tap-Taste sinnvoll.
- **Tap-Latenz kalibrieren:** Die Einstellung `tapOffsetMs` existiert, ist aber noch nicht in der Oberfläche. Das Einrasten auf Anschläge fängt den Versatz in der Praxis meist ab.
- **Ohne ARA:** Ein Offline-Export schneller als Echtzeit kann bei der Aufnahme Blöcke verlieren. Deshalb in Echtzeit abspielen.
