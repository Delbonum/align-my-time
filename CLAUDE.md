# Align my Time

DAW plug-in (VST3/AU, ARA 2, JUCE 8): tap along to a track, then time-stretch or cut it onto the project tempo. UI and docs are German.

## Layout
- `core/`: C++17, no JUCE. TempoMap, tap clean-up/Markers, OnsetDetector, WarpMap, Alignment (grid planning, TapUnit suggestion), Renderers (Signalsmith Stretch + attack splicing; slices + crossfade).
- `plugin/Source/`: JUCE plug-in. `AlignSession` = message-thread model of the three steps; `PluginProcessor` = audio thread, non-ARA input recording (merged per host pass), MIDI taps, ARA retry; `PlaybackRenderer`/`SourceLoader`/`DocumentController` = ARA; `ui/` one file per page.
- `tests/CoreTests.cpp`: framework-free core tests. `plugin/tests/PluginSmokeTest.cpp`: headless end-to-end test with a fake host; writes `docs/screenshots/*.png` when given a folder.

## Commands
- Core only: `cmake -S . -B build-core -DAMT_BUILD_PLUGIN=OFF && cmake --build build-core && ./build-core/tests/amt_core_tests`
- Full: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build && xvfb-run -a ctest --test-dir build`
- Screenshots: `xvfb-run -a ./build/plugin/AlignMyTimeSmokeTest docs/screenshots`

## Conventions
- JUCE code style (space before parentheses, `camelCase`, Allman braces); the build uses JUCE's strict warning flags: keep it warning-free (no float `==`, no shadowing).
- Non-ASCII UI strings: `de ("…")` (UTF-8 source), never a plain `juce::String ("ä")`.
- Audio thread: no allocation/locking; hand data over via `SharedObject`/`SharedClip` (freed on the message thread) or FIFOs.
- Disabled buttons must explain why (`AlignMyTimeProcessor::getBlockingReason`).
- Add a test for every behaviour change (core test, or smoke test for host interaction).
