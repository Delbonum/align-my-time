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
        value = juce::jlimit (0, 3, appSettings().getIntValue ("tapKey", 0));
        currentTapKey.store (value);
    }
    return (TapKey) value;
}

void setTapKey (TapKey key)
{
    currentTapKey.store ((int) key);
    appSettings().setValue ("tapKey", (int) key);
    appSettings().saveIfNeeded();
}

juce::KeyPress tapKeyPress (TapKey key)
{
    switch (key)
    {
        case TapKey::tab:       return juce::KeyPress (juce::KeyPress::tabKey);
        case TapKey::t:         return juce::KeyPress ('t');
        case TapKey::returnKey: return juce::KeyPress (juce::KeyPress::returnKey);
        case TapKey::space:
        default:                return juce::KeyPress (juce::KeyPress::spaceKey);
    }
}

juce::String describeTapKey (TapKey key)
{
    switch (key)
    {
        case TapKey::tab:       return "Tab";
        case TapKey::t:         return "T";
        case TapKey::returnKey: return tr ("Eingabe");
        case TapKey::space:
        default:                return tr ("Leertaste");
    }
}

juce::String versionString()
{
    return JucePlugin_VersionString;
}

} // namespace amt::plugin
