#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace amt::plugin
{

enum class Language
{
    german,
    english
};

/** UI language; persisted in the app settings. Defaults to German on German systems, else English. */
Language getLanguage();
void setLanguage (Language language);

/** Translates a German UI text (UTF-8, as written in the sources) into the current language.
    Every key needs an entry in Translations.cpp; tools/check_translations.py verifies that. */
juce::String tr (const char* germanUtf8);

/** UTF-8 text that needs no translation (punctuation like "·" or "–"). */
inline juce::String utf8 (const char* text) { return juce::String::fromUTF8 (text); }

/** A number with the current language's decimal separator (120,00 / 120.00). */
juce::String formatNumber (double value, int decimals);

/** Which key taps (besides mouse and MIDI). Default is Tab: Space usually belongs to the host's
    transport (Cubase). Ctrl+Space never taps unless it is chosen here.
    The default must stay Tab (the developer's explicit decision; a smoke test guards it). */
enum class TapKey
{
    space,
    tab,
    t,
    returnKey,
    ctrlSpace,
    custom
};

constexpr int numTapKeyChoices = 6;

TapKey getTapKey();
void setTapKey (TapKey key);

/** The key used for TapKey::custom ("Eigene Taste"). */
juce::KeyPress getCustomTapKey();
void setCustomTapKey (const juce::KeyPress& key);

juce::KeyPress tapKeyPress (TapKey key);
juce::KeyPress currentTapKeyPress();
juce::String describeTapKey (TapKey key);

/** True if `key` is the current tap key (same key, same Ctrl/Alt state; Shift is ignored). */
bool matchesTapKey (const juce::KeyPress& key);

/** Tap latency compensation (ms) added to every tap: negative when taps come late
    (reaction time, MIDI or keyboard latency). Applies to all instances. */
double getTapOffsetMs();
void setTapOffsetMs (double milliseconds);

/** Review step: true = markers move only with Ctrl held, a plain click plays from there (default);
    false = plain drag moves markers, Ctrl+click plays from there. */
bool markerDragNeedsCtrl();
void setMarkerDragNeedsCtrl (bool needsCtrl);

/** Without ARA: while the track is recorded, a host export faster than real time (offline) is
    slowed down to real time, so no audio is lost. Off by default: most hosts keep up anyway. */
bool realtimeExportEnabled();
void setRealtimeExportEnabled (bool enabled);

/** Settings shared by all instances and the standalone app (language, tap key ...). */
juce::PropertiesFile& appSettings();

/** Tests: keep the settings in `file` instead of the user's settings. */
void useSettingsFile (const juce::File& file);

/** Translates the texts of JUCE's own components (audio device selector, "OK"/"Cancel") into
    the current language. Called by setLanguage() and when an editor opens. */
void applyJuceTranslations();

/** Display version, e.g. "1.1.0". */
juce::String versionString();

} // namespace amt::plugin
