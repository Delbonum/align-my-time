#pragma once

#include "Page.h"
#include "TempoLane.h"

namespace amt::plugin::ui
{

/** Step 2: check the markers, choose how to align, listen before rendering. */
class ReviewPage : public Page, private juce::ScrollBar::Listener
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
    /** Plays original (A) or aligned (B) from a position in the recording (source time). */
    void startPreviewAt (double sourceSeconds);
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;
    void viewChanged();
    juce::String selectedInfo() const;

    WaveformView wave;
    TempoLane tempoLane;
    juce::ScrollBar scrollbar { false };
    juce::TextButton zoomOut, zoomIn, zoomFit;

    void stepUnit (int direction);

    juce::TextButton nudgeLeft, nudgeRight, addMarker, removeMarker, retap, barDown, barUp, unitBigger, unitSmaller;
    juce::TextButton applySuggestion;
    ToggleRow snap { tr ("An Transienten einrasten") };

    ChoiceCard stretchCard { tr ("Time-Stretch"), tr ("Jeder Takt wird gedehnt/gestaucht. Tonhöhe bleibt unverändert.") };
    ChoiceCard sliceCard { tr ("Schneiden + Verschieben"), tr ("An jedem Marker schneiden, aufs Raster schieben, Übergänge per Crossfade.") };
    SegmentedControl quality { { tr ("Rhythmisch"), tr ("Melodisch"), tr ("Komplex") } };
    juce::Slider crossfade { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    SegmentedControl ab { { tr ("Original"), tr ("Angepasst") } };
    juce::TextButton play;
    ToggleRow click { tr ("Klick im Projekttempo") };
    juce::Slider mix { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };

    juce::TextButton back, next;

    juce::Rectangle<int> leftColumn, middleColumn, rightColumn, footer, offsetBox, barRow, unitRow, mixRow, bannerRow;
    bool playWhenRendered = false;
    std::optional<double> playFromAfterRender;
};

} // namespace amt::plugin::ui
