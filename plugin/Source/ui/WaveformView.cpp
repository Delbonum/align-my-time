#include "WaveformView.h"

namespace amt::plugin::ui
{

static constexpr int flagHeight = 22;

WaveformView::WaveformView()
{
    setRepaintsOnMouseActivity (false);
}

void WaveformView::setClip (std::shared_ptr<const AudioClip> newClip)
{
    if (newClip == clip)
        return;
    clip = std::move (newClip);
    peaksDirty = true;
    repaint();
}

void WaveformView::setTimeRange (double startSeconds, double endSeconds)
{
    if (endSeconds <= startSeconds)
        endSeconds = startSeconds + 1.0;
    if (juce::approximatelyEqual (startSeconds, viewStart) && juce::approximatelyEqual (endSeconds, viewEnd))
        return;
    viewStart = startSeconds;
    viewEnd = endSeconds;
    peaksDirty = true;
    repaint();
}

void WaveformView::setMarkers (const std::vector<Marker>& m, int sel)
{
    markers = m;
    selected = sel;
    repaint();
}

void WaveformView::setPlayhead (std::optional<double> seconds)
{
    if (playhead == seconds)
        return;
    playhead = seconds;
    repaint();
}

juce::Rectangle<int> WaveformView::waveArea() const
{
    return getLocalBounds().withTrimmedTop (flagHeight + 4);
}

float WaveformView::timeToX (double seconds) const
{
    return (float) ((seconds - viewStart) / (viewEnd - viewStart) * getWidth());
}

double WaveformView::xToTime (float x) const
{
    return viewStart + (double) x / juce::jmax (1, getWidth()) * (viewEnd - viewStart);
}

void WaveformView::updatePeaks()
{
    peaksDirty = false;
    peaks.assign ((size_t) juce::jmax (1, getWidth()), { 0.0f, 0.0f });
    if (clip == nullptr || clip->isEmpty())
        return;

    const double samplesPerPixel = (viewEnd - viewStart) * clip->sampleRate / juce::jmax (1, getWidth());
    for (size_t x = 0; x < peaks.size(); ++x)
    {
        const auto from = (int64_t) std::floor ((viewStart * clip->sampleRate) + x * samplesPerPixel);
        const auto to = (int64_t) std::floor ((viewStart * clip->sampleRate) + (x + 1) * samplesPerPixel);
        float lo = 0.0f, hi = 0.0f;
        const auto step = juce::jmax<int64_t> (1, (to - from) / 256); // subsample very long ranges
        for (auto s = juce::jmax (from, clip->startSample); s < juce::jmin (to, clip->endSample()); s += step)
        {
            for (int c = 0; c < clip->numChannels(); ++c)
            {
                const float v = clip->channels[(size_t) c][(size_t) (s - clip->startSample)];
                lo = juce::jmin (lo, v);
                hi = juce::jmax (hi, v);
            }
        }
        peaks[x] = { lo, hi };
    }
}

void WaveformView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::background);
    g.fillRoundedRectangle (bounds, 10.0f);

    const auto wave = waveArea();
    const float mid = (float) wave.getCentreY();
    const float half = wave.getHeight() * 0.5f - 4.0f;

    g.saveState();
    juce::Path clipShape;
    clipShape.addRoundedRectangle (bounds, 10.0f);
    g.reduceClipRegion (clipShape);

    g.setColour (colours::border);
    g.drawHorizontalLine ((int) mid, 0.0f, (float) getWidth());

    if (clip == nullptr || clip->isEmpty())
    {
        g.setColour (colours::muted);
        g.setFont (uiFont (13.5f));
        g.drawFittedText (placeholder, wave.reduced (24), juce::Justification::centred, 3);
    }
    else
    {
        if (peaksDirty)
            updatePeaks();

        for (auto gx : gridLines)
        {
            g.setColour (colours::grid.withAlpha (0.55f));
            g.drawVerticalLine ((int) timeToX (gx), (float) wave.getY(), (float) wave.getBottom());
        }

        juce::Path path;
        for (size_t x = 0; x < peaks.size(); ++x)
        {
            const float top = mid - juce::jmin (1.0f, peaks[x].second) * half;
            const float bottom = mid - juce::jmax (-1.0f, peaks[x].first) * half;
            path.addRectangle ((float) x, top, 1.0f, juce::jmax (1.0f, bottom - top));
        }
        g.setColour (waveColour.withAlpha (0.85f));
        g.fillPath (path);

        if (playhead.has_value() && dimAfterPlayhead)
        {
            const float px = timeToX (*playhead);
            g.setColour (colours::panel.withAlpha (0.6f));
            g.fillRect (juce::Rectangle<float> (px, (float) wave.getY(), (float) getWidth() - px, (float) wave.getHeight()));
        }
    }

    // Markers: line + numbered flag; auto-inserted markers are drawn hollow.
    for (size_t i = 0; i < markers.size(); ++i)
    {
        const auto& m = markers[i];
        const float x = timeToX (m.seconds);
        if (x < -20.0f || x > getWidth() + 20.0f)
            continue;

        const bool isSelected = (int) i == selected;
        const bool inserted = m.origin == MarkerOrigin::inserted;

        if (showBeatTicks && i + 1 < markers.size())
        {
            const float next = timeToX (markers[i + 1].seconds);
            g.setColour (colours::accent.withAlpha (0.3f));
            for (int k = 1; k < 4; ++k)
                g.drawVerticalLine ((int) (x + (next - x) * k / 4.0f), (float) wave.getY(), (float) wave.getBottom());
        }

        g.setColour (colours::accent);
        g.fillRect (juce::Rectangle<float> (x, 4.0f, 2.0f, (float) getHeight() - 4.0f));

        const auto label = juce::String (firstLabel + (int) i);
        g.setFont (monoFont (10.5f, true));
        const float w = juce::jmax (18.0f, (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), label) + 8.0f);
        const auto flag = juce::Rectangle<float> (x, 4.0f, w, 18.0f);

        juce::Path flagPath;
        flagPath.addRoundedRectangle (flag.getX(), flag.getY(), flag.getWidth(), flag.getHeight(), 4.0f, 4.0f, false, true, false, true);
        if (inserted)
        {
            g.setColour (colours::background);
            g.fillPath (flagPath);
            g.setColour (colours::accent);
            g.strokePath (flagPath, juce::PathStrokeType (1.0f));
            g.drawText (label, flag, juce::Justification::centred);
        }
        else
        {
            g.setColour (isSelected ? juce::Colours::white : colours::accent);
            g.fillPath (flagPath);
            g.setColour (colours::ink);
            g.drawText (label, flag, juce::Justification::centred);
        }

        if (isSelected)
        {
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.strokePath (flagPath, juce::PathStrokeType (3.0f));
        }
    }

    // Selected marker info bubble
    if (juce::isPositiveAndBelow (selected, (int) markers.size()) && selectedInfo.isNotEmpty())
    {
        const float x = timeToX (markers[(size_t) selected].seconds);
        g.setFont (uiFont (11.5f));
        const auto lines = juce::StringArray::fromLines (selectedInfo);
        float w = 0.0f;
        for (const auto& l : lines)
            w = juce::jmax (w, (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), l));
        auto bubble = juce::Rectangle<float> (x + 10.0f, (float) wave.getY() + 4.0f, w + 20.0f, 8.0f + lines.size() * 15.0f);
        if (bubble.getRight() > getWidth() - 4)
            bubble.setX (x - 10.0f - bubble.getWidth());

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (bubble.translated (0.0f, 3.0f), 6.0f);
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (bubble, 6.0f);
        g.setColour (colours::ink);
        for (int l = 0; l < lines.size(); ++l)
            g.drawText (lines[l], bubble.reduced (10.0f, 4.0f).withHeight (15.0f).translated (0.0f, l * 15.0f), juce::Justification::centredLeft);
    }

    if (playhead.has_value())
    {
        const float px = timeToX (*playhead);
        g.setColour (juce::Colours::white);
        g.fillRect (juce::Rectangle<float> (px, 0.0f, 2.0f, (float) getHeight()));
        juce::Path tri;
        tri.addTriangle (px - 5.0f, 0.0f, px + 7.0f, 0.0f, px + 1.0f, 8.0f);
        g.fillPath (tri);
    }

    g.restoreState();
    g.setColour (colours::border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);
}

int WaveformView::markerAt (float x) const
{
    int best = -1;
    float bestDistance = 8.0f;
    for (size_t i = 0; i < markers.size(); ++i)
    {
        const float d = std::abs (timeToX (markers[i].seconds) - x);
        if (d < bestDistance)
        {
            bestDistance = d;
            best = (int) i;
        }
    }
    return best;
}

void WaveformView::mouseMove (const juce::MouseEvent& e)
{
    if (interactive)
        setMouseCursor (markerAt (e.position.x) >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    dragging = -1;
    if (interactive)
    {
        const int hit = markerAt (e.position.x);
        if (hit >= 0)
        {
            dragging = hit;
            if (onSelectMarker)
                onSelectMarker (hit);
            return;
        }
    }

    if (onClickTime)
        onClickTime (xToTime (e.position.x));
}

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging >= 0 && onMoveMarker)
        onMoveMarker (dragging, xToTime (e.position.x));
}

void WaveformView::mouseUp (const juce::MouseEvent&)
{
    dragging = -1;
}

void WaveformView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (interactive && markerAt (e.position.x) < 0 && onAddMarker)
        onAddMarker (xToTime (e.position.x));
}

} // namespace amt::plugin::ui
