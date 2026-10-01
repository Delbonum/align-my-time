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
        reload.onClick = [this] { processor.reloadTrack(); };
        addAndMakeVisible (reload);
    }

    void resized() override { reload.setBounds (getLocalBounds().removeFromRight (150).withSizeKeepingCentre (150, 30)); }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().withTrimmedRight (160);
        drawIcon (g, "track", area.removeFromLeft (16).toFloat().withSizeKeepingCentre (16.0f, 16.0f), colours::muted);
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
            text = "Keine Events geladen";
        else
            text = juce::String::fromUTF8 ("Ohne ARA: Spiele das Projekt im Host ab \xe2\x80\x93 die Spur wird dabei aufgenommen.");

        g.setColour (colours::muted);
        g.setFont (uiFont (12.5f));
        g.drawText ("Quelle:", area.removeFromLeft (52), juce::Justification::centredLeft);
        g.setColour (colours::text);
        g.setFont (uiFont (12.5f, true));
        g.drawFittedText (text, area, juce::Justification::centredLeft, 1);
    }

    void update() { reload.setButtonText (processor.usesARA() ? "Spur neu laden" : "Neu aufnehmen"); repaint(); }

private:
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
