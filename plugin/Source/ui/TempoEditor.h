#pragma once

#include "../AlignSession.h"
#include "LookAndFeel.h"

namespace amt::plugin::ui
{

/** Callout behind the tempo chip: take the target tempo from the project, or type one in
    (always the case in the standalone app). */
class TempoEditor : public juce::Component
{
public:
    explicit TempoEditor (AlignSession& session);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void apply();
    void updateEnablement();

    AlignSession& session;
    ToggleRow fromProject { tr ("Tempo vom Projekt übernehmen") };
    juce::Slider bpm { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ComboBox numerator, denominator;
    juce::Rectangle<int> hostInfo, bpmLabel, signatureLabel;
};

} // namespace amt::plugin::ui
