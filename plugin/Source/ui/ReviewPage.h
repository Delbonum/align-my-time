#pragma once

#include "Page.h"
#include "TempoLane.h"

namespace amt::plugin::ui
{

/** Step 2: check the markers, choose how to align, listen before rendering. */
class ReviewPage : public Page
{
public:
    explicit ReviewPage (AlignMyTimeProcessor& p);

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh() override;
    bool handleKey (const juce::KeyPress&) override;
    void sessionChanged() override;

    /** Asks the editor to go back to step 1 and re-tap from a position. */
    std::function<void (double)> onRetapFrom;

private:
    void togglePreview();
    void startPreviewNow();
    juce::String selectedInfo() const;

    WaveformView wave;
    TempoLane tempoLane;

    juce::TextButton nudgeLeft, nudgeRight, addMarker, removeMarker, retap, barDown, barUp;
    ToggleRow snap { "An Transienten einrasten" };

    ChoiceCard stretchCard { "Time-Stretch", de ("Jeder Takt wird gedehnt/gestaucht. Tonhöhe bleibt unverändert.") };
    ChoiceCard sliceCard { "Schneiden + Verschieben", de ("An jedem Marker schneiden, aufs Raster schieben, Übergänge per Crossfade.") };
    SegmentedControl quality { { "Rhythmisch", "Melodisch", "Komplex" } };
    juce::Slider crossfade { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    SegmentedControl ab { { "Original", "Angepasst" } };
    juce::TextButton play;
    ToggleRow click { "Klick im Projekttempo" };

    juce::TextButton back, next;

    juce::Rectangle<int> leftColumn, middleColumn, rightColumn, footer, offsetBox, barRow;
    bool playWhenRendered = false;
};

} // namespace amt::plugin::ui
