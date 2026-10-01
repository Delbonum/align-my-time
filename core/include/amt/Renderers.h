#pragma once

#include "amt/AudioClip.h"
#include "amt/WarpMap.h"

#include <functional>
#include <vector>

namespace amt
{

/** Called with progress 0..1; return false to cancel. */
using ProgressCallback = std::function<bool (double)>;

enum class StretchQuality
{
    rhythmic, ///< short analysis window: crisp transients (drums, bass, comping)
    melodic,  ///< balanced default (vocals, leads)
    complex   ///< long window: smooth sustained / polyphonic material (pads, mixes)
};

/** Target song-time range (seconds) to render. Defaults to the warped extent of the source. */
struct RenderRange
{
    double startSeconds = 0.0;
    double endSeconds = 0.0;
};

RenderRange defaultRenderRange (const AudioClip& source, const WarpMap& warp);

/** Continuously time-stretches `source` along `warp`, preserving pitch.

    `protectedAttacks` (source seconds, e.g. markers and detected onsets) are spliced in from the
    original audio, unstretched and exactly on their target position, which removes the
    phase-vocoder pre-echo and keeps drums and plucks crisp. Returns an empty clip if cancelled. */
AudioClip renderTimeStretch (const AudioClip& source, const WarpMap& warp, StretchQuality quality,
                             RenderRange range, const ProgressCallback& progress = {},
                             const std::vector<double>& protectedAttacks = {});

/** Cuts `source` at every warp anchor, moves each slice to its target position and joins
    neighbours with short raised-cosine crossfades placed just before each anchor, so attacks stay intact.
    Slices are played at original speed: gaps (slower target) fade to silence, overlaps are trimmed. */
AudioClip renderSlices (const AudioClip& source, const WarpMap& warp, double crossfadeSeconds,
                        RenderRange range, const ProgressCallback& progress = {});

} // namespace amt
