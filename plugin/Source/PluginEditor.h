#pragma once

#include "PluginProcessor.h"
#include "ui/Header.h"
#include "ui/LookAndFeel.h"
#include "ui/RenderPage.h"
#include "ui/ReviewPage.h"
#include "ui/TapPage.h"

namespace amt::plugin
{

/** One window, three steps: Tappen → Prüfen → Rendern. */
class AlignMyTimeEditor final : public juce::AudioProcessorEditor,
                                public juce::AudioProcessorEditorARAExtension,
                                private juce::ChangeListener,
                                private juce::Timer
{
public:
    explicit AlignMyTimeEditor (AlignMyTimeProcessor&);
    ~AlignMyTimeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void showStep (Step step);
    ui::Page* currentPage();

    AlignMyTimeProcessor& processor;
    ui::AmtLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    ui::Header header;
    ui::TapPage tapPage;
    ui::ReviewPage reviewPage;
    ui::RenderPage renderPage;
    Step shownStep = Step::tap;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlignMyTimeEditor)
};

} // namespace amt::plugin
