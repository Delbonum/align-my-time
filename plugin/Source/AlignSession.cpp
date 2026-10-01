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
        std::shared_ptr<const AudioClip> source;
        WarpMap warp;
        SessionSettings settings;
        std::vector<double> attacks;
    };

    RenderJob (Input in, std::atomic<double>& progressOut, std::function<void (std::shared_ptr<const AudioClip>)> done)
        : juce::Thread ("Align my Time render"), input (std::move (in)), progress (progressOut), onDone (std::move (done))
    {
    }

    ~RenderJob() override { stopThread (10000); }

    void run() override
    {
        auto keepGoing = [this] (double p) {
            progress.store (p);
            return ! threadShouldExit();
        };

        const auto range = defaultRenderRange (*input.source, input.warp);
        AudioClip result;
        if (input.settings.method == AlignMethod::timeStretch)
            result = renderTimeStretch (*input.source, input.warp, input.settings.quality, range, keepGoing, input.attacks);
        else
            result = renderSlices (*input.source, input.warp, input.settings.crossfadeMs / 1000.0, range, keepGoing);

        if (threadShouldExit())
            return;

        progress.store (1.0);
        onDone (result.isEmpty() ? nullptr : std::make_shared<const AudioClip> (std::move (result)));
    }

private:
    Input input;
    std::atomic<double>& progress;
    std::function<void (std::shared_ptr<const AudioClip>)> onDone;
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

void AlignSession::setSource (std::shared_ptr<const AudioClip> clip, const juce::String& description)
{
    source = std::move (clip);
    sourceDescription = description;
    detector = hasSource() ? std::make_unique<OnsetDetector> (*source) : nullptr;

    // Re-snap restored or earlier markers against the new audio.
    if (detector != nullptr && settings.snapToAttacks)
        snapMarkersToAttacks (markers, *detector);
    fixedMarkers = markers;

    markChanged();

    if (restoredReplaceActive && canAlign())
        startRender();
}

void AlignSession::setProjectTempo (const TempoMap& tempo)
{
    const auto& a = tempo.points();
    const auto& b = projectTempo.points();
    const bool sameTempo = a.size() == b.size()
                           && std::equal (a.begin(), a.end(), b.begin(), [] (const TempoPoint& x, const TempoPoint& y) {
                                  return std::abs (x.seconds - y.seconds) < 1.0e-9 && std::abs (x.quarters - y.quarters) < 1.0e-9;
                              });
    const auto& sa = tempo.signatures();
    const auto& sb = projectTempo.signatures();
    const bool sameSignatures = sa.size() == sb.size()
                                && std::equal (sa.begin(), sa.end(), sb.begin(), [] (const TimeSignature& x, const TimeSignature& y) {
                                       return x.numerator == y.numerator && x.denominator == y.denominator
                                              && std::abs (x.quarters - y.quarters) < 1.0e-9;
                                   });
    if (sameTempo && sameSignatures)
        return;

    projectTempo = tempo;
    projectTempoShared = std::make_shared<const TempoMap> (projectTempo);
    markChanged();
}

//==============================================================================
void AlignSession::beginTapping (double fromSeconds)
{
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
    liveTaps.push_back (songSeconds + settings.tapOffsetMs / 1000.0);
    rebuildMarkers();
}

void AlignSession::undoLastTap()
{
    if (! liveTaps.empty())
        liveTaps.pop_back();
    else if (! fixedMarkers.empty())
        fixedMarkers.pop_back();
    rebuildMarkers();
}

void AlignSession::clearMarkers()
{
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
        issues.clear();
    }
    else
    {
        // Clean up the whole sequence so the reference tempo includes the earlier markers,
        // then put the earlier (possibly hand-edited) markers back in place.
        std::vector<double> all;
        for (const auto& m : fixedMarkers)
            all.push_back (m.tappedSeconds);
        all.insert (all.end(), liveTaps.begin(), liveTaps.end());

        auto cleaned = cleanUpTaps (all, {}, &issues);
        if (detector != nullptr && settings.snapToAttacks)
            snapMarkersToAttacks (cleaned, *detector);

        const double lastFixed = fixedMarkers.empty() ? -std::numeric_limits<double>::infinity()
                                                      : fixedMarkers.back().tappedSeconds;
        markers = fixedMarkers;
        for (const auto& m : cleaned)
            if (m.tappedSeconds > lastFixed + 1.0e-6)
                markers.push_back (m);
        // Note: issue indices refer to the cleaned sequence; the UI only shows their count.
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

    // Keep the order: a marker can't be dragged past its neighbours.
    const double lower = index > 0 ? markers[(size_t) index - 1].seconds + 0.01 : -1.0e9;
    const double upper = index + 1 < (int) markers.size() ? markers[(size_t) index + 1].seconds - 0.01 : 1.0e9;

    auto& m = markers[(size_t) index];
    m.seconds = juce::jlimit (lower, upper, seconds);
    m.tappedSeconds = m.seconds;
    m.origin = MarkerOrigin::manual;
    m.snappedToAttack = false;

    if (detector != nullptr && settings.snapToAttacks)
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
    if (selectedMarker < 0)
        return;

    // Nudging is precise by definition: no snapping afterwards.
    const bool snap = settings.snapToAttacks;
    settings.snapToAttacks = false;
    moveMarker (selectedMarker, markers[(size_t) selectedMarker].seconds + deltaSeconds);
    settings.snapToAttacks = snap;
}

void AlignSession::addMarker (double seconds)
{
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
    if (selectedMarker < 0)
        return;

    markers.erase (markers.begin() + selectedMarker);
    selectedMarker = juce::jmin (selectedMarker, (int) markers.size() - 1);
    fixedMarkers = markers;
    liveTaps.clear();
    markChanged();
}

void AlignSession::setSnapToAttacks (bool shouldSnap)
{
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

//==============================================================================
void AlignSession::updateSettings (const std::function<void (SessionSettings&)>& change)
{
    const auto before = settings;
    change (settings);

    const bool affectsResult = before.tapUnit != settings.tapUnit || before.method != settings.method
                               || before.quality != settings.quality || ! juce::approximatelyEqual (before.crossfadeMs, settings.crossfadeMs)
                               || before.firstBar != settings.firstBar;
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
    tapUnitSuggestion = suggestTapUnit (plan, projectTempo, settings.tapUnit);

    if (affectsResult)
    {
        ++generation;
        alignedUpToDate = false;
    }
    sendChangeMessage();
}

//==============================================================================
void AlignSession::startRender()
{
    if (! canAlign())
        return;

    cancelRender();

    RenderJob::Input input { source, plan.warp, settings, {} };

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

    renderJob = std::make_unique<RenderJob> (std::move (input), renderProgress, [this, weakAlive, renderGeneration] (std::shared_ptr<const AudioClip> result) {
        juce::MessageManager::callAsync ([this, weakAlive, renderGeneration, result] {
            if (auto a = weakAlive.lock(); a != nullptr && *a)
                finishRender (result, renderGeneration);
        });
    });
    renderJob->startThread();
    sendChangeMessage();
}

void AlignSession::cancelRender()
{
    renderJob.reset();
    sendChangeMessage();
}

void AlignSession::finishRender (std::shared_ptr<const AudioClip> result, int renderGeneration)
{
    renderJob.reset();

    if (result != nullptr)
    {
        alignedForUi = result;
        alignedUpToDate = renderGeneration == generation;

        if (restoredReplaceActive || isReplaceActive())
        {
            restoredReplaceActive = false;
            replacement.set (result);
        }
    }

    sendChangeMessage();
}

void AlignSession::setReplaceActive (bool active)
{
    replacement.set (active ? alignedForUi : nullptr);
    sendChangeMessage();
}

//==============================================================================
namespace ids
{
    static const juce::Identifier root ("AlignMyTime"), markers ("Markers"), marker ("Marker"), seconds ("seconds"),
        tapped ("tapped"), origin ("origin"), snapped ("snapped"), tapMode ("tapMode"), tapUnit ("tapUnit"), clickBlend ("clickBlend"), method ("method"), quality ("quality"),
        crossfade ("crossfadeMs"), snap ("snapToAttacks"), leadIn ("leadIn"), click ("clickInPreview"), tapOffset ("tapOffsetMs"),
        destination ("destination"), fromStart ("exportFromProjectStart"), muteOriginal ("muteOriginal"), firstBar ("firstBar"),
        trackName ("trackName"), step ("step"), replace ("replaceActive"), version ("version");
}

juce::ValueTree AlignSession::toValueTree() const
{
    juce::ValueTree tree (ids::root);
    tree.setProperty (ids::version, 2, nullptr);
    tree.setProperty (ids::tapUnit, (int) settings.tapUnit, nullptr);
    tree.setProperty (ids::clickBlend, settings.clickBlend, nullptr);
    tree.setProperty (ids::method, (int) settings.method, nullptr);
    tree.setProperty (ids::quality, (int) settings.quality, nullptr);
    tree.setProperty (ids::crossfade, settings.crossfadeMs, nullptr);
    tree.setProperty (ids::snap, settings.snapToAttacks, nullptr);
    tree.setProperty (ids::leadIn, settings.leadIn, nullptr);
    tree.setProperty (ids::click, settings.clickInPreview, nullptr);
    tree.setProperty (ids::tapOffset, settings.tapOffsetMs, nullptr);
    tree.setProperty (ids::destination, (int) settings.destination, nullptr);
    tree.setProperty (ids::fromStart, settings.exportFromProjectStart, nullptr);
    tree.setProperty (ids::muteOriginal, settings.muteOriginal, nullptr);
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
    settings.method = (AlignMethod) (int) tree.getProperty (ids::method, 0);
    settings.quality = (StretchQuality) (int) tree.getProperty (ids::quality, 0);
    settings.crossfadeMs = tree.getProperty (ids::crossfade, 10.0);
    settings.snapToAttacks = tree.getProperty (ids::snap, true);
    settings.leadIn = tree.getProperty (ids::leadIn, true);
    settings.clickInPreview = tree.getProperty (ids::click, true);
    settings.tapOffsetMs = tree.getProperty (ids::tapOffset, 0.0);
    settings.destination = (Destination) (int) tree.getProperty (ids::destination, 0);
    settings.exportFromProjectStart = tree.getProperty (ids::fromStart, true);
    settings.muteOriginal = tree.getProperty (ids::muteOriginal, true);
    const int firstBar = tree.getProperty (ids::firstBar, -100000);
    settings.firstBar = firstBar > -100000 ? std::optional<int> (firstBar) : std::nullopt;
    settings.trackName = tree.getProperty (ids::trackName).toString();
    step = (Step) juce::jlimit (0, 2, (int) tree.getProperty (ids::step, 0));
    restoredReplaceActive = tree.getProperty (ids::replace, false);

    markers.clear();
    for (const auto& node : tree.getChildWithName (ids::markers))
        markers.push_back ({ node.getProperty (ids::seconds), node.getProperty (ids::tapped),
                             (MarkerOrigin) (int) node.getProperty (ids::origin, 0), node.getProperty (ids::snapped, false) });

    fixedMarkers = markers;
    liveTaps.clear();
    selectedMarker = -1;
    markChanged();

    if (restoredReplaceActive && canAlign())
        startRender();
}

//==============================================================================
juce::String formatBpm (double bpm)
{
    return juce::String (bpm, 2).replaceCharacter ('.', ',');
}

juce::String describeTapUnit (TapUnit unit)
{
    switch (unit)
    {
        case TapUnit::twoBars:  return "2 Takte";
        case TapUnit::bar:      return "1 Takt";
        case TapUnit::halfBar:  return juce::String::fromUTF8 ("\xc2\xbd Takt");
        case TapUnit::beat:     return juce::String::fromUTF8 ("1 Z\xc3\xa4hlzeit");
        case TapUnit::halfBeat:
        default:                return juce::String::fromUTF8 ("\xc2\xbd Z\xc3\xa4hlzeit");
    }
}

juce::String describeGridPosition (const TempoMap& tempo, TapUnit unit, int firstBar, int index)
{
    const double q = gridQuarters (tempo, unit, firstBar, index);
    const double bars = tempo.quartersToBars (q);
    const double barStart = std::floor (bars + 1.0e-6);
    auto text = "Takt " + juce::String ((int) barStart + 1);

    const auto& sig = tempo.signatureAt (q);
    const double beat = (q - tempo.barsToQuarters (barStart)) / sig.quartersPerBeat();
    if (beat > 1.0e-6)
    {
        const bool whole = std::abs (beat - std::round (beat)) < 1.0e-6;
        text << juce::String::fromUTF8 (", Z\xc3\xa4hlzeit ") << (whole ? juce::String ((int) std::round (beat) + 1)
                                                                    : juce::String (beat + 1.0, 1).replaceCharacter ('.', ','));
    }
    return text;
}

juce::String formatTime (double seconds)
{
    const bool negative = seconds < 0.0;
    seconds = std::abs (seconds);
    const int minutes = (int) (seconds / 60.0);
    const double rest = seconds - minutes * 60.0;
    return (negative ? "-" : "") + juce::String (minutes) + ":" + juce::String (rest, 1).paddedLeft ('0', 4).replaceCharacter ('.', ',');
}

} // namespace amt::plugin
