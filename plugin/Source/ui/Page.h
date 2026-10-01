#pragma once

#include "../PluginProcessor.h"
#include "LookAndFeel.h"
#include "WaveformView.h"

namespace amt::plugin::ui
{

/** "Quelle: Spur „Bass DI“ · 3 Events …" plus a reload button. */
class SourceStrip : public juce::Component
{
public:
    explicit SourceStrip (AlignMyTimeProcessor& p) : processor (p)
    {
        reload.setButtonText ("Spur neu laden");
        reload.getProperties().set ("icon", "refresh");
        reload.setWantsKeyboardFocus (false);
        reload.onClick = [this] {
            if (processor.usesARA())
            {
                processor.reloadTrack();
                return;
            }

            confirmDiscard();
        };
        addAndMakeVisible (reload);
    }

    void resized() override { reload.setBounds (getLocalBounds().removeFromRight (170).withSizeKeepingCentre (170, 30)); }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().withTrimmedRight (180);

        // Mode chip: how the track gets into the plug-in.
        const auto mode = processor.usesARA() ? juce::String ("ARA") : juce::String ("Insert");
        auto chip = area.removeFromLeft (processor.usesARA() ? 44 : 54).withSizeKeepingCentre (processor.usesARA() ? 44 : 54, 22);
        g.setColour (colours::panel2);
        g.fillRoundedRectangle (chip.toFloat(), 6.0f);
        g.setColour (colours::muted);
        g.setFont (monoFont (11.0f, true));
        g.drawText (mode, chip, juce::Justification::centred);
        area.removeFromLeft (10);

        auto& session = processor.getSession();
        juce::String text;
        if (processor.isLoadingTrack())
            text = juce::String::fromUTF8 ("Spur wird geladen \xe2\x80\xa6");
        else if (processor.getLoadError().isNotEmpty())
            text = processor.getLoadError();
        else if (session.hasSource())
        {
            const auto clip = session.getSource();
            text = session.getSourceDescription() + juce::String::fromUTF8 (" \xc2\xb7 ") + formatTime (clip->startSeconds())
                   + juce::String::fromUTF8 ("\xe2\x80\x93") + formatTime (clip->endSeconds());
        }
        else if (processor.usesARA())
            text = de ("Warte auf die Events von Cubase …");
        else if (processor.isHostPlaying())
            text = de ("● Aufnahme läuft – einfach mittappen");
        else
            text = de ("Spiele die Spur in Cubase ab – sie wird dabei aufgenommen.");

        g.setColour (colours::muted);
        g.setFont (uiFont (12.5f));
        g.drawText ("Quelle:", area.removeFromLeft (52), juce::Justification::centredLeft);
        g.setColour (colours::text);
        g.setFont (uiFont (12.5f, true));
        g.drawFittedText (text, area, juce::Justification::centredLeft, 1);
    }

    void update()
    {
        reload.setButtonText (processor.usesARA() ? juce::String ("Spur neu laden") : de ("Aufnahme verwerfen"));
        reload.getProperties().set ("icon", processor.usesARA() ? "refresh" : "trash");
        reload.setVisible (processor.usesARA() || processor.getSession().hasSource());
        repaint();
    }


private:
    /** Without ARA, discarding throws work away: ask first. */
    void confirmDiscard()
    {
        juce::Component::SafePointer<SourceStrip> safe (this);
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                          .withTitle (de ("Aufnahme verwerfen?"))
                                          .withMessage (de ("Die aufgenommene Spur und alle Marker werden gelöscht. "
                                                            "Danach die Spur in Cubase erneut abspielen."))
                                          .withButton ("Verwerfen")
                                          .withButton ("Abbrechen")
                                          .withAssociatedComponent (this),
                                      [safe] (int result) {
                                          if (safe != nullptr && result == 1)
                                              safe->processor.discardRecording();
                                      });
    }

    AlignMyTimeProcessor& processor;
    juce::TextButton reload;
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
