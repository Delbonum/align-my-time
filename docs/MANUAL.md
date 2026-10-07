# Align My Time – Manual

This manual is also built into the app: **Help › Manual** (standalone), the **?** at the top right, or **F1**.

## Overview
<!-- id: overview -->

Align My Time brings a freely played recording onto the tempo of your project, without warp tables or hitpoint dialogs. You listen to the track once and tap along. The taps become markers, and every marker is pulled exactly onto its bar of the project grid.

The work happens in three steps, shown at the top centre of the window:

1. **Tap:** Play the track and press a key on every downbeat (or every beat).
2. **Review:** Check and correct the markers, choose the method, listen before and after.
3. **Render:** Write the result as a WAV file or replace it directly in the track.

Align My Time comes in two forms:

- **As a plug-in** (VST3, AU) in your DAW. With ARA 2 (e.g. Cubase, Studio One, Logic, Reaper) it reads the track's events directly. Without ARA it records the track while it plays.
- **As a standalone app** without a DAW. You load audio files, enter the target tempo and save the result as a WAV file. The app has a menu bar and saves projects (`.amtp`).

![Tap](screenshots/1-tappen.png)

## Quick start: standalone app
<!-- id: quickstart-app -->

1. **File › Load audio file …** (`Ctrl+I`), or simply drag the file into the window. WAV, AIFF, FLAC, Ogg and MP3 are supported.
2. Click the tempo display at the top right and enter the **target tempo and time signature** (also **Edit › Target tempo and time signature …**, `Ctrl+T`).
3. Under "I tap on …", choose whether you tap on every one, on 1 and 3, or on every beat.
4. Press **Play & tap** (or the tap key, `Tab` by default). After a 2-second lead-in it starts: press the tap key in time until the end of the piece.
5. **Next: Review markers**. Check the markers, choose the method, listen with "Aligned".
6. **Next: Render** › **Export as file …** The app asks where to save the WAV file.
7. **File › Save project** (`Ctrl+S`) if you want to continue later.

Set up the audio output under **Edit › Audio and MIDI settings …**.

## Quick start: plug-in in your DAW
<!-- id: quickstart-plugin -->

### With ARA (Cubase, Nuendo, Studio One, Logic, Reaper …)

1. Select the track's events and open Align My Time as an ARA extension. In Cubase: **Audio › Extensions › Align My Time**. All events of the track arrive in the plug-in as *one* big event.
2. Time signature and tempo come from the project. The top right shows e.g. "Project 120.00 BPM · 4/4".
3. Tap, review and render as described below. Under "Render" choose **As a new track** (WAV file to drag in) or **Replace in this track**.

### Without ARA (as an insert effect)

1. Put Align My Time as an insert on the track.
2. Play the project. The plug-in records the track meanwhile, and you can tap along in the same pass. Every further playback in step 1 adds to the recording. Stopping early or starting in the middle is fine.
3. Review and render as with ARA.

At the top left next to "Source:" a small badge shows where the audio comes from: **ARA**, **Insert** or **File**.

Instead of the track you can also use an audio file in the plug-in: **Load file …** or drag the file into the window. **Back to the track** switches back.

## Step 1: Tap
<!-- id: tap -->

In the first step you listen to the recording and press a key in time. Every tap becomes a marker.

### How to tap

- **Tap key:** `Tab` by default, because the space bar usually controls the DAW's transport. Choose other keys under **Settings › Controls**.
- **Mouse:** click the large TAP pad.
- **MIDI:** every note and the sustain pedal tap, e.g. a foot switch. In the standalone app, enable the MIDI input under **Audio & MIDI**.

### Playback

- **Play & tap** plays the track in the plug-in, with a 2-second **lead-in** if the switch is on. The TAP pad counts down.
- In the plug-in you can also play in the DAW instead and tap along.
- In the standalone app the space bar starts and stops playback when it is not the tap key.
- **Re-tap from the start** starts a new pass. The old markers are replaced; `Ctrl+Z` brings them back.

### What do I tap on?

Under **I tap on …** choose **Every one**, **1 and 3** or **Every beat**. You can still change this in the next step without tapping again. If you counted differently than selected, Align My Time suggests the matching grid itself.

### Tapping mistakes

- A missed beat is no problem: just keep tapping. Gaps are filled in automatically (those markers are labelled "filled in automatically").
- Double taps are detected and removed.
- `Backspace` or **Delete last tap** takes back the last tap, **Delete all markers** starts over.
- Every marker snaps to the nearest attack in the recording (up to 70 ms away) while "Snap to transients" is on.

On the right under **Live** you see the tapped tempo, the target tempo, the fluctuation and how many markers are set.

## Step 2: Review
<!-- id: review -->

Here you check the markers and decide how the track is aligned.

![Review](screenshots/2-pruefen.png)

### Waveform and markers

- **Click** into the waveform to play from there (original or aligned, depending on the preview switch).
- **Ctrl+drag** moves a marker. Under **Settings › Controls** this can be swapped (then a plain drag moves, and Ctrl+click plays).
- **Double-click** adds a marker.
- A click on a marker selects it. `←`/`→` move it by 5 ms, with `Shift` by 1 ms. `↑`/`↓` select the previous or next marker, `Delete` removes it.
- The **mouse wheel** (or pinch on a trackpad) zooms around the mouse position, **Shift+wheel** scrolls. There is also the scroll bar and the buttons −, + and **All**.

Below the waveform the **tempo curve** shows the tapped tempo per bar and the target tempo as a dashed line. Outliers stand out right away.

### Editing markers (left column)

- The box between the arrows shows how far the marker lies from the tap.
- **Snap to transients:** markers snap to the nearest attack.
- **Marker** adds a marker in the middle of the selected bar, **Delete** removes the selected one.
- **Re-tap from here** repeats only the rest from the selected marker on.
- **First marker = bar** sets the project bar the first marker lands on. Normally Align My Time picks the nearest bar.
- **1 marker =** changes how far apart two taps are: 2 bars, 1 bar, ½ bar, 1 beat or ½ beat. If the tapped tempo clearly fits another grid better, a hint with a button appears at the top, e.g. "Count as ½ bar".

### Even out sloppy taps

The slider (Off to 100 %) pulls the markers towards a smooth tempo curve. Single misplaced taps are corrected, real tempo changes (e.g. a ritardando) are kept. With "Snap to transients" the detected attacks help: a corrected marker lands on the actual beat.

The setting is non-destructive. The tapped markers stay saved, **Off** (double-click the slider) restores them exactly. Markers placed by hand stay where they are.

### How it is aligned

- **Time-stretch:** every bar is stretched or squeezed, the pitch stays the same. Attacks are taken unchanged from the original, so drums and plucks stay crisp. Quality: **Rhythmic** (drums, bass, comping), **Melodic** (vocals, leads) or **Complex** (pads, whole mixes).
- **Cut + move:** the track is cut at every marker, every piece is moved onto the grid and joined with a short **crossfade** (2–50 ms). The audio itself stays untouched. Ideal for drums, especially with several tracks.

### Preview

- Choose **Original** or **Aligned** and press **Play** (or `Space`). If the result has not been computed yet, Align My Time renders it first.
- **Click in project tempo** plays a metronome along. The **Mix** slider balances track and click (double-click = centre).

## Step 3: Render
<!-- id: render -->

At the top you see before and after on the project grid. The markers of the result sit exactly on the bar lines.

![Render](screenshots/3-rendern.png)

### Plug-in

- **As a new track:** Align My Time writes a WAV file (24 bit) to the folder `Music/Align My Time`. Drag the tile on the right onto a new track. With **File from project start (bar 1)** the file starts at the beginning of the project, so you simply drop it at bar 1.
- **Replace in this track:** the track plays the aligned version from now on. This is non-destructive: **Restore original** switches back. To make it permanent, use your DAW's function, e.g. "Render in Place" in Cubase.

### Standalone app

**Export as file …** (or **File › Export result …**, `Ctrl+E`) asks where to save and writes a WAV file (24 bit). The **Track name** field suggests the file name.

**Listen** plays the result. If anything changes after rendering, the result says "outdated, please render again".

## Multitrack
<!-- id: multitrack -->

Several tracks of one recording, e.g. all microphones of a drum kit, are aligned with the same markers and therefore stay in phase. Tapping, snapping and preview use the sum of all tracks.

- **With ARA:** button **Multitrack** next to "Source" › tick the other tracks. Align My Time must run on them as an ARA extension too. "Replace in this track" then replaces on all of them.
- **Everywhere (also standalone):** **Multitrack › Add audio files …** (standalone: **File › Add more tracks …**, `Ctrl+Shift+I`), or drag several files into the window at once. The first file becomes the main track.
- **As a new track** or the export writes one WAV file per track, all of the same length and starting at the same position.

For drums, **Cut + move** is the safest choice: the tracks then stay in phase to the sample.

## Target tempo and time signature
<!-- id: tempo -->

The top right shows what the track is aligned to, e.g. "Project 120.00 BPM · 4/4" or "Target 96.00 BPM · 3/4". A click opens the settings:

- **Use the project tempo** (plug-in only): tempo, tempo changes and time signatures come from the DAW.
- Otherwise enter **tempo** and **time signature** by hand. The standalone app always uses the entered tempo.

The tempo is saved with the project and can be undone with `Ctrl+Z`.

## Projects (standalone app)
<!-- id: projects -->

The standalone app saves your work as a project file with the extension **.amtp**. The **File** menu offers:

| Command | What happens |
|---|---|
| New project (`Ctrl+N`) | Empty project. With unsaved changes the app asks first. |
| Open project … (`Ctrl+O`) | Opens an `.amtp` file. Also by drag & drop into the window. |
| Open recent | The last ten projects. |
| Save project (`Ctrl+S`) | Saves under the current name (the first time like "Save as"). |
| Save project as … (`Ctrl+Shift+S`) | Saves under a new name. |

A project contains references to the audio files, all markers (tapped and edited), grid, target tempo and all settings. The audio itself is not stored in the project.

- If the audio files are next to the project or in a subfolder, the app finds them even after you move the whole folder or copy it to another computer.
- If a file is missing, the app tells you which one. Markers and settings are loaded anyway: load the audio file again and save.
- Unsaved changes show as a `*` in the window title. Before "New", "Open" and "Quit" the app asks.
- On the next start the app reopens the project you used last.

The file format is described in `docs/PROJEKTFORMAT.md`.

## Settings
<!-- id: settings -->

Open the settings with the gear at the top right, in the standalone app also via **Edit › Settings …** (`Ctrl+,`). They apply to all projects and all plug-in instances.

- **General:** language (Deutsch or English).
- **Controls:** tap key (Space, Tab, T, Return, Ctrl+Space or a key of your own) and whether markers only move with Ctrl held.
- **Audio & MIDI** (standalone only): audio driver, output, sample rate, buffer size and the MIDI inputs for a foot switch or keyboard. No audio input is needed. **Edit › Audio and MIDI settings …** leads straight there.
- **Credits:** version, developer and libraries used. **Help › Credits** leads straight there.

**Space bar in Cubase:** if the space bar should tap, set "Start/Stop" under *Studio › Key Commands › Transport* to Ctrl+Space instead of Space. **Ctrl+Space** only taps in Align My Time when you choose it as the tap key.

## Keyboard shortcuts
<!-- id: shortcuts -->

On the Mac use `Cmd` instead of `Ctrl`.

### Everywhere

| Key | Function |
|---|---|
| Tap key (default `Tab`) | Tap (step 1) |
| `Ctrl+Z` | Undo (markers, grid, evening out, method, tempo) |
| `Ctrl+Y` or `Ctrl+Shift+Z` | Redo |
| `F1` | Manual |
| `Esc` | Stop playback, close settings or manual |

### Step 1: Tap

| Key | Function |
|---|---|
| `Space` | Standalone: start/stop playback (when it is not the tap key) |
| `Backspace` | Delete last tap |
| `Return` | Go on to "Review" |

### Step 2: Review

| Key | Function |
|---|---|
| `Space` | Start/stop preview |
| `←` / `→` | Move the selected marker by 5 ms |
| `Shift+←` / `Shift+→` | Move by 1 ms |
| `↑` / `↓` | Select previous / next marker |
| `Delete` / `Backspace` | Delete the selected marker |
| Click / `Ctrl`+drag | Play from here / move marker |
| Double-click | Add marker |
| Wheel / `Shift`+wheel | Zoom / scroll |

### Step 3: Render

| Key | Function |
|---|---|
| `Space` | Listen to the result / stop |

### Standalone app (menu bar)

| Key | Function |
|---|---|
| `Ctrl+N` / `Ctrl+O` | New project / open project |
| `Ctrl+S` / `Ctrl+Shift+S` | Save / save as |
| `Ctrl+I` / `Ctrl+Shift+I` | Load audio file / add more tracks |
| `Ctrl+E` | Export result |
| `Ctrl+T` | Target tempo and time signature |
| `Ctrl+,` | Settings |
| `Ctrl+1` / `Ctrl+2` / `Ctrl+3` | Step Tap / Review / Render |
| `Ctrl++` / `Ctrl+-` / `Ctrl+0` | Zoom in / zoom out / show all |
| `Ctrl+Q` | Quit |

## Tips and troubleshooting
<!-- id: troubleshooting -->

- **Space stops the DAW instead of tapping:** choose another tap key (default `Tab`) or move Start/Stop in the DAW to another key. On Windows, Align My Time hears the tap key even when the DAW catches it.
- **"Waiting for the audio …" (ARA):** some DAWs release the audio only after a moment. Align My Time retries every second. If that does not help: **Reload track**.
- **Insert mode: "No signal reached the plug-in during playback":** is the track muted, or is the plug-in behind a fader at zero? The plug-in needs the track's signal.
- **The tapped tempo does not fit the project:** in "Review", change the grid at **1 marker =** or accept the suggestion at the top.
- **A bar sounds smeared after aligning:** check the marker (often a missed attack), turn on **Snap to transients** or try **Cut + move**.
- **Several tracks sound phasey:** use **Cut + move**. All tracks must come from the same recording and start at the same time.
- **Finding exported files:** in the plug-in under `Music/Align My Time` (button **Show in folder**). In the standalone app wherever you saved them.
- **Recordings in insert mode** are kept in `%APPDATA%\Align My Time\Captures` (Windows) or `~/Library/Application Support/Align My Time/Captures` (macOS). They belong to DAW projects and can be deleted once the project is no longer needed.
