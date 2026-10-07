#pragma once

#include "Page.h"

namespace amt::plugin::ui
{

/** Step 1: listen to the whole track once and tap along. */
class TapPage : public Page
{
public:
    explicit TapPage (AlignMyTimeProcessor& p);
    ~TapPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh() override;
    bool handleKey (const juce::KeyPress&) override;
    void sessionChanged() override;

    /** Starts a tapping pass at `fromSeconds` (keeps earlier markers). */
    void startPass (double fromSeconds);

    /** A tap arrived (space, click or MIDI): flash the pad. */
    void flash();

private:
    class TapPad;

    void togglePlayback();
    void tap();
    juce::String nextTapLabel() const;
    /** Without ARA the track is recorded from the input; only then the export speed matters. */
    bool recordsFromInput() const;

    WaveformView wave;
    juce::TextButton rewind, playStop, undo, clearAll, next;
    ToggleRow leadIn { tr ("Vorlauf (2 s)") };
    ToggleRow realtimeExport { tr ("Echtzeit-Export") };
    SegmentedControl mode { { tr ("Jede Eins"), tr ("1 und 3"), tr ("Jede Zählzeit") } };
    std::unique_ptr<TapPad> pad;

    juce::Rectangle<int> leftColumn, rightColumn, footer, statsBox, hintBox;
};

} // namespace amt::plugin::ui
