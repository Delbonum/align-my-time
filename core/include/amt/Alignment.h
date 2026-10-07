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
    following marker advances one grid unit (bar, half bar, beat ...), following the project's
    time signatures. Requires at least two markers; with fewer an identity plan is returned. */
AlignmentPlan planAlignment (const std::vector<Marker>& markers,
                             const TempoMap& projectTempo,
                             TapUnit unit,
                             std::optional<int> firstBarOverride = std::nullopt,
                             WarpMap::Ends ends = WarpMap::Ends::continueTempo);

/** Quarter-note position of grid step `index` counted from bar `firstBar`. */
double gridQuarters (const TempoMap& tempo, TapUnit unit, int firstBar, int index);

/** The tempo of the recording itself, for exporting a tempo map instead of changing the audio:
    every marker sits exactly on its grid position (as planned with `firstBar`) at its original time,
    with constant tempo between markers and the last tempo after the last one. The time before the
    first marker becomes whole bars plus, if needed, a pick-up bar in sixteenths (x/16), so the map
    starts at second 0 like a MIDI file and the first marker falls on a bar line. Time signatures
    follow the project. With fewer than two markers the project tempo is returned. */
TempoMap recordingTempoMap (const std::vector<Marker>& markers, const TempoMap& projectTempo, TapUnit unit, int firstBar);

/** The grid unit whose implied tempo best matches the project, if it is clearly better than
    `current` (e.g. tapped on 1 and 3 while "every one" was selected). */
std::optional<TapUnit> suggestTapUnit (const AlignmentPlan& plan, const TempoMap& projectTempo, TapUnit current);

} // namespace amt
