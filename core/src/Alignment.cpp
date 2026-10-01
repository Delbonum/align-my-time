#include "amt/Alignment.h"

#include <algorithm>
#include <cmath>

namespace amt
{

double gridQuarters (const TempoMap& tempo, TapMode mode, int firstBar, int index)
{
    if (mode == TapMode::downbeats)
        return tempo.barsToQuarters ((double) (firstBar + index));

    // Beat mode: walk beat by beat so that time-signature changes are honoured.
    double q = tempo.barsToQuarters ((double) firstBar);
    for (int i = 0; i < index; ++i)
        q += tempo.signatureAt (q + 1.0e-9).quartersPerBeat();
    return q;
}

AlignmentPlan planAlignment (const std::vector<Marker>& markers, const TempoMap& projectTempo, TapMode mode,
                             std::optional<int> firstBarOverride, WarpMap::Ends ends)
{
    AlignmentPlan plan;
    if (markers.size() < 2)
        return plan;

    std::vector<double> sources;
    for (const auto& m : markers)
        sources.push_back (m.seconds);
    std::sort (sources.begin(), sources.end());

    plan.firstBar = firstBarOverride.has_value()
                        ? *firstBarOverride
                        : (int) std::lround (projectTempo.quartersToBars (projectTempo.secondsToQuarters (sources.front())));

    std::vector<WarpAnchor> anchors;
    std::vector<double> quarters;
    for (size_t i = 0; i < sources.size(); ++i)
    {
        const double q = gridQuarters (projectTempo, mode, plan.firstBar, (int) i);
        quarters.push_back (q);
        const double target = projectTempo.quartersToSeconds (q);
        plan.targetSeconds.push_back (target);
        anchors.push_back ({ sources[i], target });
    }

    plan.warp = WarpMap (std::move (anchors), ends);

    plan.minBpm = 1.0e9;
    plan.maxBpm = 0.0;
    for (size_t i = 0; i + 1 < sources.size(); ++i)
    {
        const double duration = sources[i + 1] - sources[i];
        const double bpm = duration > 0.0 ? 60.0 * (quarters[i + 1] - quarters[i]) / duration : 0.0;
        plan.tappedBpm.push_back (bpm);
        plan.minBpm = std::min (plan.minBpm, bpm);
        plan.maxBpm = std::max (plan.maxBpm, bpm);
    }

    const double totalDuration = sources.back() - sources.front();
    plan.averageBpm = totalDuration > 0.0 ? 60.0 * (quarters.back() - quarters.front()) / totalDuration : 0.0;
    return plan;
}

} // namespace amt
