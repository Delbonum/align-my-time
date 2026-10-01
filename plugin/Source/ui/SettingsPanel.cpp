#include "SettingsPanel.h"

namespace amt::plugin::ui
{

SettingsPanel::SettingsPanel (AlignMyTimeProcessor& p, std::function<void()> languageChanged, std::function<void()> closed)
    : processor (p), onLanguageChanged (std::move (languageChanged)), onClose (std::move (closed))
{
    setWantsKeyboardFocus (true);

    tabs.onChange = [this] (int i) { showTab (i); };
    addAndMakeVisible (tabs);

    language.setSelected (getLanguage() == Language::english ? 1 : 0);
    language.onChange = [this] (int i) {
        setLanguage (i == 1 ? Language::english : Language::german);
        if (onLanguageChanged)
            onLanguageChanged();
    };
    addChildComponent (language);

    tapKey.setSelected ((int) getTapKey());
    tapKey.onChange = [this] (int i) {
        setTapKey ((TapKey) i);
        repaint();
    };
    addChildComponent (tapKey);

    close.setButtonText (tr ("Schließen"));
    close.getProperties().set ("icon", "check");
    setKind (close, ButtonKind::primary);
    close.onClick = [this] {
        if (onClose)
            onClose();
    };
    addAndMakeVisible (close);

    showTab (0);
}

juce::Rectangle<int> SettingsPanel::card() const
{
    return getLocalBounds().withSizeKeepingCentre (640, 500);
}

void SettingsPanel::showTab (int index)
{
    currentTab = index;
    language.setVisible (index == 0);
    tapKey.setVisible (index == 0);
    repaint();
}

void SettingsPanel::resized()
{
    auto area = card().reduced (28);
    area.removeFromTop (40); // title
    tabs.setBounds (area.removeFromTop (40).removeFromLeft (280));
    area.removeFromTop (20);
    close.setBounds (area.removeFromBottom (40).removeFromRight (160));
    area.removeFromBottom (12);
    content = area;

    auto general = content;
    languageLabel = general.removeFromTop (22);
    language.setBounds (general.removeFromTop (40).removeFromLeft (280));
    general.removeFromTop (24);
    tapKeyLabel = general.removeFromTop (22);
    tapKey.setBounds (general.removeFromTop (40).removeFromLeft (400));
    general.removeFromTop (12);
    tapKeyHint = general;
}

void SettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.6f));

    const auto box = card().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (box, 14.0f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (box.reduced (0.5f), 14.0f, 1.0f);

    auto area = card().reduced (28);
    g.setColour (colours::text);
    g.setFont (uiFont (20.0f, true));
    g.drawText (tr ("Einstellungen"), area.removeFromTop (32), juce::Justification::centredLeft);

    if (currentTab == 0)
    {
        drawSectionLabel (g, languageLabel, tr ("Sprache"));
        drawSectionLabel (g, tapKeyLabel, tr ("Tap-Taste"));

        juce::String hint = tr ("Zusätzlich tappen immer Mausklick auf das TAP-Feld und MIDI (Note oder Sustain-Pedal).");
        if (! processor.isStandalone() && getTapKey() == TapKey::space)
            hint = tr ("In Cubase startet und stoppt die Leertaste auch die Wiedergabe. Damit sie nur tappt: in Cubase unter "
                       "Studio › Tastaturbefehle › Transport bei „Start/Stop“ die Leertaste durch Strg+Leertaste ersetzen. "
                       "Strg+Leertaste tappt im Plugin nie.")
                   + "\n\n" + hint;
        g.setColour (colours::muted);
        g.setFont (uiFont (13.0f));
        g.drawFittedText (hint, tapKeyHint, juce::Justification::topLeft, 8, 1.0f);
        return;
    }

    // Credits
    auto credits = content;
    auto line = [&] (const juce::String& label, const juce::String& value) {
        auto row = credits.removeFromTop (26);
        g.setColour (colours::muted);
        g.setFont (uiFont (13.0f));
        g.drawText (label, row.removeFromLeft (150), juce::Justification::centredLeft);
        g.setColour (colours::text);
        g.setFont (uiFont (13.0f, true));
        g.drawFittedText (value, row, juce::Justification::centredLeft, 1);
    };

    g.setColour (colours::accent);
    g.setFont (uiFont (18.0f, true));
    g.drawText ("Align my Time", credits.removeFromTop (28), juce::Justification::centredLeft);
    credits.removeFromTop (6);

    const auto format = processor.isStandalone() ? tr ("Standalone-App")
                                                 : juce::String (juce::AudioProcessor::getWrapperTypeDescription (processor.wrapperType))
                                                       + (processor.usesARA() ? " + ARA 2" : "");
    line (tr ("Version"), versionString());
    line (tr ("Entwickler"), "WiskundeKnobbel (Philippe Nix)");
    line (tr ("Format"), format);
    line (tr ("Build"), juce::String (__DATE__));
    credits.removeFromTop (14);

    g.setColour (colours::muted);
    g.setFont (uiFont (12.5f));
    g.drawFittedText (tr ("Verwendet: JUCE 8 (AGPLv3 / kommerzielle Lizenz), ARA SDK 2 von Celemony (Apache 2.0), "
                          "Signalsmith Stretch (MIT). VST ist eine Marke der Steinberg Media Technologies GmbH."),
                      credits, juce::Justification::topLeft, 8, 1.0f);
}

void SettingsPanel::mouseDown (const juce::MouseEvent& e)
{
    // Click on the dimmed backdrop closes the panel.
    if (! card().contains (e.getPosition()) && onClose)
        onClose();
}

bool SettingsPanel::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && onClose)
    {
        onClose();
        return true;
    }
    return false;
}

} // namespace amt::plugin::ui
