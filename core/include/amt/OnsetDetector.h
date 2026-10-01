#pragma once

#include "amt/AudioClip.h"

#include <optional>
#include <vector>

namespace amt
{

/** Finds note attacks (transients) so that tapped markers can snap onto them.
    Works on a log-energy novelty curve, which is robust for drums, bass and guitar alike
    and cheap enough to run on a whole track when it is loaded. */
class OnsetDetector
{
public:
    explicit OnsetDetector (const AudioClip& clip, int hopSize = 128);

    /** The attack closest to `seconds` (song time) within +-`windowSeconds`, weighting
        strong attacks over near ones. Returns nothing if there is no clear attack. */
    std::optional<double> findAttackNear (double seconds, double windowSeconds) const;

    /** All attacks whose novelty exceeds `threshold` (natural-log energy rise per hop). */
    std::vector<double> detectAll (double threshold = 1.5, double minSpacingSeconds = 0.05) const;

    /** Minimum novelty for an attack to count when snapping. */
    double snapThreshold = 1.0;

private:
    double refineToSample (int frame) const;

    std::vector<float> monoSignal;
    std::vector<float> novelty;
    double sampleRate;
    int64_t startSample;
    int hop;
};

} // namespace amt
