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

    struct SettingsHolder
    {
        SettingsHolder()
        {
            juce::PropertiesFile::Options options;
            options.applicationName = "Align my Time";
            options.filenameSuffix = "settings";
            options.folderName = "Align my Time";
            options.osxLibrarySubFolder = "Application Support";
            options.storageFormat = juce::PropertiesFile::storeAsXML;
            file = std::make_unique<juce::PropertiesFile> (options);
        }

        std::unique_ptr<juce::PropertiesFile> file;
    };
}

juce::PropertiesFile& appSettings()
{
    static SettingsHolder holder;
    return *holder.file;
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

bool markerDragNeedsCtrl()
{
    return appSettings().getBoolValue ("markerDragNeedsCtrl", true);
}

void setMarkerDragNeedsCtrl (bool needsCtrl)
{
    appSettings().setValue ("markerDragNeedsCtrl", needsCtrl);
    appSettings().saveIfNeeded();
}

juce::String versionString()
{
    return JucePlugin_VersionString;
}

} // namespace amt::plugin
