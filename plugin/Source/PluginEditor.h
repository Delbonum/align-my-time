#pragma once

#include "PluginProcessor.h"
#include "ui/Header.h"
#include "ui/LookAndFeel.h"
#include "ui/ManualView.h"
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
    std::atomic<int> keyCode { juce::KeyPress::tabKey };
    std::atomic<bool> needsCtrl { false }, needsAlt { false };

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

    /** Opens the settings overlay at a section (gear button, standalone menu). */
    void showSettings (ui::SettingsPanel::Section section = ui::SettingsPanel::Section::general);
    ui::SettingsPanel* getSettingsPanel() { return settings.get(); }

    /** Opens the manual, optionally at a chapter id (F1, "?" button, standalone menu). */
    void showManual (const juce::String& chapterId = {});
    ui::ManualView* getManual() { return manual.get(); }

    /** Closes the settings or the manual; true if one was open. */
    bool closeOverlay();

    void openTempoEditor();

    /** Rebuilds every page, e.g. after the language was switched from the standalone menu. */
    void rebuildUi() { buildUi(); }

    /** Review step: +1 zooms in, -1 out, 0 shows everything. */
    void zoomReview (int direction);

    /** Goes to "Rendern" and exports the result (rendering first if needed). */
    void exportResult();

    /** Writes the recording's tempo as a MIDI file ("Tempo-Map exportieren"). */
    void exportTempoMap();

    /** The window can be resized; the whole UI is laid out at the design size and scaled. */
    static constexpr int designWidth = 1120, designHeight = 720;
    float getUiScale() const { return uiScale; }

    /** Standalone: a project file (.amtp) was dropped onto the window. */
    std::function<void (const juce::File&)> onProjectFileDropped;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void buildUi();
    void showStep (Step step);
    void placeOverlay (juce::Component& overlay);
    ui::Page* currentPage();

    AlignMyTimeProcessor& processor;
    ui::AmtLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    std::unique_ptr<ui::Header> header;
    std::unique_ptr<ui::TapPage> tapPage;
    std::unique_ptr<ui::ReviewPage> reviewPage;
    std::unique_ptr<ui::RenderPage> renderPage;
    std::unique_ptr<ui::SettingsPanel> settings;
    std::unique_ptr<ui::ManualView> manual;
    TapKeyPoller keyPoller { processor };
    Step shownStep = Step::tap;
    bool fileDragActive = false;
    float uiScale = 1.0f;
    bool rememberSize = false;
    juce::AffineTransform uiTransform;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlignMyTimeEditor)
};

} // namespace amt::plugin
