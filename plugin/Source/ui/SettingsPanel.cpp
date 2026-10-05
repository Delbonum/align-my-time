#include "SettingsPanel.h"

namespace amt::plugin::ui
{

SettingsPanel::SettingsPanel (AlignMyTimeProcessor& p, std::function<void()> languageChanged, std::function<void()> closed)
    : processor (p), onLanguageChanged (std::move (languageChanged)), onClose (std::move (closed))
{
    setWantsKeyboardFocus (true);

    const juce::StringArray sections { tr ("Allgemein"), tr ("Bedienung"), tr ("Credits") };
    for (int i = 0; i < sections.size(); ++i)
    {
        auto* item = navItems.add (new juce::TextButton (sections[i]));
        item->getProperties().set ("nav", true);
        item->setWantsKeyboardFocus (false);
        item->onClick = [this, i] { showSection (i); };
        addAndMakeVisible (item);
    }

    language.setSelected (getLanguage() == Language::english ? 1 : 0);
    language.onChange = [this] (int i) {
        setLanguage (i == 1 ? Language::english : Language::german);
        if (onLanguageChanged)
            onLanguageChanged();
    };
    addChildComponent (language);

    tapKey.setColumns (3);
    tapKey.setSelected ((int) getTapKey());
    tapKey.onChange = [this] (int i) {
        setTapKey ((TapKey) i);
        capturingKey = false;
        updateCustomKeyButton();
        repaint();
    };
    addChildComponent (tapKey);

    setKind (customKey, ButtonKind::solid);
    customKey.setWantsKeyboardFocus (false);
    customKey.onClick = [this] {
        capturingKey = true;
        updateCustomKeyButton();
        grabKeyboardFocus();
    };
    addChildComponent (customKey);

    dragNeedsCtrl.setToggleState (markerDragNeedsCtrl(), juce::dontSendNotification);
    dragNeedsCtrl.setWantsKeyboardFocus (false);
    dragNeedsCtrl.onClick = [this] {
        setMarkerDragNeedsCtrl (dragNeedsCtrl.getToggleState());
        repaint();
    };
    addChildComponent (dragNeedsCtrl);

    close.setButtonText (tr ("Schließen"));
    close.getProperties().set ("icon", "check");
    setKind (close, ButtonKind::primary);
    close.onClick = [this] {
        if (onClose)
            onClose();
    };
    addAndMakeVisible (close);

    updateCustomKeyButton();
    showSection (0);
}

juce::Rectangle<int> SettingsPanel::card() const
{
    return getLocalBounds().withSizeKeepingCentre (780, 520);
}

void SettingsPanel::showSection (int index)
{
    currentSection = juce::jlimit (0, navItems.size() - 1, index);
    for (int i = 0; i < navItems.size(); ++i)
        navItems[i]->setToggleState (i == currentSection, juce::dontSendNotification);

    language.setVisible (currentSection == 0);
    tapKey.setVisible (currentSection == 1);
    dragNeedsCtrl.setVisible (currentSection == 1);
    capturingKey = false;
    updateCustomKeyButton();
    repaint();
}

void SettingsPanel::updateCustomKeyButton()
{
    customKey.setVisible (currentSection == 1 && getTapKey() == TapKey::custom);
    customKey.setButtonText (capturingKey ? tr ("Jetzt Taste drücken … (Esc bricht ab)")
                                          : tr ("Taste festlegen:") + " " + describeTapKey (TapKey::custom));
}

void SettingsPanel::resized()
{
    auto area = card().reduced (28);
    area.removeFromTop (48); // title

    auto bottom = area.removeFromBottom (40);
    close.setBounds (bottom.removeFromRight (160));
    area.removeFromBottom (12);

    // Navigation on the left, content on the right
    navArea = area.removeFromLeft (170);
    auto nav = navArea;
    for (auto* item : navItems)
    {
        item->setBounds (nav.removeFromTop (40));
        nav.removeFromTop (4);
    }
    area.removeFromLeft (28);
    content = area;

    // Allgemein
    auto general = content;
    languageLabel = general.removeFromTop (22);
    language.setBounds (general.removeFromTop (40).removeFromLeft (280));

    // Bedienung
    auto controls = content;
    tapKeyLabel = controls.removeFromTop (22);
    tapKey.setBounds (controls.removeFromTop (76));
    controls.removeFromTop (6);
    customKey.setBounds (controls.removeFromTop (34).removeFromLeft (360));
    controls.removeFromTop (4);
    tapKeyHint = controls.removeFromTop (92);
    controls.removeFromTop (8);
    markerLabel = controls.removeFromTop (22);
    dragNeedsCtrl.setBounds (controls.removeFromTop (32));
    markerHint = controls;
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

    // Divider between navigation and content
    g.setColour (colours::border);
    g.fillRect (juce::Rectangle<int> (navArea.getRight() + 13, navArea.getY(), 1, navArea.getHeight()));

    if (currentSection == 0)
    {
        drawSectionLabel (g, languageLabel, tr ("Sprache"));
        return;
    }

    if (currentSection == 1)
    {
        drawSectionLabel (g, tapKeyLabel, tr ("Tap-Taste"));
        drawSectionLabel (g, markerLabel, tr ("Marker bearbeiten (Schritt „Prüfen“)"));

        juce::String hint = tr ("Zusätzlich tappen immer Mausklick auf das TAP-Feld und MIDI (Note oder Sustain-Pedal).");
        if (! processor.isStandalone() && getTapKey() == TapKey::space)
            hint = tr ("In Cubase startet und stoppt die Leertaste auch die Wiedergabe. Damit sie nur tappt: in Cubase unter "
                       "Studio › Tastaturbefehle › Transport bei „Start/Stop“ die Leertaste durch Strg+Leertaste ersetzen.")
                   + " " + hint;
        else if (getTapKey() == TapKey::ctrlSpace)
            hint = tr ("Strg+Leertaste tappt nur, wenn sie in der DAW nicht selbst belegt ist.") + " " + hint;

        g.setColour (colours::muted);
        g.setFont (uiFont (12.5f));
        g.drawFittedText (hint, tapKeyHint, juce::Justification::topLeft, 5, 1.0f);

        g.drawFittedText (markerDragNeedsCtrl()
                              ? tr ("Klick in die Wellenform spielt ab dieser Stelle ab. Marker verschieben: Strg gedrückt halten und ziehen.")
                              : tr ("Marker direkt ziehen. Abspielen ab einer Stelle: Strg + Klick."),
                          markerHint, juce::Justification::topLeft, 3, 1.0f);
        return;
    }

    // Credits
    auto credits = content;
    auto line = [&] (const juce::String& label, const juce::String& value) {
        auto row = credits.removeFromTop (26);
        g.setColour (colours::muted);
        g.setFont (uiFont (13.0f));
        g.drawText (label, row.removeFromLeft (130), juce::Justification::centredLeft);
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
    if (capturingKey)
    {
        capturingKey = false;
        if (key != juce::KeyPress::escapeKey)
            setCustomTapKey (key);
        updateCustomKeyButton();
        repaint();
        return true;
    }

    if (key == juce::KeyPress::escapeKey && onClose)
    {
        onClose();
        return true;
    }
    return false;
}

} // namespace amt::plugin::ui
