#include "PluginEditor.h"

namespace amt::plugin
{

//==============================================================================
bool TapKeyPoller::isSupported()
{
   #if JUCE_WINDOWS
    return true; // GetAsyncKeyState sees the key no matter who has the focus
   #else
    return false; // elsewhere JUCE only knows keys its own windows received
   #endif
}

void TapKeyPoller::hiResTimerCallback()
{
    if (! enabled.load())
    {
        wasDown = true; // a key held while enabling must not count as a tap
        return;
    }

    const bool down = juce::KeyPress::isKeyCurrentlyDown (keyCode.load());
    const auto mods = juce::ModifierKeys::getCurrentModifiersRealtime();
    const bool modifiersMatch = (mods.isCtrlDown() || mods.isCommandDown()) == needsCtrl.load() && mods.isAltDown() == needsAlt.load();
    if (down && ! wasDown && modifiersMatch && processor.isAudioRunning())
    {
        processor.pushKeyTap (processor.getAudiblePositionSeconds());
    }
    wasDown = down;
}

//==============================================================================
AlignMyTimeEditor::AlignMyTimeEditor (AlignMyTimeProcessor& p)
    : juce::AudioProcessorEditor (&p), juce::AudioProcessorEditorARAExtension (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);
    applyJuceTranslations();
    buildUi();

    processor.onTapFromMidi = [this] {
        if (tapPage != nullptr)
            tapPage->flash();
    };
    processor.getSession().addChangeListener (this);

    setWantsKeyboardFocus (true);
    addMouseListener (this, true);

    // Resizable with fixed proportions; the size is remembered (plug-in and app separately).
    // Read before the limits are set: setting them already resizes (and would store that size).
    const auto scaleKey = processor.isStandalone() ? "standaloneScale" : "editorScale";
    const double scale = juce::jlimit (0.5, 2.0, appSettings().getDoubleValue (scaleKey, 1.0));
    setResizable (true, false);
    setResizeLimits (designWidth / 2, designHeight / 2, designWidth * 2, designHeight * 2);
    if (auto* sizeLimits = getConstrainer())
        sizeLimits->setFixedAspectRatio ((double) designWidth / (double) designHeight);
    setSize (juce::roundToInt (designWidth * scale), juce::roundToInt (designHeight * scale));
    rememberSize = true;
    startTimerHz (30);
}

AlignMyTimeEditor::~AlignMyTimeEditor()
{
    processor.onTapFromMidi = nullptr;
    processor.getSession().removeChangeListener (this);
    settings.reset();
    manual.reset();
    header.reset();
    tapPage.reset();
    reviewPage.reset();
    renderPage.reset();
    setLookAndFeel (nullptr);
}

void AlignMyTimeEditor::buildUi()
{
    // Rebuilt from scratch when the language changes: every text is created in a constructor.
    settings.reset();
    manual.reset();
    header = std::make_unique<ui::Header> (processor.getSession());
    tapPage = std::make_unique<ui::TapPage> (processor);
    reviewPage = std::make_unique<ui::ReviewPage> (processor);
    renderPage = std::make_unique<ui::RenderPage> (processor);

    header->onOpenSettings = [this] { showSettings(); };
    header->onOpenManual = [this] { showManual(); };
    reviewPage->onRetapFrom = [this] (double seconds) {
        processor.getSession().setStep (Step::tap);
        tapPage->startPass (seconds);
    };

    addAndMakeVisible (*header);
    addChildComponent (*tapPage);
    addChildComponent (*reviewPage);
    addChildComponent (*renderPage);

    showStep (processor.getSession().getStep());
    resized();
}

void AlignMyTimeEditor::showSettings (ui::SettingsPanel::Section section)
{
    closeOverlay();

    // Captured outside the lambdas: MSVC resolves 'this' inside nested init-captures wrongly.
    juce::Component::SafePointer<AlignMyTimeEditor> safe (this);

    auto languageChanged = [safe] {
        juce::MessageManager::callAsync ([safe] {
            if (safe != nullptr)
            {
                safe->buildUi();
                safe->showSettings(); // stay in the settings, now in the new language
            }
        });
    };

    auto closed = [safe] {
        juce::MessageManager::callAsync ([safe] {
            if (safe != nullptr)
                safe->closeOverlay();
        });
    };

    settings = std::make_unique<ui::SettingsPanel> (processor, std::move (languageChanged), std::move (closed));
    settings->showSection (section);
    addAndMakeVisible (*settings);
    placeOverlay (*settings);
    settings->grabKeyboardFocus();
}

void AlignMyTimeEditor::showManual (const juce::String& chapterId)
{
    closeOverlay();

    juce::Component::SafePointer<AlignMyTimeEditor> safe (this);
    manual = std::make_unique<ui::ManualView> ([safe] {
        juce::MessageManager::callAsync ([safe] {
            if (safe != nullptr)
                safe->closeOverlay();
        });
    });
    if (chapterId.isNotEmpty())
        manual->showChapter (chapterId);
    addAndMakeVisible (*manual);
    placeOverlay (*manual);
    manual->grabKeyboardFocus();
}

bool AlignMyTimeEditor::closeOverlay()
{
    if (settings == nullptr && manual == nullptr)
        return false;

    settings.reset();
    manual.reset();
    grabKeyboardFocus();
    repaint();
    return true;
}

void AlignMyTimeEditor::openTempoEditor()
{
    closeOverlay();
    header->openTempoEditor();
}

void AlignMyTimeEditor::exportResult()
{
    closeOverlay();
    processor.stopPreview();
    processor.getSession().setStep (Step::render);
    showStep (Step::render);
    renderPage->exportResult();
}

void AlignMyTimeEditor::exportTempoMap()
{
    closeOverlay();
    processor.stopPreview();
    processor.getSession().setStep (Step::render);
    showStep (Step::render);
    renderPage->exportTempoMap();
}

void AlignMyTimeEditor::zoomReview (int direction)
{
    if (shownStep == Step::review)
        reviewPage->zoom (direction);
}

void AlignMyTimeEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::colours::panel);
}

void AlignMyTimeEditor::paintOverChildren (juce::Graphics& g)
{
    if (! fileDragActive)
        return;

    auto area = getLocalBounds().reduced (16).toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (getLocalBounds());
    g.setColour (ui::colours::accent);
    juce::Path outline;
    outline.addRoundedRectangle (area, 16.0f);
    const float dashes[] = { 10.0f, 6.0f };
    juce::PathStrokeType (2.0f).createDashedStroke (outline, outline, dashes, 2);
    g.fillPath (outline);
    g.setFont (ui::uiFont (22.0f, true));
    g.drawText (tr ("Audiodatei hier ablegen"), area, juce::Justification::centred);
}

void AlignMyTimeEditor::resized()
{
    // Everything is laid out at the design size, then scaled (and centred if the proportions differ).
    uiScale = juce::jmax (0.1f, juce::jmin ((float) getWidth() / (float) designWidth, (float) getHeight() / (float) designHeight));
    uiTransform = juce::AffineTransform::scale (uiScale)
                      .translated (((float) getWidth() - (float) designWidth * uiScale) * 0.5f,
                                   ((float) getHeight() - (float) designHeight * uiScale) * 0.5f);

    juce::Rectangle<int> area (designWidth, designHeight);
    if (header != nullptr)
    {
        header->setBounds (area.removeFromTop (56));
        header->setTransform (uiTransform);
    }
    for (auto* page : { (juce::Component*) tapPage.get(), (juce::Component*) reviewPage.get(), (juce::Component*) renderPage.get() })
    {
        if (page != nullptr)
        {
            page->setBounds (area);
            page->setTransform (uiTransform);
        }
    }
    if (settings != nullptr)
        placeOverlay (*settings);
    if (manual != nullptr)
        placeOverlay (*manual);

    if (rememberSize && getWidth() > 0)
        appSettings().setValue (processor.isStandalone() ? "standaloneScale" : "editorScale", (double) getWidth() / designWidth);
}

void AlignMyTimeEditor::placeOverlay (juce::Component& overlay)
{
    overlay.setBounds (0, 0, designWidth, designHeight);
    overlay.setTransform (uiTransform);
}

ui::Page* AlignMyTimeEditor::currentPage()
{
    switch (shownStep)
    {
        case Step::review: return reviewPage.get();
        case Step::render: return renderPage.get();
        case Step::tap:
        default: return tapPage.get();
    }
}

void AlignMyTimeEditor::showStep (Step step)
{
    shownStep = step;
    tapPage->setVisible (step == Step::tap);
    reviewPage->setVisible (step == Step::review);
    renderPage->setVisible (step == Step::render);
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

    // The poller only listens while tapping makes sense and the host app is in front.
    const auto tapKey = currentTapKeyPress();
    keyPoller.keyCode.store (tapKey.getKeyCode());
    keyPoller.needsCtrl.store (tapKey.getModifiers().isCtrlDown() || tapKey.getModifiers().isCommandDown());
    keyPoller.needsAlt.store (tapKey.getModifiers().isAltDown());
    keyPoller.enabled.store (TapKeyPoller::isSupported() && settings == nullptr && manual == nullptr && shownStep == Step::tap && isShowing()
                             && processor.isAudioRunning() && juce::Process::isForegroundProcess());
}

void AlignMyTimeEditor::mouseDown (const juce::MouseEvent& e)
{
    // A click anywhere in the plug-in brings the keyboard back (tap key), but we never take
    // it on our own: when you click into the DAW, the DAW gets its keys.
    if (e.eventComponent != nullptr && ! e.eventComponent->getWantsKeyboardFocus() && settings == nullptr && manual == nullptr)
        grabKeyboardFocus();
}

bool AlignMyTimeEditor::keyPressed (const juce::KeyPress& key)
{
    if (settings != nullptr)
        return settings->keyPressed (key);
    if (manual != nullptr)
        return manual->keyPressed (key);

    if (key == juce::KeyPress::F1Key)
    {
        showManual();
        return true;
    }

    // Undo / redo of marker edits and alignment settings (Ctrl+Z, Ctrl+Y, Ctrl+Shift+Z).
    const auto mods = key.getModifiers();
    if ((mods.isCtrlDown() || mods.isCommandDown()) && ! mods.isAltDown() && ! matchesTapKey (key))
    {
        const auto code = juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) key.getKeyCode());
        auto& session = processor.getSession();
        if (code == 'z' && ! mods.isShiftDown())
        {
            session.undo();
            return true;
        }
        if (code == 'y' || (code == 'z' && mods.isShiftDown()))
        {
            session.redo();
            return true;
        }
    }

    // While the poller listens (Windows), it does the tapping: swallow the key event so it isn't
    // counted twice.
    if (shownStep == Step::tap && matchesTapKey (key) && keyPoller.enabled.load())
        return true;

    if (auto* page = currentPage())
        return page->handleKey (key);
    return false;
}

//==============================================================================
bool AlignMyTimeEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    if (onProjectFileDropped != nullptr && files.size() == 1 && juce::File (files[0]).hasFileExtension (".amtp"))
        return true;

    const auto patterns = juce::StringArray::fromTokens (AlignMyTimeProcessor::audioFileWildcard(), ";", {});
    for (const auto& f : files)
        for (const auto& pattern : patterns)
            if (juce::File (f).getFileName().matchesWildcard (pattern.trim(), true))
                return true;
    return false;
}

void AlignMyTimeEditor::fileDragEnter (const juce::StringArray&, int, int)
{
    fileDragActive = true;
    repaint();
}

void AlignMyTimeEditor::fileDragExit (const juce::StringArray&)
{
    fileDragActive = false;
    repaint();
}

void AlignMyTimeEditor::filesDropped (const juce::StringArray& files, int, int)
{
    fileDragActive = false;
    repaint();
    if (onProjectFileDropped != nullptr && files.size() == 1 && juce::File (files[0]).hasFileExtension (".amtp"))
    {
        onProjectFileDropped (juce::File (files[0]));
        return;
    }

    if (! files.isEmpty())
    {
        processor.getSession().setStep (Step::tap);
        processor.loadAudioFile (juce::File (files[0]));

        // Several files at once: a multitrack recording, aligned together.
        juce::Array<juce::File> extra;
        for (int i = 1; i < files.size(); ++i)
            extra.add (juce::File (files[i]));
        processor.addExtraFiles (extra);
    }
}

} // namespace amt::plugin
