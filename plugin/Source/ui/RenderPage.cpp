#include "RenderPage.h"
#include "../Exporter.h"

namespace amt::plugin::ui
{

/** The rendered file as a draggable tile: drop it onto the DAW's arrangement. */
class RenderPage::DragTile : public juce::Component
{
public:
    juce::Array<juce::File> files; // all tracks are dragged together
    bool savedOnly = false;        // standalone: nothing to drag into

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (colours::accentCard);
        g.fillRoundedRectangle (bounds, 10.0f);
        g.setColour (colours::accent);
        juce::Path outline;
        outline.addRoundedRectangle (bounds, 10.0f);
        const float dashes[] = { 6.0f, 4.0f };
        juce::PathStrokeType (1.5f).createDashedStroke (outline, outline, dashes, 2);
        g.fillPath (outline);

        auto area = getLocalBounds().reduced (14, 8);
        drawIcon (g, "drag", area.removeFromLeft (22).toFloat().withSizeKeepingCentre (20.0f, 20.0f), colours::accent);
        area.removeFromLeft (10);
        g.setColour (colours::text);
        g.setFont (uiFont (13.5f, true));
        const auto what = files.size() == 1 ? files[0].getFileName() : juce::String (files.size()) + tr (" Dateien, eine pro Spur");
        g.drawText ((savedOnly ? tr ("Gespeichert: ") : tr ("In die DAW ziehen: ")) + what, area.removeFromTop (area.getHeight() / 2), juce::Justification::bottomLeft);
        g.setColour (colours::muted);
        g.setFont (uiFont (12.0f));
        g.drawText (files.isEmpty() ? juce::String() : files[0].getParentDirectory().getFullPathName(), area, juce::Justification::topLeft);
    }

    void mouseDrag (const juce::MouseEvent&) override
    {
        juce::StringArray paths;
        for (const auto& f : files)
            if (f.existsAsFile())
                paths.add (f.getFullPathName());
        if (! paths.isEmpty())
            juce::DragAndDropContainer::performExternalDragDropOfFiles (paths, false, this);
    }

    void mouseEnter (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
};

//==============================================================================
RenderPage::RenderPage (AlignMyTimeProcessor& p) : Page (p), dragTile (std::make_unique<DragTile>())
{
    before.setWaveColour (colours::wave);
    after.setWaveColour (colours::waveAligned);
    after.setPlaceholder (tr ("Noch nicht berechnet – „Rendern“ erzeugt die angepasste Version."));
    addAndMakeVisible (before);
    addAndMakeVisible (after);

    newTrackCard.onClick = [this] { session.updateSettings ([] (SessionSettings& s) { s.destination = Destination::newTrack; }); };
    replaceCard.onClick = [this] { session.updateSettings ([] (SessionSettings& s) { s.destination = Destination::replaceInTrack; }); };
    addAndMakeVisible (newTrackCard);
    addAndMakeVisible (replaceCard);

    // Standalone: there is no track to replace or drag into; the result is simply a file.
    if (processor.isStandalone())
    {
        newTrackCard.setText (tr ("Als Datei speichern"), tr ("Schreibt eine WAV-Datei (24 bit). Wohin, fragt die App beim Exportieren."));
        replaceCard.setVisible (false);
        dragTile->savedOnly = true;
    }

    trackName.setFont (uiFont (13.5f));
    trackName.setIndents (12, 10);
    trackName.setTitle (tr ("Spurname"));
    trackName.onTextChange = [this] {
        const auto text = trackName.getText();
        session.updateSettings ([text] (SessionSettings& s) { s.trackName = text; });
    };
    addAndMakeVisible (trackName);

    fromProjectStart.onClick = [this] {
        session.updateSettings ([this] (SessionSettings& s) { s.exportFromProjectStart = fromProjectStart.getToggleState(); });
    };
    addChildComponent (fromProjectStart);
    fromProjectStart.setVisible (! processor.isStandalone());

    addChildComponent (*dragTile);
    configureButton (showInFolder, tr ("Im Ordner zeigen"), "folder", ButtonKind::ghost);
    showInFolder.onClick = [this] {
        if (! exportedFiles.isEmpty())
            exportedFiles[0].revealToUser();
    };
    addChildComponent (showInFolder);

    configureButton (back, tr ("Zurück"), "arrow-l", ButtonKind::ghost);
    back.onClick = [this] {
        processor.stopPreview();
        session.setStep (Step::review);
    };
    configureButton (listen, tr ("A/B vorhören"), "headphones", ButtonKind::ghost);
    listen.setTooltip (tr ("Leertaste: abwechselnd Original und angepasste Version ab dem Anfang"));
    listen.onClick = [this] {
        auto& preview = processor.getPreview();
        if (preview.isPlaying())
        {
            processor.stopPreview();
            return;
        }
        if (session.getAligned() != nullptr)
            processor.startPreview (session.getAligned()->startSeconds(), true, false, session.getSettings().clickInPreview);
    };
    renderButton.onClick = [this] { render(); };
    addAndMakeVisible (back);
    addAndMakeVisible (listen);
    addAndMakeVisible (renderButton);
}

std::vector<double> RenderPage::projectBarLines (double start, double end) const
{
    std::vector<double> lines;
    const auto& tempo = session.getProjectTempo();
    const int first = (int) std::floor (tempo.quartersToBars (tempo.secondsToQuarters (start)));
    for (int bar = first; bar < first + 2000; ++bar)
    {
        const double t = tempo.quartersToSeconds (tempo.barsToQuarters (bar));
        if (t > end)
            break;
        if (t >= start)
            lines.push_back (t);
    }
    return lines;
}

Destination RenderPage::destination() const
{
    return processor.isStandalone() ? Destination::newTrack : session.getSettings().destination;
}

void RenderPage::resized()
{
    juce::Rectangle<int> unusedFooter;
    auto area = layoutFrame (unusedFooter);
    footer = unusedFooter;

    area = area.reduced (24, 0);
    beforeCaption = area.removeFromTop (20);
    before.setBounds (area.removeFromTop (120));
    area.removeFromTop (10);
    afterCaption = area.removeFromTop (20);
    after.setBounds (area.removeFromTop (120));
    area.removeFromTop (18);

    rightArea = area.removeFromRight (320);
    area.removeFromRight (24);
    leftArea = area;

    auto left = leftArea.withTrimmedTop (22);
    auto cards = left.removeFromTop (104);
    newTrackCard.setBounds (replaceCard.isVisible() ? cards.removeFromLeft (cards.getWidth() / 2 - 6) : cards);
    cards.removeFromLeft (12);
    replaceCard.setBounds (cards);
    left.removeFromTop (12);
    auto nameRow = left.removeFromTop (38);
    nameLabel = nameRow.removeFromLeft (80);
    trackName.setBounds (nameRow);

    auto right = rightArea.withTrimmedTop (22);
    fromProjectStart.setBounds (right.removeFromTop (32));
    right.removeFromTop (10);
    dragTile->setBounds (right.removeFromTop (56));
    right.removeFromTop (6);
    showInFolder.setBounds (right.removeFromTop (30).removeFromLeft (170));

    auto f = footer.reduced (24, 12);
    back.setBounds (f.removeFromLeft (120));
    f.removeFromLeft (8);
    listen.setBounds (f.removeFromLeft (160));
    renderButton.setBounds (f.removeFromRight (250));
}

void RenderPage::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);

    const auto& plan = session.getPlan();
    g.setFont (uiFont (12.5f));
    g.setColour (colours::muted);
    g.drawText (tr ("Vorher · getappt, Ø ") + formatNumber (plan.averageBpm, 1) + " BPM", beforeCaption, juce::Justification::centredLeft);
    g.setColour (colours::grid);
    g.drawText (tr ("│ Projektraster (Takte)"), beforeCaption, juce::Justification::centredRight);

    const auto& settings = session.getSettings();
    const auto method = settings.method == AlignMethod::timeStretch
                            ? tr ("Time-Stretch · ") + juce::StringArray { tr ("Rhythmisch"), tr ("Melodisch"), tr ("Komplex") }[(int) settings.quality]
                            : tr ("Schneiden + Crossfade · ") + juce::String (juce::roundToInt (settings.crossfadeMs)) + " ms";
    g.setColour (colours::muted);
    g.drawText (tr ("Nachher · exakt im Projekttempo, Tonhöhe unverändert")
                    + (session.getAligned() != nullptr && ! session.isAlignedUpToDate() ? tr (" · veraltet, bitte neu rendern") : juce::String()),
                afterCaption, juce::Justification::centredLeft);
    g.drawText (method, afterCaption, juce::Justification::centredRight);

    drawSectionLabel (g, leftArea.withHeight (16), tr ("Ergebnis landet …"));
    drawSectionLabel (g, rightArea.withHeight (16), tr ("Optionen"));

    g.setColour (colours::muted);
    g.setFont (uiFont (12.5f));
    g.drawText (tr ("Spurname"), nameLabel, juce::Justification::centredLeft);

    if (errorText.isNotEmpty())
    {
        g.setColour (juce::Colour (0xffff8a7a));
        g.drawFittedText (errorText, rightArea.withTrimmedTop (170), juce::Justification::topLeft, 3);
    }
    else if (multitrackNote.isNotEmpty())
    {
        g.setColour (colours::muted);
        g.setFont (uiFont (12.0f));
        g.drawFittedText (multitrackNote, rightArea.withTrimmedTop (dragTile->isVisible() ? 170 : 66), juce::Justification::topLeft, 3);
    }

    paintFooter (g, footer);

    if (session.isRendering())
    {
        auto bar = footer.reduced (24, 0).withTrimmedLeft (300).withTrimmedRight (270).withSizeKeepingCentre (footer.getWidth() - 594, 6);
        g.setColour (colours::panel2);
        g.fillRoundedRectangle (bar.toFloat(), 3.0f);
        g.setColour (colours::accent);
        g.fillRoundedRectangle (bar.toFloat().withWidth ((float) (bar.getWidth() * session.getRenderProgress())), 3.0f);
    }
    else if (auto clip = session.getSource())
    {
        g.setColour (colours::muted);
        g.setFont (uiFont (12.5f));
        const int numTracks = session.getNumTracks();
        g.drawText (formatNumber (clip->sampleRate / 1000.0, 1) + utf8 (" kHz · 24 bit · ")
                        + juce::String (clip->numChannels() == 1 ? "Mono" : "Stereo") /* same in both languages */
                        + (numTracks > 1 ? utf8 (" · ") + juce::String (numTracks) + tr (" Spuren") : juce::String()),
                    footer.reduced (24, 0).withTrimmedRight (270), juce::Justification::centredRight);
    }
}

void RenderPage::sessionChanged()
{
    const auto& settings = session.getSettings();
    const auto& plan = session.getPlan();

    before.setClip (session.getSource());
    auto [start, end] = viewRange();
    if (session.canAlign())
    {
        // Common time axis: the union of both versions.
        const double s2 = plan.warp.sourceToTarget (start), e2 = plan.warp.sourceToTarget (end);
        start = juce::jmin (start, s2);
        end = juce::jmax (end, e2);
    }
    before.setTimeRange (start, end);
    after.setTimeRange (start, end);
    before.setMarkers (session.getMarkers());
    before.setGridLines (projectBarLines (start, end));

    std::vector<Marker> targets;
    for (auto t : plan.targetSeconds)
        targets.push_back ({ t, t, MarkerOrigin::tapped, true });
    after.setClip (session.getAligned());
    after.setMarkers (session.getAligned() != nullptr ? targets : std::vector<Marker> {});
    after.setGridLines (projectBarLines (start, end));
    const int firstLabel = settings.tapUnit == TapUnit::bar ? plan.firstBar + 1 : 1;
    before.setMarkerLabels (firstLabel);
    after.setMarkerLabels (firstLabel);

    newTrackCard.setToggleState (destination() == Destination::newTrack, juce::dontSendNotification);
    replaceCard.setToggleState (destination() == Destination::replaceInTrack, juce::dontSendNotification);
    fromProjectStart.setToggleState (settings.exportFromProjectStart, juce::dontSendNotification);
    fromProjectStart.setEnabled (destination() == Destination::newTrack);

    if (! trackName.hasKeyboardFocus (true))
    {
        auto name = settings.trackName;
        if (name.isEmpty())
            name = "Align My Time " + formatBpm (session.getProjectTempo().bpmAt (plan.targetSeconds.empty() ? 0.0 : plan.targetSeconds.front())) + " BPM";
        trackName.setText (name, false);
    }

    const bool showFile = destination() == Destination::newTrack && ! exportedFiles.isEmpty()
                          && exportedFiles[0].existsAsFile() && session.isAlignedUpToDate();
    dragTile->files = exportedFiles;

    // Multitrack: what happens to the other tracks.
    multitrackNote.clear();
    if (session.getNumTracks() > 1)
    {
        juce::StringArray hostTracks, files;
        for (const auto& t : session.getExtraTracks())
            (t.kind == ExtraTrack::Kind::hostTrack ? hostTracks : files).add (t.name);

        if (destination() == Destination::newTrack)
            multitrackNote = tr ("Eine Datei pro Spur, alle gleich lang und ab derselben Position: zusammen auf neue Spuren ziehen.");
        else
        {
            if (! hostTracks.isEmpty())
                multitrackNote = tr ("Ersetzt auch in:") + " " + hostTracks.joinIntoString (", ") + ".";
            if (! files.isEmpty())
                multitrackNote << (multitrackNote.isEmpty() ? "" : " ") << tr ("Zusätzliche Audiodateien gibt es nur als neue Spur.");
        }
    }
    dragTile->setVisible (showFile);
    showInFolder.setVisible (showFile);

    if (pendingAction && ! session.isRendering())
        finishPendingAction();

    refresh();
    repaint();
}

void RenderPage::refresh()
{
    juce::String text;
    juce::String icon = "check";

    if (session.isRendering())
    {
        text = tr ("Wird berechnet … ") + juce::String (juce::roundToInt (session.getRenderProgress() * 100.0)) + " %";
        icon = {};
    }
    else if (destination() == Destination::replaceInTrack)
    {
        text = session.isReplaceActive() && session.isAlignedUpToDate() ? tr ("Original wiederherstellen") : tr ("In Spur ersetzen");
        icon = session.isReplaceActive() && session.isAlignedUpToDate() ? "undo" : "check";
    }
    else
    {
        text = processor.isStandalone() ? tr ("Als Datei exportieren …") : tr ("In neue Spur rendern");
    }

    configureButton (renderButton, text, icon, ButtonKind::primary);
    renderButton.setEnabled (session.canAlign());
    renderButton.setTooltip (processor.getBlockingReason (true));
    listen.setEnabled (session.getAligned() != nullptr);
    configureButton (listen, processor.getPreview().isPlaying() ? tr ("Stopp") : tr ("Anhören"),
                     processor.getPreview().isPlaying() ? "stop" : "headphones", ButtonKind::ghost);

    if (session.isRendering())
        repaint (footer);
}

void RenderPage::exportResult()
{
    if (session.isRendering())
        pendingAction = true; // the running render ends in the export
    else if (destination() == Destination::newTrack)
        render();
}

void RenderPage::render()
{
    errorText.clear();

    if (session.isRendering())
    {
        session.cancelRender();
        pendingAction = false;
        return;
    }

    if (destination() == Destination::replaceInTrack && session.isReplaceActive() && session.isAlignedUpToDate())
    {
        session.setReplaceActive (false);
        return;
    }

    pendingAction = true;
    if (session.isAlignedUpToDate())
        finishPendingAction();
    else
        session.startRender();
}

void RenderPage::finishPendingAction()
{
    pendingAction = false;
    auto aligned = session.getAligned();
    if (aligned == nullptr || ! session.isAlignedUpToDate())
        return;

    if (destination() == Destination::replaceInTrack)
    {
        session.setReplaceActive (true);
        return;
    }

    if (! processor.isStandalone())
    {
        exportTo (Exporter::newFileFor (trackName.getText()));
        return;
    }

    // Standalone: a normal "save as" dialog, starting in Music/Align My Time.
    Exporter::defaultFolder().createDirectory();
    chooser = std::make_unique<juce::FileChooser> (tr ("Ergebnis als WAV-Datei speichern"), Exporter::newFileFor (trackName.getText()), "*.wav");
    juce::Component::SafePointer<RenderPage> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe] (const juce::FileChooser& fc) {
                              if (safe != nullptr && fc.getResult() != juce::File())
                                  safe->exportTo (fc.getResult().withFileExtension (".wav"));
                          });
}

void RenderPage::exportTo (const juce::File& firstFile)
{
    errorText.clear();
    exportedFiles.clear();
    const auto& tracks = session.getAlignedTracks();
    const bool fromStart = session.getSettings().exportFromProjectStart && ! processor.isStandalone();

    if (tracks.size() <= 1)
    {
        if (auto aligned = session.getAligned(); aligned != nullptr && Exporter::writeWav (*aligned, firstFile, fromStart, errorText))
            exportedFiles.add (firstFile);
    }
    else
    {
        // One file per track, named after the track, all padded the same way.
        const auto baseName = firstFile.getFileNameWithoutExtension();
        for (const auto& t : tracks)
        {
            const auto name = juce::File::createLegalFileName (baseName + utf8 (" – ") + (t.id.isEmpty() ? processor.getOwnTrackName() : t.name));
            auto file = firstFile.getSiblingFile (name + ".wav");
            if (! processor.isStandalone())
                file = file.getNonexistentSibling (false);
            if (! Exporter::writeWav (*t.clip, file, fromStart, errorText))
                break;
            exportedFiles.add (file);
        }
    }
    sessionChanged();
}

bool RenderPage::handleKey (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey && ! trackName.hasKeyboardFocus (true))
    {
        listen.triggerClick();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        processor.stopPreview();
        return true;
    }
    return false;
}

RenderPage::~RenderPage() = default;

} // namespace amt::plugin::ui
