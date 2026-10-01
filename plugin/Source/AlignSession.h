#pragma once

#include "SharedClip.h"

#include <amt/Alignment.h>
#include <amt/OnsetDetector.h>
#include <amt/Renderers.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>

#include <limits>
#include <optional>

namespace amt::plugin
{

enum class Step { tap, review, render };
enum class AlignMethod { timeStretch, slices };
enum class Destination { newTrack, replaceInTrack };

struct SessionSettings
{
    TapMode tapMode = TapMode::downbeats;
    AlignMethod method = AlignMethod::timeStretch;
    StretchQuality quality = StretchQuality::rhythmic;
    double crossfadeMs = 10.0;
    bool snapToAttacks = true;
    bool leadIn = true;
    bool clickInPreview = true;
    double tapOffsetMs = 0.0;           ///< added to every tap (negative = taps are late)
    Destination destination = Destination::newTrack;
    bool exportFromProjectStart = true; ///< pad the file so it can be dropped at bar 1
    bool muteOriginal = true;
    std::optional<int> firstBar;        ///< user override of the bar the first marker lands on
    juce::String trackName;
};

/** Everything the three steps work on: the merged track, the markers, the alignment plan and
    the rendered result. Lives on the message thread; the audio thread only sees SharedClips. */
class AlignSession : public juce::ChangeBroadcaster
{
public:
    AlignSession();
    ~AlignSession() override;

    //==============================================================================
    // Source and project
    void setSource (std::shared_ptr<const AudioClip> clip, const juce::String& description);
    std::shared_ptr<const AudioClip> getSource() const { return source; }
    bool hasSource() const { return source != nullptr && ! source->isEmpty(); }
    const juce::String& getSourceDescription() const { return sourceDescription; }

    void setProjectTempo (const TempoMap& tempo);
    const TempoMap& getProjectTempo() const { return projectTempo; }
    std::shared_ptr<const TempoMap> getProjectTempoShared() const { return projectTempoShared; }

    //==============================================================================
    // Step 1: tapping
    /** Starts a tapping pass. Markers at or after `fromSeconds` are discarded, earlier ones kept. */
    void beginTapping (double fromSeconds);
    void addTap (double songSeconds);
    void undoLastTap();
    void clearMarkers();
    int getNumTapsThisPass() const { return (int) liveTaps.size(); }

    //==============================================================================
    // Step 2: review
    const std::vector<Marker>& getMarkers() const { return markers; }
    const std::vector<MarkerIssue>& getIssues() const { return issues; }
    int getNumInsertedMarkers() const;

    int getSelectedMarker() const { return selectedMarker; }
    void selectMarker (int index);
    void moveMarker (int index, double seconds);
    void nudgeSelected (double deltaSeconds);
    void addMarker (double seconds);
    void removeSelected();
    void setSnapToAttacks (bool shouldSnap);

    const AlignmentPlan& getPlan() const { return plan; }
    bool canAlign() const { return hasSource() && markers.size() >= 2; }

    //==============================================================================
    // Settings
    const SessionSettings& getSettings() const { return settings; }
    void updateSettings (const std::function<void (SessionSettings&)>& change);

    Step getStep() const { return step; }
    void setStep (Step newStep);

    //==============================================================================
    // Step 3: rendering (on a background thread)
    void startRender();
    void cancelRender();
    bool isRendering() const { return renderJob != nullptr; }
    double getRenderProgress() const { return renderProgress.load(); }

    /** The aligned audio, or nullptr. `isAlignedUpToDate()` is false after any edit. */
    std::shared_ptr<const AudioClip> getAligned() const { return alignedForUi; }
    bool isAlignedUpToDate() const { return alignedUpToDate; }

    /** What the track should play instead of its original audio ("replace in track"). */
    SharedClip& getReplacementSlot() { return replacement; }
    void setReplaceActive (bool active);
    bool isReplaceActive() const { return replacement.get() != nullptr; }

    //==============================================================================
    juce::ValueTree toValueTree() const;
    void restoreFromValueTree (const juce::ValueTree& tree);

    /** Called after a state restore once the source is available again. */
    std::function<void()> onNeedsReRender;

private:
    void rebuildMarkers();
    void markChanged (bool affectsResult = true);
    void finishRender (std::shared_ptr<const AudioClip> result, int generation);

    std::shared_ptr<const AudioClip> source;
    std::unique_ptr<OnsetDetector> detector;
    juce::String sourceDescription;

    TempoMap projectTempo = TempoMap::constant (120.0);
    std::shared_ptr<const TempoMap> projectTempoShared;

    std::vector<Marker> fixedMarkers;  ///< markers before the current tapping pass
    std::vector<double> liveTaps;      ///< taps of the current pass
    double tapPassStart = std::numeric_limits<double>::infinity();

    std::vector<Marker> markers;
    std::vector<MarkerIssue> issues;
    int selectedMarker = -1;
    AlignmentPlan plan;

    SessionSettings settings;
    Step step = Step::tap;

    class RenderJob;
    std::unique_ptr<RenderJob> renderJob;
    std::atomic<double> renderProgress { 0.0 };
    int generation = 0;
    std::shared_ptr<const AudioClip> alignedForUi;
    bool alignedUpToDate = false;
    bool restoredReplaceActive = false;
    SharedClip replacement;

    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    JUCE_DECLARE_NON_COPYABLE (AlignSession)
};

juce::String formatBpm (double bpm);
juce::String formatTime (double seconds);

} // namespace amt::plugin
