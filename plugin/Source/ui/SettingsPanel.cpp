#include "SettingsPanel.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace amt::plugin::ui
{

SettingsPanel::SettingsPanel (AlignMyTimeProcessor& p, std::function<void()> languageChanged, std::function<void()> closed)
    : processor (p), onLanguageChanged (std::move (languageChanged)), onClose (std::move (closed))
{
    setWantsKeyboardFocus (true);

    sections = { Section::general, Section::controls };
    if (processor.getDeviceManager() != nullptr)
        sections.push_back (Section::audio);
    sections.push_back (Section::credits);

    for (auto section : sections)
    {
        const auto label = section == Section::general    ? tr ("Allgemein")
                           : section == Section::controls ? tr ("Bedienung")
                           : section == Section::audio    ? tr ("Audio & MIDI")
                                                          : tr ("Credits");
        auto* item = navItems.add (new juce::TextButton (label));
        item->getProperties().set ("nav", true);
        item->setWantsKeyboardFocus (false);
        item->onClick = [this, section] { showSection (section); };
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

    // Tap latency: taps usually come a little late (reaction, keyboard or MIDI latency).
    tapOffset.setRange (-100.0, 100.0, 1.0);
    tapOffset.setTextValueSuffix (" ms");
    tapOffset.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 24);
    tapOffset.setDoubleClickReturnValue (true, 0.0);
    tapOffset.setValue (getTapOffsetMs(), juce::dontSendNotification);
    tapOffset.setTitle (tr ("Tap-Ausgleich"));
    tapOffset.setTooltip (tr ("Wird zu jedem neuen Tap addiert. Negativ, wenn deine Taps zu spät kommen. Doppelklick = 0."));
    tapOffset.onValueChange = [this] {
        setTapOffsetMs (tapOffset.getValue());
        updateMeasuredOffset();
    };
    addChildComponent (tapOffset);

    setKind (applyMeasured, ButtonKind::solid);
    applyMeasured.setButtonText (tr ("Übernehmen"));
    applyMeasured.setWantsKeyboardFocus (false);
    applyMeasured.onClick = [this] {
        if (auto measured = processor.getSession().getMeasuredTapOffset())
            tapOffset.setValue (juce::roundToInt (getTapOffsetMs() + *measured * 1000.0)); // also saves it
    };
    addChildComponent (applyMeasured);

    if (auto* manager = processor.getDeviceManager())
    {
        // Output and MIDI input (foot switch); the app works with files, so no audio input.
        deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent> (*manager, 0, 0, 1, 2, true, false, true, false);
        deviceViewport.setViewedComponent (deviceSelector.get(), false);
        deviceViewport.setScrollBarsShown (true, false);
        addChildComponent (deviceViewport);
    }

    close.setButtonText (tr ("Schließen"));
    close.getProperties().set ("icon", "check");
    setKind (close, ButtonKind::primary);
    close.onClick = [this] {
        if (onClose)
            onClose();
    };
    addAndMakeVisible (close);

    updateCustomKeyButton();
    showSection (Section::general);
}

SettingsPanel::~SettingsPanel()
{
    deviceViewport.setViewedComponent (nullptr, false);
}

juce::Rectangle<int> SettingsPanel::card() const
{
    return getLocalBounds().withSizeKeepingCentre (juce::jmin (getWidth() - 32, 860), juce::jmin (getHeight() - 32, 580));
}

bool SettingsPanel::hasSection (Section section) const
{
    return std::find (sections.begin(), sections.end(), section) != sections.end();
}

void SettingsPanel::showSection (Section section)
{
    currentSection = hasSection (section) ? section : Section::general;
    for (size_t i = 0; i < sections.size(); ++i)
        navItems[(int) i]->setToggleState (sections[i] == currentSection, juce::dontSendNotification);

    language.setVisible (currentSection == Section::general);
    tapKey.setVisible (currentSection == Section::controls);
    dragNeedsCtrl.setVisible (currentSection == Section::controls);
    tapOffset.setVisible (currentSection == Section::controls);
    updateMeasuredOffset();
    deviceViewport.setVisible (currentSection == Section::audio && deviceSelector != nullptr);
    capturingKey = false;
    updateCustomKeyButton();
    repaint();
}

void SettingsPanel::updateMeasuredOffset()
{
    const auto measured = processor.getSession().getMeasuredTapOffset();
    applyMeasured.setVisible (currentSection == Section::controls && measured.has_value() && std::abs (*measured) >= 0.002);
    repaint();
}

void SettingsPanel::updateCustomKeyButton()
{
    customKey.setVisible (currentSection == Section::controls && getTapKey() == TapKey::custom);
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
    tapKeyHint = controls.removeFromTop (70);
    controls.removeFromTop (8);
    offsetLabel = controls.removeFromTop (22);
    tapOffset.setBounds (controls.removeFromTop (30).removeFromLeft (360));
    controls.removeFromTop (4);
    auto offsetRow = controls.removeFromTop (34);
    applyMeasured.setBounds (offsetRow.removeFromRight (130).reduced (0, 2));
    offsetRow.removeFromRight (12);
    offsetHint = offsetRow;
    controls.removeFromTop (8);
    markerLabel = controls.removeFromTop (22);
    dragNeedsCtrl.setBounds (controls.removeFromTop (32));
    markerHint = controls;

    // Audio & MIDI
    auto audio = content;
    audioHint = audio.removeFromBottom (44);
    audio.removeFromBottom (8);
    deviceViewport.setBounds (audio);
    if (deviceSelector != nullptr)
        deviceSelector->setSize (audio.getWidth() - deviceViewport.getScrollBarThickness() - 4, 600);
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

    if (currentSection == Section::general)
    {
        drawSectionLabel (g, languageLabel, tr ("Sprache"));
        return;
    }

    if (currentSection == Section::audio)
    {
        g.setColour (colours::muted);
        g.setFont (uiFont (12.5f));
        g.drawFittedText (tr ("Für einen MIDI-Fußschalter oder ein Keyboard zum Tappen den MIDI-Eingang aktivieren. "
                              "Ein Audioeingang wird nicht gebraucht: Die App arbeitet mit Audiodateien."),
                          audioHint, juce::Justification::topLeft, 3, 1.0f);
        return;
    }

    if (currentSection == Section::controls)
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
        g.drawFittedText (hint, tapKeyHint, juce::Justification::topLeft, 4, 1.0f);

        drawSectionLabel (g, offsetLabel, tr ("Tap-Ausgleich"));
        juce::String offsetText = tr ("Tipp: Nach dem Tappen mit „An Transienten einrasten“ misst Align My Time hier, wie weit deine Taps neben den Anschlägen lagen.");
        if (const auto measured = processor.getSession().getMeasuredTapOffset())
        {
            const int ms = juce::roundToInt (std::abs (*measured) * 1000.0);
            offsetText = ms < 2 ? tr ("Deine Taps in diesem Projekt lagen im Schnitt genau auf den Anschlägen.")
                                : tr ("Deine Taps in diesem Projekt lagen im Schnitt") + " " + juce::String (ms) + " ms "
                                      + (*measured < 0.0 ? tr ("nach dem Anschlag.") : tr ("vor dem Anschlag."));
        }
        g.setColour (colours::muted);
        g.setFont (uiFont (12.5f));
        g.drawFittedText (offsetText, offsetHint, juce::Justification::centredLeft, 2, 1.0f);

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
    g.drawText ("Align My Time", credits.removeFromTop (28), juce::Justification::centredLeft);
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
