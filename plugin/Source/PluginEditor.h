#pragma once

#include "PluginProcessor.h"
#include "ui/Header.h"
#include "ui/LookAndFeel.h"
#include "ui/RenderPage.h"
#include "ui/ReviewPage.h"
#include "ui/SettingsPanel.h"
#include "ui/TapPage.h"

namespace amt::plugin
{

/** Watches the tap key with the OS's asynchronous key state (Windows), so a tap is caught even
    when the host keeps the key for itself. Runs on its own high-resolution timer thread. */
class TapKeyPoller : private juce::HighResolutionTimer
{
public:
    explicit TapKeyPoller (AlignMyTimeProcessor& p) : processor (p) { startTimer (2); }
    ~TapKeyPoller() override { stopTimer(); }

    /** Set from the message thread: only poll while tapping makes sense. */
    std::atomic<bool> enabled { false };
    std::atomic<int> keyCode { juce::KeyPress::spaceKey };

    static bool isSupported();

private:
    void hiResTimerCallback() override;

    AlignMyTimeProcessor& processor;
    bool wasDown = false;
};

/** One window, three steps: Tappen → Prüfen → Rendern. */
class AlignMyTimeEditor final : public juce::AudioProcessorEditor,
                                public juce::AudioProcessorEditorARAExtension,
                                public juce::FileDragAndDropTarget,
                                private juce::ChangeListener,
                                private juce::Timer
{
public:
    explicit AlignMyTimeEditor (AlignMyTimeProcessor&);
    ~AlignMyTimeEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

    /** For tests: open the settings overlay. */
    void showSettings();

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void buildUi();
    void showStep (Step step);
    ui::Page* currentPage();

    AlignMyTimeProcessor& processor;
    ui::AmtLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    std::unique_ptr<ui::Header> header;
    std::unique_ptr<ui::TapPage> tapPage;
    std::unique_ptr<ui::ReviewPage> reviewPage;
    std::unique_ptr<ui::RenderPage> renderPage;
    std::unique_ptr<ui::SettingsPanel> settings;
    TapKeyPoller keyPoller { processor };
    Step shownStep = Step::tap;
    bool fileDragActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlignMyTimeEditor)
};

} // namespace amt::plugin
