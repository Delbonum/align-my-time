#include "amt/Markers.h"
#include "amt/OnsetDetector.h"

#include <algorithm>
#include <cmath>

namespace amt
{

namespace
{
    double median (std::vector<double> values)
    {
        if (values.empty())
            return 0.0;
        const auto mid = values.begin() + (std::ptrdiff_t) (values.size() / 2);
        std::nth_element (values.begin(), mid, values.end());
        return *mid;
    }

    double localMedian (const std::vector<double>& intervals, size_t index, int neighbourhood)
    {
        const auto from = (size_t) std::max<long> (0, (long) index - neighbourhood);
        const auto to = std::min (intervals.size(), index + (size_t) neighbourhood + 1);
        std::vector<double> window;
        for (auto i = from; i < to; ++i)
            if (i != index)
                window.push_back (intervals[i]);
        return window.empty() ? intervals[index] : median (window);
    }
}

std::vector<Marker> cleanUpTaps (const std::vector<double>& tapSeconds, const TapCleanupSettings& settings, std::vector<MarkerIssue>* issues)
{
    std::vector<double> taps (tapSeconds);
    std::sort (taps.begin(), taps.end());

    if (issues != nullptr)
        issues->clear();

    std::vector<Marker> result;
    if (taps.size() < 3)
    {
        for (auto t : taps)
            result.push_back ({ t, t, MarkerOrigin::tapped, false });
        return result;
    }

    // Pass 1: drop double taps. Use the global median as reference so a burst of
    // double taps cannot drag the local reference down.
    std::vector<double> intervals;
    for (size_t i = 1; i < taps.size(); ++i)
        intervals.push_back (taps[i] - taps[i - 1]);
    const double globalMedian = median (intervals);

    std::vector<double> kept { taps.front() };
    std::vector<size_t> droppedAfter; // index in `kept` after which a tap was dropped
    for (size_t i = 1; i < taps.size(); ++i)
    {
        if (taps[i] - kept.back() < settings.doubleTapRatio * globalMedian)
        {
            droppedAfter.push_back (kept.size() - 1);
            continue;
        }
        kept.push_back (taps[i]);
    }

    // Pass 2: fill gaps from missed taps, judged against the local tempo.
    intervals.clear();
    for (size_t i = 1; i < kept.size(); ++i)
        intervals.push_back (kept[i] - kept[i - 1]);

    std::vector<int> keptToResult (kept.size(), 0);
    for (size_t i = 0; i < kept.size(); ++i)
    {
        keptToResult[i] = (int) result.size();
        result.push_back ({ kept[i], kept[i], MarkerOrigin::tapped, false });

        if (i + 1 == kept.size())
            break;

        const double interval = intervals[i];
        const double reference = localMedian (intervals, i, settings.neighbourhood);
        const double ratio = interval / reference;

        if (ratio > settings.missedTapRatio)
        {
            const int steps = std::max (2, (int) std::lround (ratio));
            for (int s = 1; s < steps; ++s)
            {
                const double t = kept[i] + interval * s / steps;
                if (issues != nullptr && s == 1)
                    issues->push_back ({ MarkerIssue::Kind::missedTapFilled, (int) result.size() });
                result.push_back ({ t, t, MarkerOrigin::inserted, false });
            }
        }
        else if (issues != nullptr && std::abs (ratio - 1.0) > settings.irregularRatio)
        {
            issues->push_back ({ MarkerIssue::Kind::irregularInterval, (int) result.size() });
        }
    }

    if (issues != nullptr)
    {
        for (auto k : droppedAfter)
            issues->push_back ({ MarkerIssue::Kind::doubleTapRemoved, keptToResult[k] });

        std::sort (issues->begin(), issues->end(), [] (const MarkerIssue& a, const MarkerIssue& b) { return a.markerIndex < b.markerIndex; });
    }

    return result;
}

int snapMarkersToAttacks (std::vector<Marker>& markers, const OnsetDetector& detector, double windowSeconds)
{
    int snapped = 0;
    for (size_t i = 0; i < markers.size(); ++i)
    {
        auto& m = markers[i];
        if (m.origin == MarkerOrigin::manual)
            continue;

        // Never let a marker jump past its neighbours.
        double window = windowSeconds;
        if (i > 0)
            window = std::min (window, 0.45 * (m.tappedSeconds - markers[i - 1].tappedSeconds));
        if (i + 1 < markers.size())
            window = std::min (window, 0.45 * (markers[i + 1].tappedSeconds - m.tappedSeconds));

        if (auto attack = detector.findAttackNear (m.tappedSeconds, std::max (0.005, window)))
        {
            m.seconds = *attack;
            m.snappedToAttack = true;
            ++snapped;
        }
        else
        {
            m.seconds = m.tappedSeconds;
            m.snappedToAttack = false;
        }
    }
    return snapped;
}

} // namespace amt
