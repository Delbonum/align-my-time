#include "TapPage.h"

namespace amt::plugin::ui
{

//==============================================================================
class TapPage::TapPad : public juce::Button
{
public:
    TapPad() : juce::Button ("TAP")
    {
        setTitle ("Tap (Leertaste)");
        setWantsKeyboardFocus (false);
        setTriggeredOnMouseDown (true); // taps must not wait for mouse-up
    }

    juce::String topLabel, countdown;
    int beatsPerBar = 4;
    float beatPhase = -1.0f; // 0..beatsPerBar, or < 0 when unknown
    float flashAmount = 0.0f;
    bool active = false;

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (6.0f);
        auto pad = bounds.removeFromTop (bounds.getHeight() - 28.0f);

        // Glow while armed, brighter on each tap.
        g.setColour (colours::accent.withAlpha (0.12f + 0.35f * flashAmount));
        g.fillRoundedRectangle (pad.expanded (6.0f), 22.0f);
        g.setColour (colours::accentDark.interpolatedWith (colours::accent, 0.25f * flashAmount + (down ? 0.15f : highlighted ? 0.05f : 0.0f)));
        g.fillRoundedRectangle (pad, 18.0f);
        g.setColour (active ? colours::accent : colours::border);
        g.drawRoundedRectangle (pad, 18.0f, 2.0f);

        auto content = pad.reduced (12.0f);
        g.setColour (active ? colours::accent : colours::muted);
        g.setFont (uiFont (13.0f, true).withExtraKerningFactor (0.12f));
        g.drawText (countdown.isNotEmpty() ? countdown : topLabel, content.removeFromTop (content.getHeight() * 0.28f), juce::Justification::centredBottom);

        g.setColour (colours::text);
        g.setFont (uiFont (64.0f, true).withExtraKerningFactor (0.04f));
        g.drawText ("TAP", content.removeFromTop (content.getHeight() * 0.62f), juce::Justification::centred);

        // Key hint
        g.setFont (monoFont (12.5f));
        const juce::String key ("Leertaste");
        const juce::String rest = juce::String::fromUTF8 ("oder Klick / MIDI-Fu\xc3\x9fschalter");
        const float keyWidth = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), key) + 20.0f;
        g.setFont (uiFont (12.5f));
        const float restWidth = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), rest);
        auto hint = content.withSizeKeepingCentre (keyWidth + 8.0f + restWidth, 22.0f);
        auto keyBox = hint.removeFromLeft (keyWidth);
        g.setColour (colours::panel2);
        g.fillRoundedRectangle (keyBox, 5.0f);
        g.setColour (colours::border);
        g.drawRoundedRectangle (keyBox, 5.0f, 1.0f);
        g.setColour (colours::text);
        g.setFont (monoFont (12.5f));
        g.drawText (key, keyBox, juce::Justification::centred);
        g.setColour (colours::muted);
        g.setFont (uiFont (12.5f));
        g.drawText (rest, hint.withTrimmedLeft (8.0f), juce::Justification::centredLeft);

        // Beat dots under the pad
        const float dotsWidth = beatsPerBar * 24.0f;
        auto dots = bounds.withSizeKeepingCentre (dotsWidth, 14.0f).withY (bounds.getCentreY() - 7.0f);
        const int currentBeat = beatPhase >= 0.0f ? (int) beatPhase : -1;
        for (int i = 0; i < beatsPerBar; ++i)
        {
            auto cell = dots.removeFromLeft (24.0f);
            const bool on = i == currentBeat;
            const float size = (i == 0 || on) ? 14.0f : 10.0f;
            g.setColour (on ? colours::accent : colours::border);
            g.fillEllipse (cell.withSizeKeepingCentre (size, size));
        }
    }
};

//==============================================================================
TapPage::TapPage (AlignMyTimeProcessor& p) : Page (p), pad (std::make_unique<TapPad>())
{
    wave.setDimAfterPlayhead (true);
    wave.setPlaceholder ("Hier erscheint die Spur als ein einziges Event.");
    addAndMakeVisible (wave);

    configureButton (rewind, {}, "rewind", ButtonKind::solid);
    rewind.setTooltip ("Zum Anfang (Stopp)");
    rewind.onClick = [this] { processor.stopPreview(); };
    configureButton (playStop, "Abspielen & tappen", "play", ButtonKind::solid);
    playStop.onClick = [this] { togglePlayback(); };
    addAndMakeVisible (rewind);
    addAndMakeVisible (playStop);

    leadIn.onClick = [this] { session.updateSettings ([this] (SessionSettings& s) { s.leadIn = leadIn.getToggleState(); }); };
    addAndMakeVisible (leadIn);

    mode.onChange = [this] (int i) {
        session.updateSettings ([i] (SessionSettings& s) { s.tapMode = i == 0 ? TapMode::downbeats : TapMode::beats; });
    };
    addAndMakeVisible (mode);

    pad->onClick = [this] { tap(); };
    addAndMakeVisible (*pad);

    configureButton (undo, juce::String::fromUTF8 ("Letzten Tap l\xc3\xb6schen"), "undo", ButtonKind::ghost);
    undo.setTooltip ("Backspace");
    undo.onClick = [this] { session.undoLastTap(); };
    configureButton (clearAll, juce::String::fromUTF8 ("Alle Marker l\xc3\xb6schen"), "trash", ButtonKind::ghost);
    clearAll.onClick = [this] { session.clearMarkers(); };
    configureButton (next, juce::String::fromUTF8 ("Weiter: Marker pr\xc3\xbc" "fen"), "arrow-r", ButtonKind::primary, true);
    next.onClick = [this] {
        processor.stopPreview();
        session.setStep (Step::review);
    };
    addAndMakeVisible (undo);
    addAndMakeVisible (clearAll);
    addAndMakeVisible (next);

    for (auto* c : { (juce::Component*) &rewind, (juce::Component*) &playStop, (juce::Component*) &leadIn, (juce::Component*) &undo,
                     (juce::Component*) &clearAll, (juce::Component*) &next })
        c->setWantsKeyboardFocus (false); // keep the space bar for tapping
}

void TapPage::resized()
{
    auto area = layoutFrame (footer);
    wave.setBounds (area.removeFromTop (180).reduced (24, 0));
    area.removeFromTop (20);
    area = area.reduced (24, 0).withTrimmedBottom (16);

    leftColumn = area.removeFromLeft (300);
    rightColumn = area.removeFromRight (300);
    area.reduce (20, 0);

    auto left = leftColumn;
    left.removeFromTop (22); // label
    auto transport = left.removeFromTop (52);
    rewind.setBounds (transport.removeFromLeft (52));
    transport.removeFromLeft (8);
    playStop.setBounds (transport);
    left.removeFromTop (12);
    leadIn.setBounds (left.removeFromTop (32));
    left.removeFromTop (14 + 22);
    mode.setBounds (left.removeFromTop (40));

    pad->setBounds (area.withSizeKeepingCentre (area.getWidth(), juce::jmin (area.getHeight(), 270)));

    auto right = rightColumn;
    right.removeFromTop (22);
    statsBox = right.removeFromTop (150);
    right.removeFromTop (14);
    hintBox = right.removeFromTop (60);

    auto f = footer.reduced (24, 12);
    undo.setBounds (f.removeFromLeft (200));
    f.removeFromLeft (8);
    clearAll.setBounds (f.removeFromLeft (190));
    next.setBounds (f.removeFromRight (230));
}

void TapPage::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);

    drawSectionLabel (g, leftColumn.withHeight (16), "Wiedergabe");
    drawSectionLabel (g, mode.getBounds().translated (0, -24).withHeight (16), juce::String::fromUTF8 ("Ich tippe auf \xe2\x80\xa6"));

    // Time signature under the mode switch
    const auto& tempo = session.getProjectTempo();
    const auto& sig = tempo.signatureAt (0.0);
    auto sigRow = mode.getBounds().translated (0, 52).withHeight (20);
    g.setColour (colours::muted);
    g.setFont (uiFont (13.0f));
    g.drawText ("Taktart (vom Projekt)", sigRow, juce::Justification::centredLeft);
    g.setColour (colours::text);
    g.setFont (monoFont (13.0f, true));
    g.drawText (juce::String (sig.numerator) + "/" + juce::String (sig.denominator), sigRow, juce::Justification::centredRight);

    // Live stats
    drawSectionLabel (g, rightColumn.withHeight (16), "Live");
    g.setColour (colours::background);
    g.fillRoundedRectangle (statsBox.toFloat(), 10.0f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (statsBox.toFloat().reduced (0.5f), 10.0f, 1.0f);

    const auto& plan = session.getPlan();
    const bool hasTempo = session.getMarkers().size() >= 2;
    auto box = statsBox.reduced (16, 12);

    auto row = box.removeFromTop (34);
    g.setColour (colours::muted);
    g.setFont (uiFont (12.5f));
    g.drawText ("Getappt", row, juce::Justification::centredLeft);
    g.setColour (colours::text);
    g.setFont (monoFont (26.0f, true));
    g.drawText (hasTempo ? juce::String (plan.averageBpm, 1).replaceCharacter ('.', ',') : juce::String::fromUTF8 ("\xe2\x80\x93"),
                row.withTrimmedRight (34), juce::Justification::centredRight);
    g.setColour (colours::muted);
    g.setFont (monoFont (12.0f));
    g.drawText ("BPM", row, juce::Justification::bottomRight);

    row = box.removeFromTop (24);
    g.setColour (colours::muted);
    g.setFont (uiFont (12.5f));
    g.drawText ("Ziel (Projekt)", row, juce::Justification::centredLeft);
    g.setColour (colours::grid);
    g.setFont (monoFont (15.0f, true));
    const double targetBpm = tempo.bpmAt (plan.targetSeconds.empty() ? 0.0 : plan.targetSeconds.front());
    g.drawText (formatBpm (targetBpm) + " BPM", row, juce::Justification::centredRight);

    box.removeFromTop (6);
    g.setColour (colours::border);
    g.fillRect (box.removeFromTop (1));
    box.removeFromTop (6);

    auto smallRow = [&] (const juce::String& label, const juce::String& value) {
        auto r = box.removeFromTop (22);
        g.setColour (colours::muted);
        g.setFont (uiFont (12.5f));
        g.drawText (label, r, juce::Justification::centredLeft);
        g.setColour (colours::text);
        g.setFont (monoFont (12.5f));
        g.drawText (value, r, juce::Justification::centredRight);
    };
    smallRow ("Schwankung", hasTempo ? juce::String::fromUTF8 ("\xc2\xb1 ") + juce::String ((plan.maxBpm - plan.minBpm) * 0.5, 1).replaceCharacter ('.', ',') + " BPM"
                                     : juce::String::fromUTF8 ("\xe2\x80\x93"));
    const int inserted = session.getNumInsertedMarkers();
    smallRow ("Marker gesetzt", juce::String ((int) session.getMarkers().size()) + (inserted > 0 ? " (" + juce::String (inserted) + juce::String::fromUTF8 (" erg\xc3\xa4nzt)") : juce::String()));

    // Hint
    auto hint = hintBox;
    drawIcon (g, "info", hint.removeFromLeft (16).withHeight (16).toFloat(), colours::muted);
    hint.removeFromLeft (10);
    g.setColour (colours::muted);
    g.setFont (uiFont (12.5f));
    g.drawFittedText (juce::String::fromUTF8 ("Einen Schlag verpasst? Einfach weitertippen \xe2\x80\x93 L\xc3\xbc" "cken werden erg\xc3\xa4nzt, "
                                              "und im n\xc3\xa4" "chsten Schritt l\xc3\xa4sst sich jeder Marker korrigieren."),
                      hint, juce::Justification::topLeft, 4, 1.0f);

    paintFooter (g, footer);
}

void TapPage::sessionChanged()
{
    const auto& settings = session.getSettings();
    mode.setSelected (settings.tapMode == TapMode::downbeats ? 0 : 1);
    leadIn.setToggleState (settings.leadIn, juce::dontSendNotification);

    wave.setClip (session.getSource());
    const auto [start, end] = viewRange();
    wave.setTimeRange (start, end);
    wave.setMarkers (session.getMarkers());
    wave.setBeatTicks (settings.tapMode == TapMode::downbeats);

    const bool hasMarkers = ! session.getMarkers().empty();
    undo.setEnabled (hasMarkers);
    clearAll.setEnabled (hasMarkers);
    next.setEnabled (session.getMarkers().size() >= 2);
    playStop.setEnabled (session.hasSource());

    const auto& tempo = session.getProjectTempo();
    const auto& sig = tempo.signatureAt (0.0);
    pad->beatsPerBar = sig.numerator;

    refresh();
    repaint();
}

juce::String TapPage::nextTapLabel() const
{
    const auto& sig = session.getProjectTempo().signatureAt (0.0);
    const int count = (int) session.getMarkers().size();
    if (session.getSettings().tapMode == TapMode::downbeats)
        return "TAKT " + juce::String (count + 1) + juce::String::fromUTF8 (" \xc2\xb7 EINS");
    return "TAKT " + juce::String (count / sig.numerator + 1) + juce::String::fromUTF8 (" \xc2\xb7 Z\xc3\x84HLZEIT ") + juce::String (count % sig.numerator + 1);
}

void TapPage::refresh()
{
    const bool running = processor.isAudioRunning();
    const auto& preview = processor.getPreview();

    const double position = running ? processor.getAudiblePositionSeconds() : 0.0;
    wave.setPlayhead (running ? std::optional<double> (position) : std::nullopt);

    if (! session.hasSource() && ! processor.usesARA())
    {
        // Recording the input: show time so far.
        const auto& rec = processor.getRecorder();
        wave.setPlaceholder (processor.isHostPlaying() && rec.isArmed()
                                 ? juce::String::fromUTF8 ("\xe2\x97\x8f Aufnahme l\xc3\xa4uft \xe2\x80\x93 ") + formatTime (position) + juce::String::fromUTF8 (" \xe2\x80\x93 einfach mittappen!")
                                 : juce::String::fromUTF8 ("Starte die Wiedergabe im Host: Die Spur wird dabei aufgenommen, und du kannst direkt mittappen."));
        if (running)
            wave.setTimeRange (0.0, juce::jmax (30.0, position + 5.0));
    }

    const bool previewing = preview.isPlaying();
    configureButton (playStop, previewing ? juce::String ("Stopp  ") + formatTime (position)
                                          : (session.getMarkers().empty() ? juce::String ("Abspielen & tappen") : juce::String ("Von vorn neu tappen")),
                     previewing ? "stop" : "play", ButtonKind::solid);

    pad->active = running;
    const double leadInLeft = preview.leadInRemaining();
    pad->countdown = previewing && leadInLeft > 0.0 ? "GLEICH GEHT'S LOS  " + juce::String ((int) std::ceil (leadInLeft)) : juce::String();
    pad->topLabel = running ? nextTapLabel() : juce::String::fromUTF8 ("BEREIT \xe2\x80\x93 LEERTASTE STARTET");

    // Animate the beat dots from the tapped tempo.
    const auto& markers = session.getMarkers();
    pad->beatPhase = -1.0f;
    if (running && markers.size() >= 2)
    {
        const double barLength = (markers.back().seconds - markers.front().seconds) / (double) (markers.size() - 1)
                                 * (session.getSettings().tapMode == TapMode::beats ? pad->beatsPerBar : 1);
        const double sinceLast = position - markers.back().seconds;
        if (sinceLast >= 0.0 && sinceLast < barLength * 2.0)
        {
            const double phase = std::fmod (sinceLast / barLength, 1.0) * pad->beatsPerBar;
            pad->beatPhase = (float) phase;
        }
    }

    pad->flashAmount = juce::jmax (0.0f, pad->flashAmount - 0.12f);
    pad->repaint();
}

void TapPage::flash()
{
    pad->flashAmount = 1.0f;
    pad->repaint();
}

void TapPage::tap()
{
    if (! processor.isAudioRunning())
    {
        togglePlayback();
        return;
    }
    if (processor.getPreview().leadInRemaining() > 0.0)
        return;

    processor.tapNow();
    flash();
}

void TapPage::startPass (double fromSeconds)
{
    session.beginTapping (fromSeconds);
    const auto clip = session.getSource();
    const double start = clip != nullptr ? juce::jmax (clip->startSeconds(), fromSeconds - 3.0) : fromSeconds;
    processor.startPreview (start, false, session.getSettings().leadIn, false);
}

void TapPage::togglePlayback()
{
    if (processor.getPreview().isPlaying())
    {
        processor.stopPreview();
        return;
    }

    if (auto clip = session.getSource())
        startPass (clip->startSeconds());
}

bool TapPage::handleKey (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        tap();
        return true;
    }
    if (key == juce::KeyPress::backspaceKey)
    {
        session.undoLastTap();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        processor.stopPreview();
        return true;
    }
    if (key == juce::KeyPress::returnKey && next.isEnabled())
    {
        next.triggerClick();
        return true;
    }
    return false;
}

TapPage::~TapPage() = default;

} // namespace amt::plugin::ui
