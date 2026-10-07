#pragma once

#include "../PluginProcessor.h"
#include "LookAndFeel.h"
#include "WaveformView.h"

namespace amt::plugin::ui
{

/** "Quelle: Spur „Bass DI“ · 3 Events …" plus "load file" and reload/discard. */
class SourceStrip : public juce::Component
{
public:
    explicit SourceStrip (AlignMyTimeProcessor& p) : processor (p)
    {
        loadFile.setButtonText (tr ("Datei laden …"));
        loadFile.getProperties().set ("icon", "file");
        loadFile.setTooltip (tr ("Eine Audiodatei statt der Spur verwenden (auch per Drag & Drop ins Fenster)"));
        loadFile.setWantsKeyboardFocus (false);
        loadFile.onClick = [this] { chooseFile(); };
        addAndMakeVisible (loadFile);

        reload.setWantsKeyboardFocus (false);
        reload.onClick = [this] {
            if (processor.usesARA() || processor.isUsingAudioFile())
            {
                processor.reloadTrack();
                return;
            }
            confirmDiscard();
        };
        addAndMakeVisible (reload);

        tracks.getProperties().set ("icon", "plus");
        tracks.setTooltip (tr ("Weitere Spuren mit denselben Markern anpassen, z. B. alle Mikrofone einer Schlagzeugaufnahme. "
                               "Alle Spuren werden gleich behandelt und bleiben phasengleich."));
        tracks.setWantsKeyboardFocus (false);
        tracks.onClick = [this] { showTracksMenu(); };
        addAndMakeVisible (tracks);
        update();
    }

    void resized() override
    {
        auto area = getLocalBounds();
        if (reload.isVisible())
        {
            reload.setBounds (area.removeFromRight (180).withSizeKeepingCentre (180, 30));
            area.removeFromRight (8);
        }
        loadFile.setBounds (area.removeFromRight (150).withSizeKeepingCentre (150, 30));
        area.removeFromRight (8);
        tracks.setBounds (area.removeFromRight (140).withSizeKeepingCentre (140, 30));
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().withRight (tracks.getX() - 10);

        // Mode chip: where the audio comes from.
        const auto mode = processor.isUsingAudioFile() || processor.isStandalone() ? tr ("Datei")
                          : processor.usesARA()                                    ? juce::String ("ARA")
                                                                                   : juce::String ("Insert");
        g.setFont (monoFont (11.0f, true));
        const int chipWidth = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), mode) + 18;
        auto chip = area.removeFromLeft (chipWidth).withSizeKeepingCentre (chipWidth, 22);
        g.setColour (colours::panel2);
        g.fillRoundedRectangle (chip.toFloat(), 6.0f);
        g.setColour (colours::muted);
        g.drawText (mode, chip, juce::Justification::centred);
        area.removeFromLeft (10);

        auto& session = processor.getSession();
        juce::String text;
        if (processor.isLoadingTrack())
            text = tr ("Spur wird geladen …");
        else if (processor.getLoadError().isNotEmpty())
            text = processor.getLoadError();
        else if (session.hasSource())
        {
            const auto clip = session.getSource();
            // Built at display time, so a language switch applies here too.
            const auto description = processor.isUsingAudioFile() ? tr ("Datei") + ": " + processor.getAudioFile().getFileName()
                                     : processor.usesARA()         ? session.getSourceDescription()
                                                                   : tr ("Aufnahme vom Spureingang");
            text = description + juce::String::fromUTF8 (" \xc2\xb7 ") + formatTime (clip->startSeconds())
                   + juce::String::fromUTF8 ("\xe2\x80\x93") + formatTime (clip->endSeconds());

            const int extra = session.getNumTracks() - (session.getOwnTrack() != nullptr ? 1 : 0);
            if (extra > 0)
                text = description + " + " + juce::String (extra) + (extra == 1 ? tr (" weitere Spur") : tr (" weitere Spuren"))
                       + tr (" (Summe)");
            if (session.isAnalysing())
                text << utf8 (" · ") << tr ("Anschläge werden erkannt …");
        }
        else if (processor.isStandalone())
            text = tr ("Lade eine Audiodatei – Button rechts oder einfach ins Fenster ziehen.");
        else if (processor.usesARA())
            text = withHostName (tr ("Warte auf die Events von Cubase …"));
        else if (processor.isHostPlaying())
            text = tr ("● Aufnahme läuft – einfach mittappen");
        else
            text = withHostName (tr ("Spiele die Spur in Cubase ab – sie wird dabei aufgenommen."));

        // This track is aligned by the instance on another track (multitrack).
        if (const auto owner = processor.getLinkedByName(); owner.isNotEmpty() && session.getMarkers().empty())
            text = tr ("Wird mit Spur") + utf8 (" „") + owner + utf8 ("“ ") + tr ("angepasst – Marker und Rendern dort.");

        g.setColour (colours::muted);
        g.setFont (uiFont (12.5f));
        g.drawText (tr ("Quelle:"), area.removeFromLeft (52), juce::Justification::centredLeft);
        g.setColour (colours::text);
        g.setFont (uiFont (12.5f, true));
        g.drawFittedText (text, area, juce::Justification::centredLeft, 1);
    }

    void update()
    {
        if (processor.isUsingAudioFile() && ! processor.isStandalone())
        {
            reload.setButtonText (tr ("Zurück zur Spur"));
            reload.getProperties().set ("icon", "arrow-l");
        }
        else
        {
            reload.setButtonText (processor.usesARA() ? tr ("Spur neu laden") : tr ("Aufnahme verwerfen"));
            reload.getProperties().set ("icon", processor.usesARA() ? "refresh" : "trash");
        }
        reload.setVisible (! processor.isStandalone()
                           && (processor.usesARA() || processor.isUsingAudioFile() || processor.getSession().hasSource()));

        const int count = processor.getSession().getNumTracks();
        tracks.setButtonText (count > 1 ? tr ("Spuren") + " (" + juce::String (count) + ")" : tr ("Mehrspur"));
        resized();
        repaint();
    }

private:
    /** Which other tracks are aligned along: the project's tracks (ARA) and extra audio files. */
    void showTracksMenu()
    {
        juce::PopupMenu menu;
        menu.addSectionHeader (tr ("Mit denselben Markern anpassen"));

        const auto hostTracks = processor.getHostTracks();
        if (processor.usesARA())
        {
            if (hostTracks.empty())
                menu.addItem (tr ("Weitere Spuren erscheinen hier, wenn Align My Time auch auf ihnen läuft."), false, false, [] {});
            for (const auto& t : hostTracks)
            {
                const auto label = t.linkedElsewhere.isNotEmpty() ? t.name + utf8 (" – ") + tr ("schon mit") + utf8 (" „") + t.linkedElsewhere + utf8 ("“")
                                                                  : t.name;
                menu.addItem (label, t.linked || t.linkedElsewhere.isEmpty(), t.linked,
                              [safe = juce::Component::SafePointer<SourceStrip> (this), id = t.id, linked = t.linked] {
                                  if (safe != nullptr)
                                      safe->processor.setHostTrackLinked (id, ! linked);
                              });
            }
            menu.addSeparator();
        }

        menu.addItem (tr ("Audiodateien hinzufügen …"), [safe = juce::Component::SafePointer<SourceStrip> (this)] {
            if (safe != nullptr)
                safe->chooseExtraFiles();
        });

        for (const auto& t : processor.getSession().getExtraTracks())
        {
            if (t.kind != ExtraTrack::Kind::file)
                continue;
            menu.addItem (tr ("Entfernen:") + " " + t.name + (t.clip == nullptr ? tr (" (wird geladen …)") : juce::String()),
                          [safe = juce::Component::SafePointer<SourceStrip> (this), id = t.id] {
                              if (safe != nullptr)
                                  safe->processor.removeExtraTrack (ExtraTrack::Kind::file, id);
                          });
        }

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&tracks));
    }

    void chooseExtraFiles()
    {
        chooser = std::make_unique<juce::FileChooser> (tr ("Weitere Spuren laden"), juce::File(), AlignMyTimeProcessor::audioFileWildcard());
        juce::Component::SafePointer<SourceStrip> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectMultipleItems,
                              [safe] (const juce::FileChooser& fc) {
                                  if (safe != nullptr && ! fc.getResults().isEmpty())
                                      safe->processor.addExtraFiles (fc.getResults());
                              });
    }

    void chooseFile()
    {
        chooser = std::make_unique<juce::FileChooser> (tr ("Audiodatei laden"), juce::File(), AlignMyTimeProcessor::audioFileWildcard());
        juce::Component::SafePointer<SourceStrip> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe] (const juce::FileChooser& fc) {
                                  if (safe == nullptr)
                                      return;
                                  const auto file = fc.getResult();
                                  if (file.existsAsFile())
                                      safe->processor.loadAudioFile (file);
                              });
    }

    /** Without ARA, discarding throws work away: ask first. */
    void confirmDiscard()
    {
        juce::Component::SafePointer<SourceStrip> safe (this);
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                          .withTitle (tr ("Aufnahme verwerfen?"))
                                          .withMessage (withHostName (tr ("Die aufgenommene Spur und alle Marker werden gelöscht. "
                                                                          "Danach die Spur in Cubase erneut abspielen.")))
                                          .withButton (tr ("Verwerfen"))
                                          .withButton (tr ("Abbrechen"))
                                          .withAssociatedComponent (this),
                                      [safe] (int result) {
                                          if (safe != nullptr && result == 1)
                                              safe->processor.discardRecording();
                                      });
    }

    AlignMyTimeProcessor& processor;
    juce::TextButton loadFile, reload, tracks;
    std::unique_ptr<juce::FileChooser> chooser;
};

/** Base for the three step pages. */
class Page : public juce::Component, protected juce::ChangeListener
{
public:
    explicit Page (AlignMyTimeProcessor& p) : processor (p), session (p.getSession()), source (p)
    {
        addAndMakeVisible (source);
        session.addChangeListener (this);
    }

    ~Page() override { session.removeChangeListener (this); }

    /** ~30 times per second while visible (playhead, meters). */
    virtual void refresh() {}

    /** Keyboard shortcuts while the page is shown. */
    virtual bool handleKey (const juce::KeyPress&) { return false; }

    /** Called when the session changed (markers, settings, source ...). */
    virtual void sessionChanged() {}

protected:
    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        source.update();
        if (isShowing())
            sessionChanged();
    }

    void visibilityChanged() override
    {
        if (isVisible())
            sessionChanged();
    }

    /** Lays out source strip + footer; returns the area in between. */
    juce::Rectangle<int> layoutFrame (juce::Rectangle<int>& footer)
    {
        auto area = getLocalBounds();
        footer = area.removeFromBottom (64);
        source.setBounds (area.removeFromTop (44).reduced (20, 0));
        return area;
    }

    void paintFooter (juce::Graphics& g, juce::Rectangle<int> footer)
    {
        g.setColour (colours::panel);
        g.fillRect (footer);
        g.setColour (colours::border);
        g.fillRect (footer.removeFromTop (1));
    }

    /** Explains a disabled "next" button right next to it (amber, so it is noticed). */
    static void paintBlockingReason (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& reason)
    {
        if (reason.isEmpty())
            return;
        g.setFont (uiFont (12.5f, true));
        const int w = juce::jmin (area.getWidth(), juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), reason) + 26);
        auto box = area.removeFromRight (w);
        drawIcon (g, "info", box.removeFromLeft (16).withSizeKeepingCentre (16, 16).toFloat(), colours::accent);
        box.removeFromLeft (8);
        g.setColour (colours::accent);
        g.drawFittedText (reason, box, juce::Justification::centredLeft, 2);
    }

    /** Range shown by waveforms: the source plus a little air. */
    std::pair<double, double> viewRange() const
    {
        if (auto clip = session.getSource())
        {
            const double pad = juce::jmax (0.2, (clip->endSeconds() - clip->startSeconds()) * 0.01);
            return { clip->startSeconds() - pad, clip->endSeconds() + pad };
        }
        return { 0.0, 30.0 };
    }

    AlignMyTimeProcessor& processor;
    AlignSession& session;
    SourceStrip source;
};

inline void configureButton (juce::TextButton& b, const juce::String& text, const juce::String& icon, ButtonKind kind, bool iconAfter = false)
{
    b.setButtonText (text);
    if (icon.isNotEmpty())
        b.getProperties().set ("icon", icon);
    if (iconAfter)
        b.getProperties().set ("iconAfter", true);
    setKind (b, kind);
    if (text.isEmpty())
        b.setTitle (icon);
}

} // namespace amt::plugin::ui
