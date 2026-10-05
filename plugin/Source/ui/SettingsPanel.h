#pragma once

#include "../PluginProcessor.h"
#include "LookAndFeel.h"

namespace amt::plugin::ui
{

/** "Einstellungen" overlay: navigation on the left (Allgemein / Bedienung / Credits),
    the selected section on the right. */
class SettingsPanel : public juce::Component
{
public:
    SettingsPanel (AlignMyTimeProcessor& processor, std::function<void()> onLanguageChanged, std::function<void()> onClose);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    /** For tests and screenshots: 0 = Allgemein, 1 = Bedienung, 2 = Credits. */
    void showSection (int index);

    /** True while waiting for the user to press the key for "Eigene Taste". */
    bool isCapturingKey() const { return capturingKey; }

private:
    juce::Rectangle<int> card() const;
    void updateCustomKeyButton();

    AlignMyTimeProcessor& processor;
    std::function<void()> onLanguageChanged, onClose;

    juce::OwnedArray<juce::TextButton> navItems;
    int currentSection = 0;

    // Allgemein
    SegmentedControl language { { "Deutsch", "English" } };

    // Bedienung
    SegmentedControl tapKey { { describeTapKey (TapKey::space), describeTapKey (TapKey::tab), describeTapKey (TapKey::t),
                                describeTapKey (TapKey::returnKey), describeTapKey (TapKey::ctrlSpace), tr ("Eigene Taste") } };
    juce::TextButton customKey;
    bool capturingKey = false;
    ToggleRow dragNeedsCtrl { tr ("Marker nur mit gedrückter Strg-Taste verschieben") };

    juce::TextButton close;

    juce::Rectangle<int> navArea, content, languageLabel, tapKeyLabel, tapKeyHint, markerLabel, markerHint;
};

} // namespace amt::plugin::ui
