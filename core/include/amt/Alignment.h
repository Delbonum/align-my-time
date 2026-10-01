#pragma once

#include "amt/Markers.h"
#include "amt/TempoMap.h"
#include "amt/WarpMap.h"

#include <optional>
#include <vector>

namespace amt
{

struct AlignmentPlan
{
    WarpMap warp;
    int firstBar = 0;                  ///< zero-based project bar the first marker lands on
    std::vector<double> targetSeconds; ///< where each marker lands in the project
    std::vector<double> tappedBpm;     ///< tempo of the recording between marker i and i+1
    double averageBpm = 0.0;           ///< over the whole tapped range
    double minBpm = 0.0;
    double maxBpm = 0.0;
};

/** Maps markers onto the project's bar (or beat) grid.

    The first marker lands on the project bar nearest to it (or `firstBarOverride`); every
    following marker advances one bar (downbeat mode) or one beat (beat mode), following the
    project's time signatures. Requires at least two markers; with fewer an identity plan is returned. */
AlignmentPlan planAlignment (const std::vector<Marker>& markers,
                             const TempoMap& projectTempo,
                             TapMode mode,
                             std::optional<int> firstBarOverride = std::nullopt,
                             WarpMap::Ends ends = WarpMap::Ends::continueTempo);

/** Quarter-note position of grid step `index` counted from bar `firstBar`. */
double gridQuarters (const TempoMap& tempo, TapMode mode, int firstBar, int index);

} // namespace amt
