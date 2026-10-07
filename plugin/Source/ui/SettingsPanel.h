#pragma once

#include "../PluginProcessor.h"
#include "LookAndFeel.h"

namespace amt::plugin::ui
{

/** "Einstellungen" overlay: navigation on the left (Allgemein / Bedienung / Audio & MIDI / Credits),
    the selected section on the right. "Audio & MIDI" only exists in the standalone app. */
class SettingsPanel : public juce::Component
{
public:
    enum class Section { general, controls, audio, credits };

    SettingsPanel (AlignMyTimeProcessor& processor, std::function<void()> onLanguageChanged, std::function<void()> onClose);
    ~SettingsPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    /** Shows a section; "Audio & MIDI" falls back to "Allgemein" in the plug-in. */
    void showSection (Section section);
    Section getSection() const { return currentSection; }
    bool hasSection (Section section) const;

private:
    juce::Rectangle<int> card() const;
    void updateCustomKeyButton();

    AlignMyTimeProcessor& processor;
    std::function<void()> onLanguageChanged, onClose;

    std::vector<Section> sections;
    juce::OwnedArray<juce::TextButton> navItems;
    Section currentSection = Section::general;

    // Allgemein
    SegmentedControl language { { "Deutsch", "English" } };

    // Bedienung
    SegmentedControl tapKey { { describeTapKey (TapKey::space), describeTapKey (TapKey::tab), describeTapKey (TapKey::t),
                                describeTapKey (TapKey::returnKey), describeTapKey (TapKey::ctrlSpace), tr ("Eigene Taste") } };
    juce::TextButton customKey;
    bool capturingKey = false;
    ToggleRow dragNeedsCtrl { tr ("Marker nur mit gedrückter Strg-Taste verschieben") };
    juce::Slider tapOffset { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::TextButton applyMeasured;
    void updateMeasuredOffset();

    // Audio & MIDI (standalone)
    std::unique_ptr<juce::Component> deviceSelector;
    juce::Viewport deviceViewport;

    juce::TextButton close;

    juce::Rectangle<int> navArea, content, languageLabel, tapKeyLabel, tapKeyHint, markerLabel, markerHint, audioHint, offsetLabel, offsetHint;
};

} // namespace amt::plugin::ui
