#pragma once

#include "../AlignSession.h"
#include "LookAndFeel.h"

namespace amt::plugin::ui
{

/** Logo, the three steps (Tappen / Prüfen / Rendern), the target tempo (click to edit) and the settings. */
class Header : public juce::Component, private juce::ChangeListener
{
public:
    explicit Header (AlignSession& session);
    ~Header() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    std::function<void()> onOpenSettings;

private:
    class StepButton;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void update();

    AlignSession& session;
    juce::OwnedArray<StepButton> steps;
    juce::TextButton settingsButton;
    juce::Rectangle<int> tempoChip;
};

/** "Projekt 120,00 BPM · 4/4" (or "variabel" for tempo changes). */
juce::String describeProjectTempo (const TempoMap& tempo);

} // namespace amt::plugin::ui
