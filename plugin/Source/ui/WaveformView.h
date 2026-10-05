#pragma once

#include "LookAndFeel.h"

#include <amt/AudioClip.h>
#include <amt/Markers.h>

#include <optional>

namespace amt::plugin::ui
{

/** The whole track as one waveform, with numbered markers, an optional project grid and a
    playhead. Markers can be selected, dragged and added in the review step. */
class WaveformView : public juce::Component, public juce::SettableTooltipClient
{
public:
    WaveformView();

    void setClip (std::shared_ptr<const AudioClip> clip);
    /** Sets the whole range and shows all of it (no zoom). */
    void setTimeRange (double startSeconds, double endSeconds);
    double getStartSeconds() const { return viewStart; }
    double getEndSeconds() const { return viewEnd; }

    //==============================================================================
    // Zoom & scroll (review step)
    /** Sets the whole range but keeps the current zoom/scroll if the user has zoomed in. */
    void setFullRange (double startSeconds, double endSeconds);
    double getFullStart() const { return fullStart; }
    double getFullEnd() const { return fullEnd; }
    void setView (double startSeconds, double endSeconds);
    void zoomBy (double factor, double anchorSeconds);
    void zoomToFit() { setView (fullStart, fullEnd); }
    bool isZoomed() const { return viewEnd - viewStart < (fullEnd - fullStart) * 0.999; }
    /** Scrolls so that `seconds` is visible (page-wise, like a DAW following the playhead). */
    void keepVisible (double seconds);
    void setZoomable (bool shouldZoom) { zoomable = shouldZoom; }
    std::function<void()> onViewChanged;

    /** Review step: true = markers move only with Ctrl, a plain click plays (onClickTime);
        false = plain drag moves markers, Ctrl+click plays. */
    void setDragNeedsCtrl (bool needsCtrl) { dragNeedsCtrl = needsCtrl; }

    void setMarkers (const std::vector<Marker>& markers, int selected = -1);
    void setMarkerLabels (int firstNumber) { firstLabel = firstNumber; repaint(); }
    void setGridLines (std::vector<double> seconds) { gridLines = std::move (seconds); repaint(); }
    void setBeatTicks (bool show) { showBeatTicks = show; repaint(); }
    void setPlayhead (std::optional<double> seconds);
    void setDimAfterPlayhead (bool shouldDim) { dimAfterPlayhead = shouldDim; }
    void setWaveColour (juce::Colour c) { waveColour = c; repaint(); }
    void setInteractive (bool shouldBeInteractive) { interactive = shouldBeInteractive; }
    void setSelectedInfo (const juce::String& text) { selectedInfo = text; repaint(); }
    void setPlaceholder (const juce::String& text) { placeholder = text; repaint(); }

    std::function<void (int)> onSelectMarker;
    std::function<void (int, double)> onMoveMarker;
    std::function<void (double)> onAddMarker;
    std::function<void (double)> onClickTime;

    void paint (juce::Graphics&) override;
    void resized() override { peaksDirty = true; }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;
    void modifierKeysChanged (const juce::ModifierKeys&) override;

    float timeToX (double seconds) const;
    double xToTime (float x) const;

private:
    void updatePeaks();
    int markerAt (float x) const;
    juce::Rectangle<int> waveArea() const;

    std::shared_ptr<const AudioClip> clip;
    std::vector<std::pair<float, float>> peaks;
    bool peaksDirty = true;

    bool wantsMarkerMove (const juce::ModifierKeys& mods) const;
    void updateCursor (juce::Point<float> position, const juce::ModifierKeys& mods);

    double viewStart = 0.0, viewEnd = 1.0, fullStart = 0.0, fullEnd = 1.0;
    bool zoomable = false, dragNeedsCtrl = false;
    juce::Point<float> lastMouse;
    std::vector<Marker> markers;
    std::vector<double> gridLines;
    int selected = -1;
    int firstLabel = 1;
    bool showBeatTicks = false;
    std::optional<double> playhead;
    bool dimAfterPlayhead = false;
    bool interactive = false;
    juce::Colour waveColour = colours::wave;
    juce::String selectedInfo, placeholder;
    int dragging = -1;
};

} // namespace amt::plugin::ui
