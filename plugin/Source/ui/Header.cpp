#include "Header.h"

namespace amt::plugin::ui
{

juce::String describeProjectTempo (const TempoMap& tempo)
{
    const auto& points = tempo.points();
    bool constant = true;
    const double first = tempo.bpmAt (points.front().seconds);
    for (const auto& p : points)
        constant = constant && std::abs (tempo.bpmAt (p.seconds) - first) < 0.005;

    const auto& sig = tempo.signatures().front();
    const auto bpmText = constant ? formatBpm (first) + " BPM" : juce::String ("Tempo variabel");
    return bpmText + juce::String::fromUTF8 (" \xc2\xb7 ") + juce::String (sig.numerator) + "/" + juce::String (sig.denominator)
           + (tempo.signatures().size() > 1 ? juce::String ("+") : juce::String());
}

//==============================================================================
class Header::StepButton : public juce::Button
{
public:
    StepButton (int n, const juce::String& label) : juce::Button (label), number (n) {}

    bool active = false, done = false;

    void paintButton (juce::Graphics& g, bool highlighted, bool) override
    {
        auto bounds = getLocalBounds().toFloat();
        if (active)
        {
            g.setColour (colours::panel2);
            g.fillRoundedRectangle (bounds.reduced (0.5f), 8.0f);
            g.setColour (colours::border);
            g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);
        }
        else if (highlighted && isEnabled())
        {
            g.setColour (colours::panel2.withAlpha (0.5f));
            g.fillRoundedRectangle (bounds.reduced (0.5f), 8.0f);
        }

        auto area = getLocalBounds().reduced (8, 0);
        const auto badge = area.removeFromLeft (20).withSizeKeepingCentre (20, 20).toFloat();
        g.setColour (active ? colours::accent : done ? juce::Colour (0xff2c3a2f) : colours::panel2);
        g.fillEllipse (badge);

        if (done && ! active)
        {
            drawIcon (g, "check", badge.reduced (4.0f), colours::good, 2.6f);
        }
        else
        {
            g.setColour (active ? colours::ink : colours::muted);
            g.setFont (monoFont (11.0f, true));
            g.drawText (juce::String (number), badge, juce::Justification::centred);
        }

        area.removeFromLeft (8);
        g.setColour (active ? colours::text : isEnabled() ? colours::muted : colours::muted.withAlpha (0.4f));
        g.setFont (uiFont (13.5f, true));
        g.drawText (getButtonText(), area, juce::Justification::centredLeft);
    }

private:
    int number;
};

//==============================================================================
Header::Header (AlignSession& s) : session (s)
{
    const juce::StringArray labels { "Tappen", juce::String::fromUTF8 ("Pr\xc3\xbc" "fen"), "Rendern" };
    for (int i = 0; i < 3; ++i)
    {
        auto* b = steps.add (new StepButton (i + 1, labels[i]));
        b->setTitle (juce::String ("Schritt ") + juce::String (i + 1) + ": " + labels[i]);
        b->onClick = [this, i] { session.setStep ((Step) i); };
        addAndMakeVisible (b);
    }
    session.addChangeListener (this);
    update();
}

Header::~Header()
{
    session.removeChangeListener (this);
}

void Header::changeListenerCallback (juce::ChangeBroadcaster*)
{
    update();
}

void Header::update()
{
    const int current = (int) session.getStep();
    for (int i = 0; i < steps.size(); ++i)
    {
        steps[i]->active = i == current;
        steps[i]->done = i < current;
    }
    steps[1]->setEnabled (session.getMarkers().size() >= 2);
    steps[2]->setEnabled (session.canAlign());
    steps[1]->setTooltip (steps[1]->isEnabled() ? juce::String() : juce::String::fromUTF8 ("Erst mindestens 2 Marker tappen"));
    steps[2]->setTooltip (steps[2]->isEnabled() ? juce::String()
                                                : session.hasSource() ? juce::String::fromUTF8 ("Erst mindestens 2 Marker tappen")
                                                                      : juce::String::fromUTF8 ("Die Spur fehlt noch (siehe Quelle)"));
    repaint();
}

void Header::resized()
{
    auto area = getLocalBounds().reduced (20, 10);
    auto middle = area.withSizeKeepingCentre (110 * 3 + 2 * 24, area.getHeight());
    for (auto* b : steps)
    {
        b->setBounds (middle.removeFromLeft (110));
        middle.removeFromLeft (24);
    }
    tempoChip = area.removeFromRight (250).withSizeKeepingCentre (250, 32);
}

void Header::paint (juce::Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRect (getLocalBounds());
    g.setColour (colours::border);
    g.fillRect (getLocalBounds().removeFromBottom (1));

    // Logo: four tap strokes
    auto logo = juce::Rectangle<float> (20.0f, (getHeight() - 28) * 0.5f, 28.0f, 28.0f);
    g.setColour (colours::panel2);
    g.fillRoundedRectangle (logo, 7.0f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (logo, 7.0f, 1.0f);
    g.setColour (colours::accent);
    const float xs[] = { 7, 12, 17, 22 }, tops[] = { 9, 7, 9, 11 }, bottoms[] = { 19, 21, 19, 17 };
    for (int i = 0; i < 4; ++i)
        g.drawLine (logo.getX() + xs[i], logo.getY() + tops[i], logo.getX() + xs[i], logo.getY() + bottoms[i], 2.2f);

    g.setColour (colours::text);
    g.setFont (uiFont (16.0f, true));
    g.drawText ("Align my Time", juce::Rectangle<int> (58, 0, 200, getHeight()), juce::Justification::centredLeft);

    // Connectors between steps
    g.setColour (colours::border);
    for (int i = 0; i + 1 < steps.size(); ++i)
        g.fillRect (juce::Rectangle<int> (steps[i]->getRight() + 3, getHeight() / 2, 18, 1));

    // Project tempo chip
    const auto chip = tempoChip.toFloat();
    g.setColour (colours::gridDark);
    g.fillRoundedRectangle (chip, 16.0f);
    g.setColour (juce::Colour (0xff2a4a52));
    g.drawRoundedRectangle (chip.reduced (0.5f), 16.0f, 1.0f);

    auto inner = tempoChip.reduced (12, 0);
    drawIcon (g, "link", inner.removeFromLeft (14).toFloat().withSizeKeepingCentre (14.0f, 14.0f), colours::grid);
    inner.removeFromLeft (8);
    g.setColour (colours::grid);
    g.setFont (uiFont (12.0f, true));
    g.drawText ("Projekt", inner.removeFromLeft (52), juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xffbdebf4));
    g.setFont (monoFont (12.0f, true));
    g.drawText (describeProjectTempo (session.getProjectTempo()), inner, juce::Justification::centredLeft);
}

} // namespace amt::plugin::ui
