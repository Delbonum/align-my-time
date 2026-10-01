#include "TempoEditor.h"
#include "Header.h"

namespace amt::plugin::ui
{

TempoEditor::TempoEditor (AlignSession& s) : session (s)
{
    const auto& settings = session.getSettings();

    fromProject.setToggleState (session.hasHostTempo() && ! settings.manualTempo, juce::dontSendNotification);
    fromProject.setEnabled (session.hasHostTempo());
    fromProject.onClick = [this] { apply(); };
    addAndMakeVisible (fromProject);

    bpm.setRange (20.0, 400.0, 0.01);
    bpm.setSkewFactorFromMidPoint (120.0);
    bpm.setTextValueSuffix (" BPM");
    bpm.setNumDecimalPlacesToDisplay (2);
    bpm.setTextBoxStyle (juce::Slider::TextBoxRight, false, 96, 28);
    bpm.setTitle (tr ("Ziel-Tempo"));
    bpm.setValue (session.usesManualTempo() ? settings.manualBpm : session.getProjectTempo().bpmAt (0.0), juce::dontSendNotification);
    bpm.onValueChange = [this] { apply(); };
    addAndMakeVisible (bpm);

    for (int n = 1; n <= 16; ++n)
        numerator.addItem (juce::String (n), n);
    for (int d : { 2, 4, 8, 16 })
        denominator.addItem (juce::String (d), d);

    const auto& sig = session.getProjectTempo().signatureAt (0.0);
    numerator.setSelectedId (settings.manualTempo || ! session.hasHostTempo() ? settings.manualNumerator : sig.numerator, juce::dontSendNotification);
    denominator.setSelectedId (settings.manualTempo || ! session.hasHostTempo() ? settings.manualDenominator : sig.denominator, juce::dontSendNotification);
    numerator.onChange = [this] { apply(); };
    denominator.onChange = [this] { apply(); };
    numerator.setTitle (tr ("Taktart"));
    addAndMakeVisible (numerator);
    addAndMakeVisible (denominator);

    updateEnablement();
    setSize (360, 200);
}

void TempoEditor::updateEnablement()
{
    const bool manual = ! fromProject.getToggleState();
    bpm.setEnabled (manual);
    numerator.setEnabled (manual);
    denominator.setEnabled (manual);
}

void TempoEditor::apply()
{
    const bool manual = ! fromProject.getToggleState();
    const double value = bpm.getValue();
    const int num = juce::jmax (1, numerator.getSelectedId());
    const int den = juce::jmax (1, denominator.getSelectedId());

    session.updateSettings ([=] (SessionSettings& s) {
        s.manualTempo = manual;
        s.manualBpm = value;
        s.manualNumerator = num;
        s.manualDenominator = den;
    });
    updateEnablement();
    repaint();
}

void TempoEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    hostInfo = area.removeFromTop (20);
    area.removeFromTop (8);
    fromProject.setBounds (area.removeFromTop (30));
    area.removeFromTop (12);
    auto row = area.removeFromTop (32);
    bpmLabel = row.removeFromLeft (70);
    bpm.setBounds (row);
    area.removeFromTop (12);
    row = area.removeFromTop (30);
    signatureLabel = row.removeFromLeft (70);
    numerator.setBounds (row.removeFromLeft (70));
    row.removeFromLeft (24);
    denominator.setBounds (row.removeFromLeft (70));
}

void TempoEditor::paint (juce::Graphics& g)
{
    g.setFont (uiFont (12.5f));
    g.setColour (colours::muted);
    const auto info = session.hasHostTempo() ? tr ("Projekt:") + " " + describeProjectTempo (*session.getHostTempo())
                                             : tr ("Kein Projekttempo verfügbar – bitte Tempo eingeben.");
    g.drawFittedText (info, hostInfo, juce::Justification::centredLeft, 1);

    g.setColour (colours::text);
    g.setFont (uiFont (13.0f));
    g.drawText (tr ("Tempo"), bpmLabel, juce::Justification::centredLeft);
    g.drawText (tr ("Taktart"), signatureLabel, juce::Justification::centredLeft);
    g.setFont (uiFont (16.0f, true));
    g.drawText ("/", juce::Rectangle<int> (numerator.getRight(), numerator.getY(), 24, numerator.getHeight()), juce::Justification::centred);
}

} // namespace amt::plugin::ui
