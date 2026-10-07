#include "Localisation.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <string>
#include <unordered_map>

namespace amt::plugin
{

// German key -> English text. Defined in Translations.cpp.
const std::unordered_map<std::string, const char*>& englishTranslations();

namespace
{
    std::atomic<int> currentLanguage { -1 };
    std::atomic<int> currentTapKey { -1 };

    juce::PropertiesFile::Options settingsOptions()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "Align My Time";
        options.filenameSuffix = "settings";
        options.folderName = "Align My Time";
        options.osxLibrarySubFolder = "Application Support";
        options.storageFormat = juce::PropertiesFile::storeAsXML;
        return options;
    }

    std::unique_ptr<juce::PropertiesFile>& settingsFile()
    {
        static std::unique_ptr<juce::PropertiesFile> file;
        return file;
    }

    /** German texts for JUCE's own components (audio device selector, alert buttons). */
    const char* const juceGerman = R"juce(
"OK" = "OK"
"Cancel" = "Abbrechen"
"Yes" = "Ja"
"No" = "Nein"
"none" = "keins"
"Audio device type:" = "Audiotreiber:"
"Output:" = "Ausgang:"
"Input:" = "Eingang:"
"Device:" = "Gerät:"
"Test" = "Test"
"Plays a test tone" = "Spielt einen Testton"
"Sample rate:" = "Samplerate:"
"Audio buffer size:" = "Puffergröße:"
"Active output channels:" = "Aktive Ausgangskanäle:"
"Active input channels:" = "Aktive Eingangskanäle:"
"(no audio output channels found)" = "(keine Audioausgänge gefunden)"
"(no audio input channels found)" = "(keine Audioeingänge gefunden)"
"Active MIDI inputs:" = "Aktive MIDI-Eingänge:"
"No MIDI inputs available" = "Keine MIDI-Eingänge vorhanden"
"MIDI Output:" = "MIDI-Ausgang:"
"Control Panel" = "Treiber-Einstellungen"
"Opens the device's own control panel" = "Öffnet die Einstellungen des Audiotreibers"
"Reset Device" = "Gerät zurücksetzen"
"Resets the audio interface - sometimes needed after changing a device's properties in its custom control panel" = "Setzt das Audio-Interface zurück, z. B. nach Änderungen in den Treiber-Einstellungen"
"Show advanced settings..." = "Erweiterte Einstellungen …"
"Error when trying to open audio device!" = "Das Audiogerät kann nicht geöffnet werden!"
"Bluetooth MIDI" = "Bluetooth-MIDI"
"Scan for bluetooth MIDI devices" = "Nach Bluetooth-MIDI-Geräten suchen"
)juce";
}

juce::PropertiesFile& appSettings()
{
    auto& file = settingsFile();
    if (file == nullptr)
        file = std::make_unique<juce::PropertiesFile> (settingsOptions());
    return *file;
}

void useSettingsFile (const juce::File& file)
{
    settingsFile() = std::make_unique<juce::PropertiesFile> (file, settingsOptions());
    currentLanguage.store (-1);
    currentTapKey.store (-1);
}

void applyJuceTranslations()
{
    juce::LocalisedStrings::setCurrentMappings (getLanguage() == Language::german
                                                    ? new juce::LocalisedStrings (juce::String::fromUTF8 (juceGerman), false)
                                                    : nullptr);
}

Language getLanguage()
{
    int value = currentLanguage.load();
    if (value < 0)
    {
        const bool germanSystem = juce::SystemStats::getUserLanguage().startsWithIgnoreCase ("de");
        value = appSettings().getIntValue ("language", germanSystem ? 0 : 1);
        currentLanguage.store (value);
    }
    return value == 1 ? Language::english : Language::german;
}

void setLanguage (Language language)
{
    currentLanguage.store (language == Language::english ? 1 : 0);
    appSettings().setValue ("language", language == Language::english ? 1 : 0);
    appSettings().saveIfNeeded();
    applyJuceTranslations();
}

juce::String tr (const char* germanUtf8)
{
    if (getLanguage() == Language::german)
        return juce::String::fromUTF8 (germanUtf8);

    const auto& table = englishTranslations();
    const auto it = table.find (germanUtf8);
    if (it != table.end())
        return juce::String::fromUTF8 (it->second);

    jassertfalse; // missing translation: add it to Translations.cpp
    return juce::String::fromUTF8 (germanUtf8);
}

juce::String formatNumber (double value, int decimals)
{
    const auto text = juce::String (value, decimals);
    return getLanguage() == Language::german ? text.replaceCharacter ('.', ',') : text;
}

TapKey getTapKey()
{
    int value = currentTapKey.load();
    if (value < 0)
    {
        // "tapKeyChoice" replaces 1.1's "tapKey" so that everyone starts with the new default (Tab).
        value = juce::jlimit (0, numTapKeyChoices - 1, appSettings().getIntValue ("tapKeyChoice", (int) TapKey::tab));
        currentTapKey.store (value);
    }
    return (TapKey) value;
}

void setTapKey (TapKey key)
{
    currentTapKey.store ((int) key);
    appSettings().setValue ("tapKeyChoice", (int) key);
    appSettings().saveIfNeeded();
}

juce::KeyPress getCustomTapKey()
{
    const auto stored = juce::KeyPress::createFromDescription (appSettings().getValue ("customTapKey", "F5"));
    return stored.isValid() ? stored : juce::KeyPress (juce::KeyPress::F5Key);
}

void setCustomTapKey (const juce::KeyPress& key)
{
    appSettings().setValue ("customTapKey", key.getTextDescription());
    appSettings().saveIfNeeded();
}

juce::KeyPress tapKeyPress (TapKey key)
{
    switch (key)
    {
        case TapKey::space:     return juce::KeyPress (juce::KeyPress::spaceKey);
        case TapKey::t:         return juce::KeyPress ('t');
        case TapKey::returnKey: return juce::KeyPress (juce::KeyPress::returnKey);
        case TapKey::ctrlSpace: return juce::KeyPress (juce::KeyPress::spaceKey, juce::ModifierKeys::ctrlModifier, 0);
        case TapKey::custom:    return getCustomTapKey();
        case TapKey::tab:
        default:                return juce::KeyPress (juce::KeyPress::tabKey);
    }
}

juce::KeyPress currentTapKeyPress()
{
    return tapKeyPress (getTapKey());
}

juce::String describeTapKey (TapKey key)
{
    switch (key)
    {
        case TapKey::space:     return tr ("Leertaste");
        case TapKey::t:         return "T";
        case TapKey::returnKey: return tr ("Eingabe");
        case TapKey::ctrlSpace: return tr ("Strg+Leertaste");
        case TapKey::custom:    return getCustomTapKey().getTextDescriptionWithIcons();
        case TapKey::tab:
        default:                return "Tab";
    }
}

bool matchesTapKey (const juce::KeyPress& key)
{
    const auto tapKey = currentTapKeyPress();
    const auto normalise = [] (int code) { return code >= 'A' && code <= 'Z' ? code + ('a' - 'A') : code; };
    const auto ctrl = [] (const juce::ModifierKeys& m) { return m.isCtrlDown() || m.isCommandDown(); };
    return normalise (key.getKeyCode()) == normalise (tapKey.getKeyCode())
           && ctrl (key.getModifiers()) == ctrl (tapKey.getModifiers())
           && key.getModifiers().isAltDown() == tapKey.getModifiers().isAltDown();
}

double getTapOffsetMs()
{
    return juce::jlimit (-150.0, 150.0, appSettings().getDoubleValue ("tapOffsetMs", 0.0));
}

void setTapOffsetMs (double milliseconds)
{
    appSettings().setValue ("tapOffsetMs", juce::jlimit (-150.0, 150.0, milliseconds));
    appSettings().saveIfNeeded();
}

bool markerDragNeedsCtrl()
{
    return appSettings().getBoolValue ("markerDragNeedsCtrl", true);
}

void setMarkerDragNeedsCtrl (bool needsCtrl)
{
    appSettings().setValue ("markerDragNeedsCtrl", needsCtrl);
    appSettings().saveIfNeeded();
}

bool realtimeExportEnabled()
{
    return appSettings().getBoolValue ("realtimeExport", false);
}

void setRealtimeExportEnabled (bool enabled)
{
    appSettings().setValue ("realtimeExport", enabled);
    appSettings().saveIfNeeded();
}

juce::String versionString()
{
    return JucePlugin_VersionString;
}

} // namespace amt::plugin
