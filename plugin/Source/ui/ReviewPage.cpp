#include "ReviewPage.h"

namespace amt::plugin::ui
{

ReviewPage::ReviewPage (AlignMyTimeProcessor& p) : Page (p)
{
    wave.setInteractive (true);
    wave.setTooltip (de ("Marker ziehen zum Verschieben · Doppelklick fügt einen Marker hinzu"));
    wave.onSelectMarker = [this] (int i) { session.selectMarker (i); };
    wave.onMoveMarker = [this] (int i, double t) { session.moveMarker (i, t); };
    wave.onAddMarker = [this] (double t) { session.addMarker (t); };
    addAndMakeVisible (wave);
    addAndMakeVisible (tempoLane);

    configureButton (nudgeLeft, {}, "nudge-l", ButtonKind::solid);
    nudgeLeft.setTooltip (de ("5 ms früher (←)"));
    nudgeLeft.onClick = [this] { session.nudgeSelected (-0.005); };
    configureButton (nudgeRight, {}, "nudge-r", ButtonKind::solid);
    nudgeRight.setTooltip (de ("5 ms später (→)"));
    nudgeRight.onClick = [this] { session.nudgeSelected (0.005); };
    configureButton (addMarker, "Marker", "plus", ButtonKind::ghost);
    addMarker.setTooltip (de ("Fügt mitten im ausgewählten Takt einen Marker ein (oder Doppelklick in die Wellenform)"));
    addMarker.onClick = [this] {
        const auto& markers = session.getMarkers();
        const int sel = session.getSelectedMarker();
        if (sel >= 0 && sel + 1 < (int) markers.size())
            session.addMarker (0.5 * (markers[(size_t) sel].seconds + markers[(size_t) sel + 1].seconds));
        else if (! markers.empty())
            session.addMarker (markers.back().seconds + 1.0);
    };
    configureButton (removeMarker, de ("Löschen"), "trash", ButtonKind::ghost);
    removeMarker.onClick = [this] { session.removeSelected(); };
    configureButton (retap, "Ab hier neu tappen", "refresh", ButtonKind::ghost);
    retap.onClick = [this] {
        const int sel = session.getSelectedMarker();
        if (sel >= 0 && onRetapFrom)
            onRetapFrom (session.getMarkers()[(size_t) sel].seconds);
    };
    configureButton (barDown, {}, "nudge-l", ButtonKind::ghost);
    barDown.setTooltip (de ("Erster Marker einen Takt früher"));
    barDown.onClick = [this] {
        const int bar = session.getPlan().firstBar;
        session.updateSettings ([bar] (SessionSettings& s) { s.firstBar = bar - 1; });
    };
    configureButton (barUp, {}, "nudge-r", ButtonKind::ghost);
    barUp.setTooltip (de ("Erster Marker einen Takt später"));
    barUp.onClick = [this] {
        const int bar = session.getPlan().firstBar;
        session.updateSettings ([bar] (SessionSettings& s) { s.firstBar = bar + 1; });
    };
    snap.onClick = [this] { session.setSnapToAttacks (snap.getToggleState()); };

    // How far apart the taps are on the grid: fixes "I tapped on 1 and 3" without re-tapping.
    configureButton (unitBigger, {}, "nudge-l", ButtonKind::ghost);
    unitBigger.setTooltip (de ("Größerer Abstand (z. B. 2 Takte)"));
    unitBigger.onClick = [this] { stepUnit (-1); };
    configureButton (unitSmaller, {}, "nudge-r", ButtonKind::ghost);
    unitSmaller.setTooltip (de ("Kleinerer Abstand (z. B. ½ Takt, wenn du auf 1 und 3 getippt hast)"));
    unitSmaller.onClick = [this] { stepUnit (1); };

    configureButton (applySuggestion, {}, "check", ButtonKind::primary);
    applySuggestion.onClick = [this] {
        if (auto unit = session.getTapUnitSuggestion())
            session.updateSettings ([u = *unit] (SessionSettings& s) { s.tapUnit = u; });
    };
    addChildComponent (applySuggestion);

    for (auto* c : { &nudgeLeft, &nudgeRight, &addMarker, &removeMarker, &retap, &barDown, &barUp, &unitBigger, &unitSmaller })
        addAndMakeVisible (c);
    addAndMakeVisible (snap);

    stretchCard.getProperties().set ("icon", "stretch");
    sliceCard.getProperties().set ("icon", "scissors");
    stretchCard.onClick = [this] { session.updateSettings ([] (SessionSettings& s) { s.method = AlignMethod::timeStretch; }); };
    sliceCard.onClick = [this] { session.updateSettings ([] (SessionSettings& s) { s.method = AlignMethod::slices; }); };
    addAndMakeVisible (stretchCard);
    addAndMakeVisible (sliceCard);

    quality.onChange = [this] (int i) { session.updateSettings ([i] (SessionSettings& s) { s.quality = (StretchQuality) i; }); };
    stretchCard.addAndMakeVisible (quality);

    crossfade.setRange (2.0, 50.0, 1.0);
    crossfade.setTextValueSuffix (" ms");
    crossfade.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 24);
    crossfade.setTitle ("Crossfade");
    crossfade.onValueChange = [this] {
        const double v = crossfade.getValue();
        session.updateSettings ([v] (SessionSettings& s) { s.crossfadeMs = v; });
    };
    sliceCard.addAndMakeVisible (crossfade);

    ab.onChange = [this] (int) {
        if (processor.getPreview().isPlaying())
            startPreviewNow();
    };
    addAndMakeVisible (ab);
    configureButton (play, "Abspielen", "play", ButtonKind::solid);
    play.onClick = [this] { togglePreview(); };
    addAndMakeVisible (play);
    click.onClick = [this] { session.updateSettings ([this] (SessionSettings& s) { s.clickInPreview = click.getToggleState(); }); };
    addAndMakeVisible (click);

    mix.setRange (0.0, 1.0, 0.01);
    mix.setTitle ("Mix Spur / Klick");
    mix.setTooltip (de ("Lautstärkeverhältnis beim Vorhören: links nur Spur, Mitte beides voll, rechts nur Klick"));
    mix.setDoubleClickReturnValue (true, 0.5);
    mix.onValueChange = [this] {
        const float v = (float) mix.getValue();
        processor.setPreviewMix (v);
        session.updateSettings ([v] (SessionSettings& s) { s.clickBlend = v; });
    };
    addAndMakeVisible (mix);

    configureButton (back, de ("Zurück"), "arrow-l", ButtonKind::ghost);
    back.onClick = [this] {
        processor.stopPreview();
        session.setStep (Step::tap);
    };
    configureButton (next, "Weiter: Rendern", "arrow-r", ButtonKind::primary, true);
    next.onClick = [this] {
        processor.stopPreview();
        session.setStep (Step::render);
    };
    addAndMakeVisible (back);
    addAndMakeVisible (next);
}

void ReviewPage::resized()
{
    auto area = layoutFrame (footer);
    wave.setBounds (area.removeFromTop (150).reduced (24, 0));
    area.removeFromTop (10);
    tempoLane.setBounds (area.removeFromTop (66).reduced (24, 0));
    area.removeFromTop (8);
    bannerRow = area.removeFromTop (40).reduced (24, 0);
    applySuggestion.setBounds (bannerRow.removeFromRight (230).reduced (0, 3));
    area.removeFromTop (10);
    area = area.reduced (24, 0).withTrimmedBottom (8);

    leftColumn = area.removeFromLeft (250);
    rightColumn = area.removeFromRight (230);
    area.reduce (20, 0);
    middleColumn = area;

    auto left = leftColumn.withTrimmedTop (22);
    auto nudgeRow = left.removeFromTop (36);
    nudgeLeft.setBounds (nudgeRow.removeFromLeft (44));
    nudgeRight.setBounds (nudgeRow.removeFromRight (44));
    offsetBox = nudgeRow.reduced (8, 0);
    left.removeFromTop (6);
    snap.setBounds (left.removeFromTop (30));
    left.removeFromTop (6);
    auto buttons = left.removeFromTop (34);
    addMarker.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 4));
    buttons.removeFromLeft (8);
    removeMarker.setBounds (buttons);
    left.removeFromTop (8);
    retap.setBounds (left.removeFromTop (34));
    left.removeFromTop (8);
    barRow = left.removeFromTop (30);
    barUp.setBounds (barRow.removeFromRight (30));
    barDown.setBounds (barRow.removeFromRight (30).translated (-60, 0));
    left.removeFromTop (4);
    unitRow = left.removeFromTop (30);
    unitSmaller.setBounds (unitRow.removeFromRight (30));
    unitBigger.setBounds (unitRow.removeFromRight (30).translated (-60, 0));

    auto middle = middleColumn.withTrimmedTop (22);
    auto cards = middle.removeFromTop (juce::jmin (middle.getHeight(), 168));
    stretchCard.setBounds (cards.removeFromLeft (cards.getWidth() / 2 - 6));
    cards.removeFromLeft (12);
    sliceCard.setBounds (cards);
    quality.setBounds (stretchCard.getExtraArea().withHeight (36));
    crossfade.setBounds (sliceCard.getExtraArea().withHeight (30));

    auto right = rightColumn.withTrimmedTop (22);
    ab.setBounds (right.removeFromTop (40));
    right.removeFromTop (10);
    play.setBounds (right.removeFromTop (48));
    right.removeFromTop (10);
    click.setBounds (right.removeFromTop (32));
    right.removeFromTop (6);
    mixRow = right.removeFromTop (40);
    mix.setBounds (mixRow.withTrimmedTop (16));

    auto f = footer.reduced (24, 12);
    back.setBounds (f.removeFromLeft (130));
    next.setBounds (f.removeFromRight (200));
}

void ReviewPage::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);

    const int sel = session.getSelectedMarker();
    drawSectionLabel (g, leftColumn.withHeight (16), sel >= 0 ? de ("Marker · ") + selectedInfo().upToFirstOccurrenceOf ("\n", false, false) : juce::String ("Marker bearbeiten"));
    drawSectionLabel (g, middleColumn.withHeight (16), "So wird angepasst");
    drawSectionLabel (g, rightColumn.withHeight (16), de ("VORHÖREN"));

    // Mix labels above the slider
    g.setFont (uiFont (11.5f));
    g.setColour (colours::muted);
    g.drawText ("Spur", mixRow.withHeight (14), juce::Justification::centredLeft);
    g.drawText ("Klick", mixRow.withHeight (14), juce::Justification::centredRight);
    g.drawText ("Mix", mixRow.withHeight (14), juce::Justification::centred);

    // Tempo suggestion banner
    if (auto suggestion = session.getTapUnitSuggestion())
    {
        const auto& plan = session.getPlan();
        const double target = session.getProjectTempo().bpmAt (plan.targetSeconds.empty() ? 0.0 : plan.targetSeconds.front());
        auto banner = bannerRow.withRight (applySuggestion.getRight()).toFloat();
        g.setColour (colours::accentCard);
        g.fillRoundedRectangle (banner, 8.0f);
        g.setColour (colours::accent);
        g.drawRoundedRectangle (banner.reduced (0.5f), 8.0f, 1.0f);
        auto text = bannerRow.reduced (12, 0);
        drawIcon (g, "info", text.removeFromLeft (16).withSizeKeepingCentre (16, 16).toFloat(), colours::accent);
        text.removeFromLeft (10);
        g.setColour (colours::text);
        g.setFont (uiFont (13.0f));
        g.drawFittedText (de ("Getappt ≈ ") + juce::String (plan.averageBpm, 1).replaceCharacter ('.', ',') + de (" BPM, Projekt ")
                              + juce::String (target, 1).replaceCharacter ('.', ',') + de (" BPM. Anders gezählt? Dann ist jeder Marker ")
                              + describeTapUnit (*suggestion) + ".",
                          text, juce::Justification::centredLeft, 1);
    }
    else if (session.getMarkers().size() >= 2)
    {
        const auto& plan = session.getPlan();
        const double target = session.getProjectTempo().bpmAt (plan.targetSeconds.empty() ? 0.0 : plan.targetSeconds.front());
        auto text = bannerRow;
        drawIcon (g, "check", text.removeFromLeft (16).withSizeKeepingCentre (16, 16).toFloat(), colours::good);
        text.removeFromLeft (10);
        g.setColour (colours::muted);
        g.setFont (uiFont (12.5f));
        g.drawText (de ("Getappt Ø ") + juce::String (plan.averageBpm, 1).replaceCharacter ('.', ',') + de (" BPM → wird auf ")
                        + juce::String (target, 1).replaceCharacter ('.', ',') + de (" BPM gebracht · 1 Marker = ")
                        + describeTapUnit (session.getSettings().tapUnit),
                    text, juce::Justification::centredLeft);
    }

    // Offset of the selected marker against the tap
    g.setColour (colours::background);
    g.fillRoundedRectangle (offsetBox.toFloat(), 8.0f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (offsetBox.toFloat().reduced (0.5f), 8.0f, 1.0f);
    g.setFont (monoFont (13.0f));
    g.setColour (sel >= 0 ? colours::text : colours::muted);
    juce::String offset = de ("–");
    if (sel >= 0)
    {
        const auto& m = session.getMarkers()[(size_t) sel];
        const int ms = juce::roundToInt ((m.seconds - m.tappedSeconds) * 1000.0);
        offset = (ms > 0 ? "+" : ms < 0 ? de ("−") : juce::String()) + juce::String (std::abs (ms)) + " ms";
    }
    g.drawText (offset, offsetBox, juce::Justification::centred);

    // Grid unit
    {
        auto unitArea = juce::Rectangle<int> (leftColumn.getX(), unitRow.getY(), leftColumn.getWidth(), unitRow.getHeight());
        g.setColour (colours::muted);
        g.setFont (uiFont (12.5f));
        g.drawText ("1 Marker =", unitArea, juce::Justification::centredLeft);
        g.setColour (colours::text);
        g.setFont (uiFont (13.0f, true));
        g.drawText (describeTapUnit (session.getSettings().tapUnit),
                    juce::Rectangle<int> (unitBigger.getRight(), unitArea.getY(), unitSmaller.getX() - unitBigger.getRight(), unitArea.getHeight()),
                    juce::Justification::centred);
    }

    // First bar
    auto row = juce::Rectangle<int> (leftColumn.getX(), barRow.getY(), leftColumn.getWidth(), barRow.getHeight());
    g.setColour (colours::muted);
    g.setFont (uiFont (12.5f));
    g.drawText ("Erster Marker = Takt", row, juce::Justification::centredLeft);
    g.setColour (colours::text);
    g.setFont (monoFont (14.0f, true));
    g.drawText (juce::String (session.getPlan().firstBar + 1), juce::Rectangle<int> (barDown.getRight(), row.getY(), barUp.getX() - barDown.getRight(), row.getHeight()),
                juce::Justification::centred);

    paintFooter (g, footer);
    if (! next.isEnabled())
    {
        paintBlockingReason (g, footer.reduced (24, 0).withTrimmedRight (next.getWidth() + 16).withTrimmedLeft (back.getWidth() + 16),
                             processor.getBlockingReason (false));
        return;
    }
    const int inserted = session.getNumInsertedMarkers();
    g.setColour (colours::muted);
    g.setFont (uiFont (12.5f));
    g.drawText (juce::String ((int) session.getMarkers().size()) + " Marker" + (inserted > 0 ? de (" · ") + juce::String (inserted) + de (" ergänzt – bitte prüfen") : juce::String())
                    + (session.isRendering() ? de (" · wird berechnet …") : juce::String()),
                footer.reduced (24, 0).withTrimmedRight (220).withTrimmedLeft (150), juce::Justification::centredRight);
}

juce::String ReviewPage::selectedInfo() const
{
    const int sel = session.getSelectedMarker();
    if (sel < 0)
        return {};

    const auto& m = session.getMarkers()[(size_t) sel];
    const auto& plan = session.getPlan();
    const auto position = describeGridPosition (session.getProjectTempo(), session.getSettings().tapUnit, plan.firstBar, sel);

    juce::String how = m.origin == MarkerOrigin::inserted ? de ("automatisch ergänzt")
                       : m.origin == MarkerOrigin::manual ? juce::String ("von Hand gesetzt")
                       : m.snappedToAttack                ? juce::String ("an Transiente gerastet")
                                                          : juce::String ("getappt");
    return position + "\n" + how;
}

void ReviewPage::sessionChanged()
{
    const auto& settings = session.getSettings();
    const auto& markers = session.getMarkers();

    wave.setClip (session.getSource());
    const auto [start, end] = viewRange();
    wave.setTimeRange (start, end);
    wave.setMarkers (markers, session.getSelectedMarker());
    wave.setMarkerLabels (settings.tapUnit == TapUnit::bar ? session.getPlan().firstBar + 1 : 1);
    wave.setSelectedInfo (selectedInfo());

    std::vector<double> seconds;
    for (const auto& m : markers)
        seconds.push_back (m.seconds);
    tempoLane.setData (session.getPlan(), seconds, session.getProjectTempo(), start, end);

    const bool hasSelection = session.getSelectedMarker() >= 0;
    nudgeLeft.setEnabled (hasSelection);
    nudgeRight.setEnabled (hasSelection);
    removeMarker.setEnabled (hasSelection);
    retap.setEnabled (hasSelection);
    snap.setToggleState (settings.snapToAttacks, juce::dontSendNotification);

    stretchCard.setToggleState (settings.method == AlignMethod::timeStretch, juce::dontSendNotification);
    sliceCard.setToggleState (settings.method == AlignMethod::slices, juce::dontSendNotification);
    quality.setSelected ((int) settings.quality);
    crossfade.setValue (settings.crossfadeMs, juce::dontSendNotification);
    click.setToggleState (settings.clickInPreview, juce::dontSendNotification);
    mix.setValue (settings.clickBlend, juce::dontSendNotification);
    processor.setPreviewMix (settings.clickBlend);
    next.setEnabled (session.canAlign());
    next.setTooltip (processor.getBlockingReason (false));

    const auto suggestion = session.getTapUnitSuggestion();
    applySuggestion.setVisible (suggestion.has_value());
    if (suggestion.has_value())
        configureButton (applySuggestion, "Als " + describeTapUnit (*suggestion) + " werten", "check", ButtonKind::primary);
    unitBigger.setEnabled (settings.tapUnit != TapUnit::twoBars);
    unitSmaller.setEnabled (settings.tapUnit != TapUnit::halfBeat);

    // A render finished that we were waiting for: play it.
    if (playWhenRendered && ! session.isRendering() && session.isAlignedUpToDate())
    {
        playWhenRendered = false;
        startPreviewNow();
    }

    repaint();
}

void ReviewPage::refresh()
{
    const auto& preview = processor.getPreview();
    const bool playing = preview.isPlaying();

    if (playing && ab.getSelected() == 1)
    {
        // Show the playhead in source time while the aligned version plays.
        wave.setPlayhead (session.getPlan().warp.targetToSource (preview.positionSeconds()));
    }
    else
    {
        wave.setPlayhead (playing ? std::optional<double> (preview.positionSeconds()) : std::nullopt);
    }

    const bool waiting = playWhenRendered && session.isRendering();
    configureButton (play, waiting ? de ("Wird berechnet … ") + juce::String (juce::roundToInt (session.getRenderProgress() * 100.0)) + " %"
                                   : playing ? juce::String ("Stopp") : juce::String ("Abspielen"),
                     playing ? "stop" : "play", ButtonKind::solid);
}

void ReviewPage::togglePreview()
{
    if (processor.getPreview().isPlaying() || playWhenRendered)
    {
        playWhenRendered = false;
        processor.stopPreview();
        return;
    }
    startPreviewNow();
}

void ReviewPage::startPreviewNow()
{
    const auto& markers = session.getMarkers();
    const int sel = session.getSelectedMarker();
    const double from = sel >= 0 ? markers[(size_t) sel].seconds - 1.0 : (session.hasSource() ? session.getSource()->startSeconds() : 0.0);

    if (ab.getSelected() == 0)
    {
        processor.startPreview (from, false, false, false);
        return;
    }

    if (! session.isAlignedUpToDate())
    {
        playWhenRendered = true;
        if (! session.isRendering())
            session.startRender();
        return;
    }

    processor.startPreview (session.getPlan().warp.sourceToTarget (from), true, false, session.getSettings().clickInPreview);
}

void ReviewPage::stepUnit (int direction)
{
    const auto current = session.getSettings().tapUnit;
    int index = 0;
    for (int i = 0; i < (int) std::size (tapUnitsBySize); ++i)
        if (tapUnitsBySize[i] == current)
            index = i;
    const auto newUnit = tapUnitsBySize[juce::jlimit (0, (int) std::size (tapUnitsBySize) - 1, index + direction)];
    session.updateSettings ([newUnit] (SessionSettings& s) { s.tapUnit = newUnit; });
}

bool ReviewPage::handleKey (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        togglePreview();
        return true;
    }
    if (key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey)
    {
        const double step = key.getModifiers().isShiftDown() ? 0.001 : 0.005;
        session.nudgeSelected (key == juce::KeyPress::leftKey ? -step : step);
        return true;
    }
    if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
    {
        const int count = (int) session.getMarkers().size();
        const int sel = session.getSelectedMarker();
        session.selectMarker (juce::jlimit (0, count - 1, sel + (key == juce::KeyPress::downKey ? 1 : -1)));
        return true;
    }
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        session.removeSelected();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        processor.stopPreview();
        return true;
    }
    return false;
}

} // namespace amt::plugin::ui
