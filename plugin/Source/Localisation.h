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

/** Which key taps (besides mouse and MIDI). Space collides with the host's transport in some
    DAWs (Cubase), so it can be changed in the settings. */
enum class TapKey
{
    space,
    tab,
    t,
    returnKey
};

TapKey getTapKey();
void setTapKey (TapKey key);
juce::KeyPress tapKeyPress (TapKey key);
juce::String describeTapKey (TapKey key);

/** Settings shared by all instances and the standalone app (language, tap key ...). */
juce::PropertiesFile& appSettings();

/** Display version, e.g. "1.1.0". */
juce::String versionString();

} // namespace amt::plugin
