#pragma once

#include <vector>

namespace amt
{

/** A pair of matching positions: material at `source` seconds should sound at `target` seconds. */
struct WarpAnchor
{
    double source = 0.0;
    double target = 0.0;
};

/** Piecewise-linear, strictly monotonic mapping between source time and target time. */
class WarpMap
{
public:
    /** What happens before the first and after the last anchor. */
    enum class Ends
    {
        continueTempo,     ///< keep the stretch factor of the adjacent segment (good for pick-ups and tails)
        keepOriginalSpeed  ///< play the material 1:1 (no stretching outside the tapped range)
    };

    WarpMap() = default;
    explicit WarpMap (std::vector<WarpAnchor> anchors, Ends ends = Ends::continueTempo);

    double targetToSource (double target) const noexcept;
    double sourceToTarget (double source) const noexcept;

    /** target duration / source duration at a target position (> 1 means slowing down). */
    double stretchFactorAtTarget (double target) const noexcept;

    const std::vector<WarpAnchor>& anchors() const noexcept { return points; }
    bool isIdentity() const noexcept { return points.size() < 2; }
    Ends ends() const noexcept { return endMode; }

private:
    std::vector<WarpAnchor> points;
    Ends endMode = Ends::continueTempo;
    double startSlope = 1.0; ///< d target / d source before the first anchor
    double endSlope = 1.0;   ///< d target / d source after the last anchor
};

} // namespace amt
