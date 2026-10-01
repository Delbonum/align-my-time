#include "amt/Alignment.h"

#include <algorithm>
#include <cmath>

namespace amt
{

double gridQuarters (const TempoMap& tempo, TapUnit unit, int firstBar, int index)
{
    switch (unit)
    {
        case TapUnit::twoBars: return tempo.barsToQuarters ((double) firstBar + 2.0 * index);
        case TapUnit::bar:     return tempo.barsToQuarters ((double) (firstBar + index));
        case TapUnit::halfBar: return tempo.barsToQuarters ((double) firstBar + 0.5 * index);
        case TapUnit::beat:
        case TapUnit::halfBeat:
        default: break;
    }

    // Beats: walk step by step so that time-signature changes are honoured.
    const double fraction = unit == TapUnit::halfBeat ? 0.5 : 1.0;
    double q = tempo.barsToQuarters ((double) firstBar);
    for (int i = 0; i < index; ++i)
        q += fraction * tempo.signatureAt (q + 1.0e-9).quartersPerBeat();
    return q;
}

std::optional<TapUnit> suggestTapUnit (const AlignmentPlan& plan, const TempoMap& projectTempo, TapUnit current)
{
    if (plan.targetSeconds.size() < 4 || plan.averageBpm <= 0.0)
        return std::nullopt;

    // Tempo the taps would imply for each unit, compared with the project tempo.
    const auto quartersOf = [&] (TapUnit u) { return gridQuarters (projectTempo, u, plan.firstBar, 1) - gridQuarters (projectTempo, u, plan.firstBar, 0); };
    const double target = projectTempo.bpmAt (plan.targetSeconds.front());
    const double currentQuarters = quartersOf (current);

    auto deviation = [&] (TapUnit u) { return std::abs (std::log (plan.averageBpm * quartersOf (u) / currentQuarters / target)); };

    TapUnit best = current;
    for (auto u : tapUnitsBySize)
        if (deviation (u) < deviation (best))
            best = u;

    // Only suggest when the current reading is far off (> ~25 %) and the other one is close (< ~12 %).
    if (best != current && deviation (current) > std::log (1.25) && deviation (best) < std::log (1.12))
        return best;
    return std::nullopt;
}

AlignmentPlan planAlignment (const std::vector<Marker>& markers, const TempoMap& projectTempo, TapUnit unit,
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
        const double q = gridQuarters (projectTempo, unit, plan.firstBar, (int) i);
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
