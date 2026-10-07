#pragma once

#include "Localisation.h"
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
    TapUnit tapUnit = TapUnit::bar;
    float clickBlend = 0.5f;            ///< preview mix: 0 = only the track, 1 = only the click
    AlignMethod method = AlignMethod::timeStretch;
    StretchQuality quality = StretchQuality::rhythmic;
    double crossfadeMs = 10.0;
    bool snapToAttacks = true;
    double straighten = 0.0;            ///< 0..1: how far markers are pulled towards a smooth tempo curve (0 = off)
    bool leadIn = true;
    bool clickInPreview = true;
    double tapOffsetMs = 0.0;           ///< added to every tap (negative = taps are late)
    Destination destination = Destination::newTrack;
    bool exportFromProjectStart = true; ///< pad the file so it can be dropped at bar 1
    std::optional<int> firstBar;        ///< user override of the bar the first marker lands on
    bool manualTempo = false;           ///< target tempo typed in instead of taken from the host
    double manualBpm = 120.0;
    int manualNumerator = 4;
    int manualDenominator = 4;
    juce::String trackName;
};

/** Another track aligned together with this one, e.g. the other microphones of a drum recording:
    same markers, same warp, rendered sample-aligned with this track. */
struct ExtraTrack
{
    enum class Kind { file, hostTrack };

    Kind kind = Kind::file;
    juce::String id;                       ///< file path, or the host's track name (ARA)
    juce::String name;                     ///< shown in the UI and used for exported file names
    std::shared_ptr<const AudioClip> clip; ///< nullptr while loading or when missing
};

/** One rendered track. `id` is empty for this plug-in's own track. */
struct AlignedTrack
{
    juce::String id, name;
    std::shared_ptr<const AudioClip> clip;
};

/** Everything the three steps work on: the merged track, the markers, the alignment plan and
    the rendered result. Lives on the message thread; the audio thread only sees SharedClips. */
class AlignSession : public juce::ChangeBroadcaster
{
public:
    AlignSession();
    ~AlignSession() override;

    /** Back to an empty session (new project): no audio, no markers, default settings. */
    void reset();

    //==============================================================================
    // Source and project
    /** This plug-in's own track (ARA events, input recording or audio file). */
    void setSource (std::shared_ptr<const AudioClip> clip, const juce::String& description);
    std::shared_ptr<const AudioClip> getOwnTrack() const { return ownTrack; }
    const juce::String& getSourceDescription() const { return sourceDescription; }

    /** What is shown, tapped along to, snapped to and previewed: the own track, or the sum of
        all tracks when extra tracks are aligned along. */
    std::shared_ptr<const AudioClip> getSource() const { return source; }
    bool hasSource() const { return source != nullptr && ! source->isEmpty(); }

    //==============================================================================
    // Extra tracks (multitrack recordings)
    /** Adds the track, or updates the one with the same kind and id (e.g. once its audio is loaded). */
    void setExtraTrack (ExtraTrack track);
    void removeExtraTrack (ExtraTrack::Kind kind, const juce::String& id);
    const std::vector<ExtraTrack>& getExtraTracks() const { return extraTracks; }
    /** Tracks with audio, including the own one. */
    int getNumTracks() const;

    /** Tempo map reported by the host (ARA or play head). */
    void setHostTempo (const TempoMap& tempo);
    bool hasHostTempo() const { return hostTempo.has_value(); }
    const std::optional<TempoMap>& getHostTempo() const { return hostTempo; }

    /** The target the track is aligned to: the host's tempo map, or the manual tempo
        (settings.manualTempo, or whenever there is no host tempo, e.g. in the standalone app). */
    const TempoMap& getProjectTempo() const { return projectTempo; }
    bool usesManualTempo() const { return settings.manualTempo || ! hostTempo.has_value(); }
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
    /** The markers as they are aligned: the edited markers, evened out by `settings.straighten`. */
    const std::vector<Marker>& getMarkers() const { return straightenedMarkers; }
    /** How far marker `index` was moved by straightening (seconds). */
    double getStraightenShift (int index) const;
    int getNumInsertedMarkers() const;

    int getSelectedMarker() const { return selectedMarker; }
    void selectMarker (int index);
    void moveMarker (int index, double seconds);
    void nudgeSelected (double deltaSeconds);
    void addMarker (double seconds);
    void removeSelected();
    void setSnapToAttacks (bool shouldSnap);
    void setStraighten (double amount);

    const AlignmentPlan& getPlan() const { return plan; }

    /** Set when the taps fit the project tempo much better with another grid unit
        (e.g. tapped on 1 and 3 while "every one" was selected). */
    std::optional<TapUnit> getTapUnitSuggestion() const { return tapUnitSuggestion; }
    bool canAlign() const { return hasSource() && markers.size() >= 2; }

    //==============================================================================
    // Undo / redo of everything that changes the result: markers (a whole tapping pass is one
    // step), grid unit, first bar, straightening, snapping, method and target tempo.
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    void undo();
    void redo();

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

    /** The aligned audio (the sum of all tracks), or nullptr. `isAlignedUpToDate()` is false after any edit. */
    std::shared_ptr<const AudioClip> getAligned() const { return alignedForUi; }
    bool isAlignedUpToDate() const { return alignedUpToDate; }

    /** Every rendered track on its own (own track first), all starting at the same sample. */
    const std::vector<AlignedTrack>& getAlignedTracks() const { return alignedTracks; }

    /** What the track should play instead of its original audio ("replace in track"). */
    SharedClip& getReplacementSlot() { return replacement; }
    void setReplaceActive (bool active);
    bool isReplaceActive() const { return replaceActive; }

    /** Called whenever the replacement audio changes (switched on/off or re-rendered), so the
        extra host tracks can follow. */
    std::function<void()> onReplacementChanged;

    //==============================================================================
    juce::ValueTree toValueTree() const;
    void restoreFromValueTree (const juce::ValueTree& tree);

    /** Called after a state restore once the source is available again. */
    std::function<void()> onNeedsReRender;

private:
    void rebuildMarkers();
    void markChanged (bool affectsResult = true);
    void finishRender (std::vector<std::shared_ptr<const AudioClip>> results, int generation);
    void rebuildSource();
    void moveMarkerTo (int index, double seconds, bool snap);

    /** The part of the session undo restores. */
    struct EditState
    {
        std::vector<Marker> markers;
        SessionSettings settings;
    };
    EditState captureEditState() const { return { markers, settings }; }
    void applyEditState (const EditState& state);
    /** Remembers `before` for undo. Edits with the same non-empty `key` in quick succession
        (dragging a marker, moving a slider) or within one tapping pass become one step. */
    void recordUndo (const juce::String& key, EditState before);
    void recordUndo (const juce::String& key) { recordUndo (key, captureEditState()); }

    std::shared_ptr<const AudioClip> ownTrack;
    std::vector<ExtraTrack> extraTracks;
    std::shared_ptr<const AudioClip> source; ///< own track, or the sum of all tracks
    std::unique_ptr<OnsetDetector> detector;
    juce::String sourceDescription;

    void applyTempo();

    std::optional<TempoMap> hostTempo;
    TempoMap projectTempo = TempoMap::constant (120.0);
    std::shared_ptr<const TempoMap> projectTempoShared;

    std::vector<Marker> fixedMarkers;  ///< markers before the current tapping pass
    std::vector<double> liveTaps;      ///< taps of the current pass
    double tapPassStart = std::numeric_limits<double>::infinity();

    std::vector<Marker> markers;             ///< as tapped, snapped and edited
    std::vector<Marker> straightenedMarkers; ///< what is shown and aligned
    int selectedMarker = -1;
    AlignmentPlan plan;
    std::optional<TapUnit> tapUnitSuggestion;

    SessionSettings settings;
    Step step = Step::tap;

    class RenderJob;
    std::unique_ptr<RenderJob> renderJob;
    std::atomic<double> renderProgress { 0.0 };
    int generation = 0;
    std::shared_ptr<const AudioClip> alignedForUi;
    std::vector<AlignedTrack> alignedTracks;
    std::vector<AlignedTrack> renderingTracks; ///< what the running render job works on (clips = sources)
    bool alignedUpToDate = false;
    bool replaceActive = false;
    bool restoredReplaceActive = false;
    SharedClip replacement;

    std::vector<EditState> undoStack, redoStack;
    juce::String lastUndoKey;
    double lastUndoTime = 0.0;
    int tapPass = 0;

    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    JUCE_DECLARE_NON_COPYABLE (AlignSession)
};

juce::String formatBpm (double bpm);

/** "1 Takt", "½ Takt", "1 Zählzeit" ... */
juce::String describeTapUnit (TapUnit unit);

/** Where grid step `index` lands, e.g. "Takt 7" or "Takt 7, Zählzeit 3". */
juce::String describeGridPosition (const TempoMap& tempo, TapUnit unit, int firstBar, int index);
juce::String formatTime (double seconds);

} // namespace amt::plugin
