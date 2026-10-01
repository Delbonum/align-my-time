#pragma once

#include "../AlignSession.h"
#include "LookAndFeel.h"

namespace amt::plugin::ui
{

/** Logo, the three steps (Tappen / Prüfen / Rendern) and the project tempo synced from the host. */
class Header : public juce::Component, private juce::ChangeListener
{
public:
    explicit Header (AlignSession& session);
    ~Header() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class StepButton;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void update();

    AlignSession& session;
    juce::OwnedArray<StepButton> steps;
    juce::Rectangle<int> tempoChip;
};

/** "Projekt 120,00 BPM · 4/4" (or "variabel" for tempo changes). */
juce::String describeProjectTempo (const TempoMap& tempo);

} // namespace amt::plugin::ui
