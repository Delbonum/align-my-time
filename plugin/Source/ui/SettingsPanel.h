#pragma once

#include "../PluginProcessor.h"
#include "LookAndFeel.h"

namespace amt::plugin::ui
{

/** "Einstellungen": language, tap key, and the credits. Shown as an overlay over the editor. */
class SettingsPanel : public juce::Component
{
public:
    SettingsPanel (AlignMyTimeProcessor& processor, std::function<void()> onLanguageChanged, std::function<void()> onClose);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void showTab (int index);
    juce::Rectangle<int> card() const;

    AlignMyTimeProcessor& processor;
    std::function<void()> onLanguageChanged, onClose;

    SegmentedControl tabs { { tr ("Allgemein"), tr ("Credits") } };
    SegmentedControl language { { "Deutsch", "English" } };
    SegmentedControl tapKey { { tr ("Leertaste"), "Tab", "T", tr ("Eingabe") } };
    juce::TextButton close;

    juce::Rectangle<int> content, languageLabel, tapKeyLabel, tapKeyHint;
    int currentTab = 0;
};

} // namespace amt::plugin::ui
