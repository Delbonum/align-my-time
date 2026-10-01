#include "PluginEditor.h"

namespace amt::plugin
{

AlignMyTimeEditor::AlignMyTimeEditor (AlignMyTimeProcessor& p)
    : juce::AudioProcessorEditor (&p), juce::AudioProcessorEditorARAExtension (&p), processor (p),
      header (p.getSession()), tapPage (p), reviewPage (p), renderPage (p)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (header);
    addChildComponent (tapPage);
    addChildComponent (reviewPage);
    addChildComponent (renderPage);

    reviewPage.onRetapFrom = [this] (double seconds) {
        processor.getSession().setStep (Step::tap);
        tapPage.startPass (seconds);
    };
    processor.onTapFromMidi = [this] { tapPage.flash(); };

    processor.getSession().addChangeListener (this);
    showStep (processor.getSession().getStep());

    setWantsKeyboardFocus (true);
    setSize (1120, 720);
    startTimerHz (30);
}

AlignMyTimeEditor::~AlignMyTimeEditor()
{
    processor.onTapFromMidi = nullptr;
    processor.getSession().removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void AlignMyTimeEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::colours::panel);
}

void AlignMyTimeEditor::resized()
{
    auto area = getLocalBounds();
    header.setBounds (area.removeFromTop (56));
    for (auto* page : { (juce::Component*) &tapPage, (juce::Component*) &reviewPage, (juce::Component*) &renderPage })
        page->setBounds (area);
}

ui::Page* AlignMyTimeEditor::currentPage()
{
    switch (shownStep)
    {
        case Step::review: return &reviewPage;
        case Step::render: return &renderPage;
        case Step::tap:
        default: return &tapPage;
    }
}

void AlignMyTimeEditor::showStep (Step step)
{
    shownStep = step;
    tapPage.setVisible (step == Step::tap);
    reviewPage.setVisible (step == Step::review);
    renderPage.setVisible (step == Step::render);
    grabKeyboardFocus();
}

void AlignMyTimeEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (processor.getSession().getStep() != shownStep)
        showStep (processor.getSession().getStep());
}

void AlignMyTimeEditor::timerCallback()
{
    if (auto* page = currentPage())
        page->refresh();

    // Keep the space bar for tapping even after clicking around.
    if (shownStep == Step::tap && ! hasKeyboardFocus (true) && isShowing())
        grabKeyboardFocus();
}

bool AlignMyTimeEditor::keyPressed (const juce::KeyPress& key)
{
    if (auto* page = currentPage())
        return page->handleKey (key);
    return false;
}

} // namespace amt::plugin
