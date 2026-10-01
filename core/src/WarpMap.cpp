#include "amt/WarpMap.h"

#include <algorithm>

namespace amt
{

WarpMap::WarpMap (std::vector<WarpAnchor> anchors, Ends ends) : endMode (ends)
{
    std::sort (anchors.begin(), anchors.end(), [] (const WarpAnchor& a, const WarpAnchor& b) { return a.source < b.source; });

    for (const auto& a : anchors)
        if (points.empty() || (a.source > points.back().source && a.target > points.back().target))
            points.push_back (a);

    if (points.size() >= 2 && ends == Ends::continueTempo)
    {
        auto slope = [] (const WarpAnchor& a, const WarpAnchor& b) { return (b.target - a.target) / (b.source - a.source); };
        startSlope = slope (points[0], points[1]);
        endSlope = slope (points[points.size() - 2], points.back());
    }
}

double WarpMap::sourceToTarget (double source) const noexcept
{
    if (points.empty())
        return source;
    if (points.size() == 1)
        return source + (points[0].target - points[0].source);

    if (source <= points.front().source)
        return points.front().target + (source - points.front().source) * startSlope;
    if (source >= points.back().source)
        return points.back().target + (source - points.back().source) * endSlope;

    auto it = std::upper_bound (points.begin(), points.end(), source, [] (double s, const WarpAnchor& a) { return s < a.source; });
    const auto& b = *it;
    const auto& a = *(it - 1);
    return a.target + (source - a.source) * (b.target - a.target) / (b.source - a.source);
}

double WarpMap::targetToSource (double target) const noexcept
{
    if (points.empty())
        return target;
    if (points.size() == 1)
        return target - (points[0].target - points[0].source);

    if (target <= points.front().target)
        return points.front().source + (target - points.front().target) / startSlope;
    if (target >= points.back().target)
        return points.back().source + (target - points.back().target) / endSlope;

    auto it = std::upper_bound (points.begin(), points.end(), target, [] (double t, const WarpAnchor& a) { return t < a.target; });
    const auto& b = *it;
    const auto& a = *(it - 1);
    return a.source + (target - a.target) * (b.source - a.source) / (b.target - a.target);
}

double WarpMap::stretchFactorAtTarget (double target) const noexcept
{
    if (points.size() < 2)
        return 1.0;
    if (target <= points.front().target)
        return startSlope;
    if (target >= points.back().target)
        return endSlope;

    auto it = std::upper_bound (points.begin(), points.end(), target, [] (double t, const WarpAnchor& a) { return t < a.target; });
    const auto& b = *it;
    const auto& a = *(it - 1);
    return (b.target - a.target) / (b.source - a.source);
}

} // namespace amt
