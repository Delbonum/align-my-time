#include "AlignSession.h"

#include <algorithm>
#include <cmath>

namespace amt::plugin
{

//==============================================================================
class AlignSession::RenderJob : public juce::Thread
{
public:
    struct Input
    {
        std::vector<std::shared_ptr<const AudioClip>> tracks;
        RenderRange range;
        WarpMap warp;
        SessionSettings settings;
        std::vector<double> attacks;
    };

    using Results = std::vector<std::shared_ptr<const AudioClip>>;

    RenderJob (Input in, std::atomic<double>& progressOut, std::function<void (Results)> done)
        : juce::Thread ("Align My Time render"), input (std::move (in)), progress (progressOut), onDone (std::move (done))
    {
    }

    ~RenderJob() override { stopThread (10000); }

    void run() override
    {
        // Every track gets the same warp, attacks and range, so the results stay sample-aligned.
        Results results;
        const auto numTracks = (double) input.tracks.size();
        for (const auto& track : input.tracks)
        {
            const auto done = (double) results.size();
            auto keepGoing = [this, done, numTracks] (double p) {
                progress.store ((done + p) / numTracks);
                return ! threadShouldExit();
            };

            AudioClip result;
            if (input.settings.method == AlignMethod::timeStretch)
                result = renderTimeStretch (*track, input.warp, input.settings.quality, input.range, keepGoing, input.attacks);
            else
                result = renderSlices (*track, input.warp, input.settings.crossfadeMs / 1000.0, input.range, keepGoing);

            if (threadShouldExit())
                return;
            results.push_back (result.isEmpty() ? nullptr : std::make_shared<const AudioClip> (std::move (result)));
        }

        progress.store (1.0);
        onDone (std::move (results));
    }

private:
    Input input;
    std::atomic<double>& progress;
    std::function<void (Results)> onDone;
};

//==============================================================================
AlignSession::AlignSession()
{
    projectTempoShared = std::make_shared<const TempoMap> (projectTempo);
}

AlignSession::~AlignSession()
{
    *alive = false;
    renderJob.reset();
}

void AlignSession::reset()
{
    renderJob.reset();
    ownTrack = nullptr;
    extraTracks.clear();
    source = nullptr;
    detector = nullptr;
    sourceDescription.clear();

    markers.clear();
    fixedMarkers.clear();
    liveTaps.clear();
    tapPassStart = std::numeric_limits<double>::infinity();
    selectedMarker = -1;
    settings = {};
    step = Step::tap;

    alignedForUi = nullptr;
    alignedTracks.clear();
    renderingTracks.clear();
    alignedUpToDate = false;
    restoredReplaceActive = false;
    undoStack.clear();
    redoStack.clear();
    lastUndoKey.clear();

    setReplaceActive (false);
    applyTempo();
    markChanged();
}

void AlignSession::setSource (std::shared_ptr<const AudioClip> clip, const juce::String& description)
{
    ownTrack = std::move (clip);
    sourceDescription = description;
    rebuildSource();
}

void AlignSession::setExtraTrack (ExtraTrack track)
{
    auto existing = std::find_if (extraTracks.begin(), extraTracks.end(),
                                  [&] (const ExtraTrack& t) { return t.kind == track.kind && t.id == track.id; });
    if (existing != extraTracks.end())
        *existing = std::move (track);
    else
        extraTracks.push_back (std::move (track));
    rebuildSource();

    // Audio of a restored extra track arrived after the own track was already re-rendered.
    if (isReplaceActive() && canAlign())
        startRender();
}

void AlignSession::removeExtraTrack (ExtraTrack::Kind kind, const juce::String& id)
{
    extraTracks.erase (std::remove_if (extraTracks.begin(), extraTracks.end(),
                                       [&] (const ExtraTrack& t) { return t.kind == kind && t.id == id; }),
                       extraTracks.end());
    rebuildSource();
}

int AlignSession::getNumTracks() const
{
    int count = ownTrack != nullptr && ! ownTrack->isEmpty() ? 1 : 0;
    for (const auto& t : extraTracks)
        count += t.clip != nullptr && ! t.clip->isEmpty() ? 1 : 0;
    return count;
}

void AlignSession::rebuildSource()
{
    std::vector<std::shared_ptr<const AudioClip>> clips;
    if (ownTrack != nullptr && ! ownTrack->isEmpty())
        clips.push_back (ownTrack);
    for (const auto& t : extraTracks)
        if (t.clip != nullptr && ! t.clip->isEmpty() && (clips.empty() || juce::approximatelyEqual (t.clip->sampleRate, clips.front()->sampleRate)))
            clips.push_back (t.clip);

    if (clips.size() <= 1)
    {
        source = clips.empty() ? ownTrack : clips.front();
    }
    else
    {
        // The sum of all tracks, as the band sounds: drums snap to kick, snare and overheads together.
        int64_t start = clips.front()->startSample, end = clips.front()->endSample();
        int channels = 1;
        for (const auto& c : clips)
        {
            start = std::min (start, c->startSample);
            end = std::max (end, c->endSample());
            channels = std::max (channels, c->numChannels());
        }
        auto sum = std::make_shared<AudioClip> (channels, end - start, clips.front()->sampleRate, start);
        for (const auto& c : clips)
            sum->mixIn (*c);
        source = std::move (sum);
    }

    detector = hasSource() ? std::make_unique<OnsetDetector> (*source) : nullptr;

    // Re-snap restored or earlier markers against the new audio.
    if (detector != nullptr && settings.snapToAttacks)
        snapMarkersToAttacks (markers, *detector);
    fixedMarkers = markers;

    markChanged();

    if (restoredReplaceActive && canAlign())
        startRender();
}

namespace
{
    bool sameTempoMap (const TempoMap& tempo, const TempoMap& other)
    {
        const auto& a = tempo.points();
        const auto& b = other.points();
        const bool sameTempo = a.size() == b.size()
                               && std::equal (a.begin(), a.end(), b.begin(), [] (const TempoPoint& x, const TempoPoint& y) {
                                      return std::abs (x.seconds - y.seconds) < 1.0e-9 && std::abs (x.quarters - y.quarters) < 1.0e-9;
                                  });
        const auto& sa = tempo.signatures();
        const auto& sb = other.signatures();
        const bool sameSignatures = sa.size() == sb.size()
                                    && std::equal (sa.begin(), sa.end(), sb.begin(), [] (const TimeSignature& x, const TimeSignature& y) {
                                           return x.numerator == y.numerator && x.denominator == y.denominator
                                                  && std::abs (x.quarters - y.quarters) < 1.0e-9;
                                       });
        return sameTempo && sameSignatures;
    }
}

void AlignSession::setHostTempo (const TempoMap& tempo)
{
    if (hostTempo.has_value() && sameTempoMap (tempo, *hostTempo))
        return;

    hostTempo = tempo;
    applyTempo();
}

void AlignSession::applyTempo()
{
    auto target = usesManualTempo() ? TempoMap::constant (juce::jlimit (10.0, 999.0, settings.manualBpm), settings.manualNumerator, settings.manualDenominator)
                                    : *hostTempo;
    if (sameTempoMap (target, projectTempo))
    {
        markChanged (false);
        return;
    }

    projectTempo = std::move (target);
    projectTempoShared = std::make_shared<const TempoMap> (projectTempo);
    markChanged();
}

//==============================================================================
void AlignSession::beginTapping (double fromSeconds)
{
    // The whole pass is one undo step: from the first marker it discards, or else its first tap.
    ++tapPass;
    if (std::any_of (markers.begin(), markers.end(), [fromSeconds] (const Marker& m) { return m.seconds >= fromSeconds - 1.0e-6; }))
        recordUndo ("tap" + juce::String (tapPass));

    fixedMarkers = markers;
    fixedMarkers.erase (std::remove_if (fixedMarkers.begin(), fixedMarkers.end(),
                                        [fromSeconds] (const Marker& m) { return m.seconds >= fromSeconds - 1.0e-6; }),
                        fixedMarkers.end());
    liveTaps.clear();
    tapPassStart = fromSeconds;
    selectedMarker = -1;
    rebuildMarkers();
}

void AlignSession::addTap (double songSeconds)
{
    recordUndo ("tap" + juce::String (tapPass));
    liveTaps.push_back (songSeconds + settings.tapOffsetMs / 1000.0);
    rebuildMarkers();
}

void AlignSession::undoLastTap()
{
    if (liveTaps.empty() && fixedMarkers.empty())
        return;

    recordUndo ({});
    if (! liveTaps.empty())
        liveTaps.pop_back();
    else if (! fixedMarkers.empty())
        fixedMarkers.pop_back();
    rebuildMarkers();
}

void AlignSession::clearMarkers()
{
    if (! markers.empty())
        recordUndo ({});
    fixedMarkers.clear();
    liveTaps.clear();
    selectedMarker = -1;
    rebuildMarkers();
}

void AlignSession::rebuildMarkers()
{
    if (liveTaps.empty())
    {
        markers = fixedMarkers;
    }
    else
    {
        // Clean up the whole sequence so the reference tempo includes the earlier markers,
        // then put the earlier (possibly hand-edited) markers back in place.
        std::vector<double> all;
        for (const auto& m : fixedMarkers)
            all.push_back (m.tappedSeconds);
        all.insert (all.end(), liveTaps.begin(), liveTaps.end());

        auto cleaned = cleanUpTaps (all);
        if (detector != nullptr && settings.snapToAttacks)
            snapMarkersToAttacks (cleaned, *detector);

        const double lastFixed = fixedMarkers.empty() ? -std::numeric_limits<double>::infinity()
                                                      : fixedMarkers.back().tappedSeconds;
        markers = fixedMarkers;
        for (const auto& m : cleaned)
            if (m.tappedSeconds > lastFixed + 1.0e-6)
                markers.push_back (m);
    }

    std::sort (markers.begin(), markers.end(), [] (const Marker& a, const Marker& b) { return a.seconds < b.seconds; });
    markChanged();
}

int AlignSession::getNumInsertedMarkers() const
{
    return (int) std::count_if (markers.begin(), markers.end(), [] (const Marker& m) { return m.origin == MarkerOrigin::inserted; });
}

//==============================================================================
void AlignSession::selectMarker (int index)
{
    selectedMarker = juce::isPositiveAndBelow (index, (int) markers.size()) ? index : -1;
    sendChangeMessage();
}

void AlignSession::moveMarker (int index, double seconds)
{
    if (! juce::isPositiveAndBelow (index, (int) markers.size()))
        return;

    recordUndo ("move" + juce::String (index));
    moveMarkerTo (index, seconds, settings.snapToAttacks);
}

void AlignSession::moveMarkerTo (int index, double seconds, bool snap)
{
    // Keep the order: a marker can't be dragged past its neighbours.
    const double lower = index > 0 ? markers[(size_t) index - 1].seconds + 0.01 : -1.0e9;
    const double upper = index + 1 < (int) markers.size() ? markers[(size_t) index + 1].seconds - 0.01 : 1.0e9;

    auto& m = markers[(size_t) index];
    m.seconds = juce::jlimit (lower, upper, seconds);
    m.tappedSeconds = m.seconds;
    m.origin = MarkerOrigin::manual;
    m.snappedToAttack = false;

    if (detector != nullptr && snap)
        if (auto attack = detector->findAttackNear (m.seconds, 0.02))
            if (*attack > lower && *attack < upper)
            {
                m.seconds = *attack;
                m.snappedToAttack = true;
            }

    fixedMarkers = markers;
    liveTaps.clear();
    markChanged();
}

void AlignSession::nudgeSelected (double deltaSeconds)
{
    if (! juce::isPositiveAndBelow (selectedMarker, (int) markers.size()))
        return;

    // Nudging is precise by definition: no snapping afterwards. Starts from where the marker is shown.
    recordUndo ("move" + juce::String (selectedMarker));
    moveMarkerTo (selectedMarker, straightenedMarkers[(size_t) selectedMarker].seconds + deltaSeconds, false);
}

void AlignSession::addMarker (double seconds)
{
    recordUndo ({});
    Marker m { seconds, seconds, MarkerOrigin::manual, false };
    if (detector != nullptr && settings.snapToAttacks)
        if (auto attack = detector->findAttackNear (seconds, 0.05))
        {
            m.seconds = *attack;
            m.snappedToAttack = true;
        }

    const auto position = std::upper_bound (markers.begin(), markers.end(), m.seconds,
                                            [] (double t, const Marker& x) { return t < x.seconds; });
    selectedMarker = (int) (markers.insert (position, m) - markers.begin());
    fixedMarkers = markers;
    liveTaps.clear();
    markChanged();
}

void AlignSession::removeSelected()
{
    if (! juce::isPositiveAndBelow (selectedMarker, (int) markers.size()))
        return;

    recordUndo ({});
    markers.erase (markers.begin() + selectedMarker);
    selectedMarker = juce::jmin (selectedMarker, (int) markers.size() - 1);
    fixedMarkers = markers;
    liveTaps.clear();
    markChanged();
}

void AlignSession::setSnapToAttacks (bool shouldSnap)
{
    recordUndo ({});
    settings.snapToAttacks = shouldSnap;

    for (auto& m : markers)
    {
        if (m.origin == MarkerOrigin::manual)
            continue;
        m.seconds = m.tappedSeconds;
        m.snappedToAttack = false;
    }

    if (shouldSnap && detector != nullptr)
        snapMarkersToAttacks (markers, *detector);

    fixedMarkers = markers;
    liveTaps.clear();
    markChanged();
}

void AlignSession::setStraighten (double amount)
{
    updateSettings ([amount] (SessionSettings& s) { s.straighten = juce::jlimit (0.0, 1.0, amount); });
}

double AlignSession::getStraightenShift (int index) const
{
    if (! juce::isPositiveAndBelow (index, (int) markers.size()) || straightenedMarkers.size() != markers.size())
        return 0.0;
    return straightenedMarkers[(size_t) index].seconds - markers[(size_t) index].seconds;
}

//==============================================================================
void AlignSession::updateSettings (const std::function<void (SessionSettings&)>& change)
{
    const auto before = settings;
    change (settings);

    const bool affectsResult = before.tapUnit != settings.tapUnit || before.method != settings.method
                               || before.quality != settings.quality || ! juce::approximatelyEqual (before.crossfadeMs, settings.crossfadeMs)
                               || before.firstBar != settings.firstBar || ! juce::approximatelyEqual (before.straighten, settings.straighten);
    const bool tempoChanged = before.manualTempo != settings.manualTempo || ! juce::approximatelyEqual (before.manualBpm, settings.manualBpm)
                              || before.manualNumerator != settings.manualNumerator || before.manualDenominator != settings.manualDenominator;

    // Sliders and the tempo editor send a stream of changes: one undo step per gesture.
    if (affectsResult || tempoChanged)
        recordUndo ("settings", { markers, before });

    if (tempoChanged)
    {
        applyTempo(); // marks the result as changed if the target actually moved
        if (affectsResult)
            markChanged (true);
        return;
    }

    markChanged (affectsResult);
}

void AlignSession::setStep (Step newStep)
{
    if (step == newStep)
        return;

    // Leaving the tap step commits the pass.
    fixedMarkers = markers;
    liveTaps.clear();
    tapPassStart = std::numeric_limits<double>::infinity();

    step = newStep;
    sendChangeMessage();
}

void AlignSession::markChanged (bool affectsResult)
{
    plan = planAlignment (markers, projectTempo, settings.tapUnit, settings.firstBar);

    straightenedMarkers = markers;
    if (settings.straighten > 0.0 && markers.size() >= 4)
    {
        // Straighten along the grid the markers land on, then plan with the evened-out positions.
        std::vector<double> grid;
        for (size_t i = 0; i < markers.size(); ++i)
            grid.push_back (gridQuarters (projectTempo, settings.tapUnit, plan.firstBar, (int) i));

        // With snapping on, the audio's attacks guide the straightening as well (hitpoints).
        std::vector<bool> onAttack;
        const auto positions = straightenMarkers (markers, grid, settings.straighten, 4,
                                                  settings.snapToAttacks ? detector.get() : nullptr, &onAttack);
        for (size_t i = 0; i < markers.size(); ++i)
        {
            if (std::abs (positions[i] - markers[i].seconds) > 1.0e-9)
                straightenedMarkers[i].snappedToAttack = onAttack[i];
            straightenedMarkers[i].seconds = positions[i];
        }
        plan = planAlignment (straightenedMarkers, projectTempo, settings.tapUnit, plan.firstBar);
    }
    tapUnitSuggestion = suggestTapUnit (plan, projectTempo, settings.tapUnit);

    if (affectsResult)
    {
        ++generation;
        alignedUpToDate = false;
    }
    sendChangeMessage();
}

//==============================================================================
void AlignSession::recordUndo (const juce::String& key, EditState before)
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const bool sameGesture = key.isNotEmpty() && key == lastUndoKey && (key.startsWith ("tap") || now - lastUndoTime < 1500.0);
    lastUndoKey = key;
    lastUndoTime = now;
    if (sameGesture)
        return;

    undoStack.push_back (std::move (before));
    if (undoStack.size() > 200)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void AlignSession::applyEditState (const EditState& state)
{
    markers = state.markers;
    fixedMarkers = markers;
    liveTaps.clear();
    tapPassStart = std::numeric_limits<double>::infinity();
    selectedMarker = juce::jmin (selectedMarker, (int) markers.size() - 1);
    lastUndoKey.clear();

    // Only what changes the result; preview and export options stay as they are.
    const auto& s = state.settings;
    settings.tapUnit = s.tapUnit;
    settings.firstBar = s.firstBar;
    settings.straighten = s.straighten;
    settings.snapToAttacks = s.snapToAttacks;
    settings.method = s.method;
    settings.quality = s.quality;
    settings.crossfadeMs = s.crossfadeMs;
    settings.manualTempo = s.manualTempo;
    settings.manualBpm = s.manualBpm;
    settings.manualNumerator = s.manualNumerator;
    settings.manualDenominator = s.manualDenominator;

    applyTempo();
    markChanged();
}

void AlignSession::undo()
{
    if (undoStack.empty())
        return;
    redoStack.push_back (captureEditState());
    const auto state = std::move (undoStack.back());
    undoStack.pop_back();
    applyEditState (state);
}

void AlignSession::redo()
{
    if (redoStack.empty())
        return;
    undoStack.push_back (captureEditState());
    const auto state = std::move (redoStack.back());
    redoStack.pop_back();
    applyEditState (state);
}

//==============================================================================
void AlignSession::startRender()
{
    if (! canAlign())
        return;

    cancelRender();

    RenderJob::Input input { {}, defaultRenderRange (*source, plan.warp), plan.warp, settings, {} };
    std::vector<AlignedTrack> tracks;
    if (ownTrack != nullptr && ! ownTrack->isEmpty())
        tracks.push_back ({ {}, {}, ownTrack });
    for (const auto& t : extraTracks)
        if (t.clip != nullptr && ! t.clip->isEmpty() && juce::approximatelyEqual (t.clip->sampleRate, source->sampleRate))
            tracks.push_back ({ t.id, t.name, t.clip });
    for (const auto& t : tracks)
        input.tracks.push_back (t.clip);

    if (settings.method == AlignMethod::timeStretch)
    {
        for (const auto& m : markers)
            input.attacks.push_back (m.seconds);
        if (detector != nullptr)
            for (auto onset : detector->detectAll (2.0, 0.08))
                input.attacks.push_back (onset);

        // Markers and detected onsets describe the same attacks: keep one of each pair.
        std::sort (input.attacks.begin(), input.attacks.end());
        input.attacks.erase (std::unique (input.attacks.begin(), input.attacks.end(), [] (double a, double b) { return b - a < 0.03; }),
                             input.attacks.end());
    }

    renderProgress.store (0.0);
    const int renderGeneration = generation;
    std::weak_ptr<bool> weakAlive = alive;

    renderJob = std::make_unique<RenderJob> (std::move (input), renderProgress, [this, weakAlive, renderGeneration] (RenderJob::Results results) {
        juce::MessageManager::callAsync ([this, weakAlive, renderGeneration, results] {
            if (auto a = weakAlive.lock(); a != nullptr && *a)
                finishRender (results, renderGeneration);
        });
    });
    renderingTracks = std::move (tracks);
    renderJob->startThread();
    sendChangeMessage();
}

void AlignSession::cancelRender()
{
    renderJob.reset();
    sendChangeMessage();
}

void AlignSession::finishRender (std::vector<std::shared_ptr<const AudioClip>> results, int renderGeneration)
{
    renderJob.reset();

    const bool complete = ! results.empty() && results.size() == renderingTracks.size()
                          && std::all_of (results.begin(), results.end(), [] (const auto& r) { return r != nullptr; });
    if (complete)
    {
        alignedTracks = renderingTracks;
        for (size_t i = 0; i < results.size(); ++i)
            alignedTracks[i].clip = results[i];

        if (results.size() == 1)
        {
            alignedForUi = results.front();
        }
        else
        {
            // All results share start and length (same warp and range).
            auto sum = std::make_shared<AudioClip> (results.front()->numChannels(), results.front()->numSamples(),
                                                    results.front()->sampleRate, results.front()->startSample);
            for (const auto& r : results)
                sum->mixIn (*r);
            alignedForUi = std::move (sum);
        }
        alignedUpToDate = renderGeneration == generation;

        if (restoredReplaceActive || isReplaceActive())
        {
            restoredReplaceActive = false;
            setReplaceActive (true);
            return;
        }
    }

    sendChangeMessage();
}

void AlignSession::setReplaceActive (bool active)
{
    replaceActive = active && ! alignedTracks.empty();
    const bool ownTrackRendered = replaceActive && alignedTracks.front().id.isEmpty();
    replacement.set (ownTrackRendered ? alignedTracks.front().clip : nullptr);
    if (onReplacementChanged)
        onReplacementChanged();
    sendChangeMessage();
}

//==============================================================================
namespace ids
{
    static const juce::Identifier root ("AlignMyTime"), markers ("Markers"), marker ("Marker"), seconds ("seconds"),
        tapped ("tapped"), origin ("origin"), snapped ("snapped"), tapMode ("tapMode"), tapUnit ("tapUnit"), manualTempo ("manualTempo"), manualBpm ("manualBpm"),
        manualNumerator ("manualNumerator"), manualDenominator ("manualDenominator"), clickBlend ("clickBlend"), method ("method"), quality ("quality"),
        crossfade ("crossfadeMs"), snap ("snapToAttacks"), straighten ("straighten"), leadIn ("leadIn"), click ("clickInPreview"), tapOffset ("tapOffsetMs"),
        destination ("destination"), fromStart ("exportFromProjectStart"), firstBar ("firstBar"),
        trackName ("trackName"), step ("step"), replace ("replaceActive"), version ("version"),
        extraTracks ("ExtraTracks"), extraTrack ("ExtraTrack"), kind ("kind"), id ("id"), name ("name");
}

juce::ValueTree AlignSession::toValueTree() const
{
    juce::ValueTree tree (ids::root);
    tree.setProperty (ids::version, 2, nullptr);
    tree.setProperty (ids::tapUnit, (int) settings.tapUnit, nullptr);
    tree.setProperty (ids::clickBlend, settings.clickBlend, nullptr);
    tree.setProperty (ids::manualTempo, settings.manualTempo, nullptr);
    tree.setProperty (ids::manualBpm, settings.manualBpm, nullptr);
    tree.setProperty (ids::manualNumerator, settings.manualNumerator, nullptr);
    tree.setProperty (ids::manualDenominator, settings.manualDenominator, nullptr);
    tree.setProperty (ids::method, (int) settings.method, nullptr);
    tree.setProperty (ids::quality, (int) settings.quality, nullptr);
    tree.setProperty (ids::crossfade, settings.crossfadeMs, nullptr);
    tree.setProperty (ids::snap, settings.snapToAttacks, nullptr);
    tree.setProperty (ids::straighten, settings.straighten, nullptr);
    tree.setProperty (ids::leadIn, settings.leadIn, nullptr);
    tree.setProperty (ids::click, settings.clickInPreview, nullptr);
    tree.setProperty (ids::tapOffset, settings.tapOffsetMs, nullptr);
    tree.setProperty (ids::destination, (int) settings.destination, nullptr);
    tree.setProperty (ids::fromStart, settings.exportFromProjectStart, nullptr);
    tree.setProperty (ids::firstBar, settings.firstBar.has_value() ? *settings.firstBar : -100000, nullptr);
    tree.setProperty (ids::trackName, settings.trackName, nullptr);
    tree.setProperty (ids::step, (int) step, nullptr);
    tree.setProperty (ids::replace, isReplaceActive() || restoredReplaceActive, nullptr);

    juce::ValueTree markerList (ids::markers);
    for (const auto& m : markers)
    {
        juce::ValueTree node (ids::marker);
        node.setProperty (ids::seconds, m.seconds, nullptr);
        node.setProperty (ids::tapped, m.tappedSeconds, nullptr);
        node.setProperty (ids::origin, (int) m.origin, nullptr);
        node.setProperty (ids::snapped, m.snappedToAttack, nullptr);
        markerList.appendChild (node, nullptr);
    }
    tree.appendChild (markerList, nullptr);

    // Only the references: the audio comes from the files or the host again.
    juce::ValueTree trackList (ids::extraTracks);
    for (const auto& t : extraTracks)
    {
        juce::ValueTree node (ids::extraTrack);
        node.setProperty (ids::kind, (int) t.kind, nullptr);
        node.setProperty (ids::id, t.id, nullptr);
        node.setProperty (ids::name, t.name, nullptr);
        trackList.appendChild (node, nullptr);
    }
    tree.appendChild (trackList, nullptr);
    return tree;
}

void AlignSession::restoreFromValueTree (const juce::ValueTree& tree)
{
    if (! tree.hasType (ids::root))
        return;

    // Version 1 stored "tapMode" (0 = every one, 1 = every beat).
    const int legacyMode = tree.getProperty (ids::tapMode, 0);
    settings.tapUnit = (TapUnit) (int) tree.getProperty (ids::tapUnit, (int) (legacyMode == 1 ? TapUnit::beat : TapUnit::bar));
    settings.clickBlend = tree.getProperty (ids::clickBlend, 0.5f);
    settings.manualTempo = tree.getProperty (ids::manualTempo, settings.manualTempo);
    settings.manualBpm = tree.getProperty (ids::manualBpm, 120.0);
    settings.manualNumerator = juce::jlimit (1, 32, (int) tree.getProperty (ids::manualNumerator, 4));
    settings.manualDenominator = juce::jlimit (1, 32, (int) tree.getProperty (ids::manualDenominator, 4));
    settings.method = (AlignMethod) (int) tree.getProperty (ids::method, 0);
    settings.quality = (StretchQuality) (int) tree.getProperty (ids::quality, 0);
    settings.crossfadeMs = tree.getProperty (ids::crossfade, 10.0);
    settings.snapToAttacks = tree.getProperty (ids::snap, true);
    settings.straighten = juce::jlimit (0.0, 1.0, (double) tree.getProperty (ids::straighten, 0.0));
    settings.leadIn = tree.getProperty (ids::leadIn, true);
    settings.clickInPreview = tree.getProperty (ids::click, true);
    settings.tapOffsetMs = tree.getProperty (ids::tapOffset, 0.0);
    settings.destination = (Destination) (int) tree.getProperty (ids::destination, 0);
    settings.exportFromProjectStart = tree.getProperty (ids::fromStart, true);
    const int firstBar = tree.getProperty (ids::firstBar, -100000);
    settings.firstBar = firstBar > -100000 ? std::optional<int> (firstBar) : std::nullopt;
    settings.trackName = tree.getProperty (ids::trackName).toString();
    step = (Step) juce::jlimit (0, 2, (int) tree.getProperty (ids::step, 0));
    restoredReplaceActive = tree.getProperty (ids::replace, false);

    markers.clear();
    for (const auto& node : tree.getChildWithName (ids::markers))
        markers.push_back ({ node.getProperty (ids::seconds), node.getProperty (ids::tapped),
                             (MarkerOrigin) (int) node.getProperty (ids::origin, 0), node.getProperty (ids::snapped, false) });

    extraTracks.clear();
    for (const auto& node : tree.getChildWithName (ids::extraTracks))
        extraTracks.push_back ({ (ExtraTrack::Kind) juce::jlimit (0, 1, (int) node.getProperty (ids::kind, 0)),
                                 node.getProperty (ids::id).toString(), node.getProperty (ids::name).toString(), nullptr });

    fixedMarkers = markers;
    liveTaps.clear();
    selectedMarker = -1;
    undoStack.clear();
    redoStack.clear();
    lastUndoKey.clear();
    applyTempo();
    markChanged();

    if (restoredReplaceActive && canAlign())
        startRender();
}

//==============================================================================
juce::String formatBpm (double bpm)
{
    return formatNumber (bpm, 2);
}

juce::String describeTapUnit (TapUnit unit)
{
    switch (unit)
    {
        case TapUnit::twoBars:  return tr ("2 Takte");
        case TapUnit::bar:      return tr ("1 Takt");
        case TapUnit::halfBar:  return tr ("½ Takt");
        case TapUnit::beat:     return tr ("1 Zählzeit");
        case TapUnit::halfBeat:
        default:                return tr ("½ Zählzeit");
    }
}

juce::String describeGridPosition (const TempoMap& tempo, TapUnit unit, int firstBar, int index)
{
    const double q = gridQuarters (tempo, unit, firstBar, index);
    const double bars = tempo.quartersToBars (q);
    const double barStart = std::floor (bars + 1.0e-6);
    auto text = tr ("Takt") + " " + juce::String ((int) barStart + 1);

    const auto& sig = tempo.signatureAt (q);
    const double beat = (q - tempo.barsToQuarters (barStart)) / sig.quartersPerBeat();
    if (beat > 1.0e-6)
    {
        const bool whole = std::abs (beat - std::round (beat)) < 1.0e-6;
        text << tr (", Zählzeit ") << (whole ? juce::String ((int) std::round (beat) + 1)
                                                                    : formatNumber (beat + 1.0, 1));
    }
    return text;
}

juce::String formatTime (double seconds)
{
    const bool negative = seconds < 0.0;
    seconds = std::abs (seconds);
    const int minutes = (int) (seconds / 60.0);
    const double rest = seconds - minutes * 60.0;
    return (negative ? "-" : "") + juce::String (minutes) + ":" + formatNumber (rest, 1).paddedLeft ('0', 4);
}

} // namespace amt::plugin
