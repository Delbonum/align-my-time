#include "TempoLane.h"

namespace amt::plugin::ui
{

void TempoLane::setData (const AlignmentPlan& plan, const std::vector<double>& markerSeconds, const TempoMap& project,
                         double start, double end)
{
    bpm = plan.tappedBpm;
    starts = markerSeconds;
    average = plan.averageBpm;
    minimum = plan.minBpm;
    maximum = plan.maxBpm;
    target = project.bpmAt (plan.targetSeconds.empty() ? 0.0 : plan.targetSeconds.front());
    viewStart = start;
    viewEnd = end;
    repaint();
}

float TempoLane::timeToX (double seconds) const
{
    return (float) ((seconds - viewStart) / (viewEnd - viewStart) * getWidth());
}

void TempoLane::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::background);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

    g.setColour (colours::muted);
    g.setFont (uiFont (11.5f));
    g.drawText (tr ("Tempo je Takt"), bounds.reduced (10.0f, 5.0f), juce::Justification::topLeft);

    if (bpm.empty())
    {
        g.drawText (tr ("Erscheint, sobald mindestens zwei Marker gesetzt sind."), bounds, juce::Justification::centred);
        return;
    }

    // Vertical range: tapped range and target, with some margin.
    const double lo = juce::jmin (minimum, target) - 3.0;
    const double hi = juce::jmax (maximum, target) + 3.0;
    auto yFor = [&] (double v) { return bounds.getBottom() - 6.0f - (float) ((v - lo) / (hi - lo)) * (bounds.getHeight() - 12.0f); };

    const float targetY = yFor (target);
    juce::Path dashed;
    dashed.startNewSubPath (0.0f, targetY);
    dashed.lineTo (bounds.getRight(), targetY);
    const float dashes[] = { 5.0f, 5.0f };
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, dashed, dashes, 2);
    g.setColour (colours::grid);
    g.fillPath (dashed);

    juce::Path line;
    for (size_t i = 0; i < bpm.size() && i + 1 < starts.size(); ++i)
    {
        const float x0 = timeToX (starts[i]), x1 = timeToX (starts[i + 1]);
        const float y = yFor (bpm[i]);
        if (i == 0)
            line.startNewSubPath (x0, y);
        else
            line.lineTo (x0, y);
        line.lineTo (x1, y);
    }
    g.setColour (colours::accent);
    g.strokePath (line, juce::PathStrokeType (2.0f));

    // Target label next to its line (kept inside the lane); the average goes to the other edge.
    g.setFont (monoFont (11.0f));
    g.setColour (colours::grid);
    const float labelY = juce::jlimit (4.0f, bounds.getHeight() - 18.0f, targetY - 17.0f);
    g.drawText (tr ("Ziel") + " " + formatNumber (target, 1), juce::Rectangle<float> (bounds.getRight() - 110.0f, labelY, 100.0f, 14.0f),
                juce::Justification::centredRight);
    const bool labelAtTop = labelY < bounds.getCentreY();
    g.setColour (colours::muted);
    g.drawText (utf8 ("Ø ") + formatNumber (average, 1) + utf8 (" · ") + formatNumber (minimum, 1)
                    + utf8 ("–") + formatNumber (maximum, 1),
                bounds.reduced (10.0f, 5.0f),
                labelAtTop ? juce::Justification::bottomRight : juce::Justification::topRight);
}

} // namespace amt::plugin::ui
