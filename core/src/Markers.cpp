#include "amt/Markers.h"
#include "amt/OnsetDetector.h"

#include <algorithm>
#include <cmath>
#include <optional>

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

std::vector<double> straightenMarkers (const std::vector<Marker>& markers, const std::vector<double>& grid, double amount, int neighbourhood,
                                       const OnsetDetector* detector, std::vector<bool>* onAttack)
{
    if (onAttack != nullptr)
        onAttack->assign (markers.size(), false);

    const auto n = markers.size();
    std::vector<double> seconds;
    for (const auto& m : markers)
        seconds.push_back (m.seconds);

    amount = std::clamp (amount, 0.0, 1.0);
    if (n < 4 || grid.size() != n || amount <= 0.0 || neighbourhood < 2)
        return seconds;

    std::vector<double> robustness (n, 1.0);

    // Weighted least-squares fit of time over grid position through the neighbours of marker i,
    // evaluated at marker i. Near neighbours count more (tricube), outliers less (robustness).
    // A parabola where there are neighbours on both sides, so a ritardando is followed without
    // lagging behind; a line at the ends, where a parabola would extrapolate wildly.
    auto predict = [&] (size_t i) -> std::optional<double> {
        const auto from = (size_t) std::max<long> (0, (long) i - neighbourhood);
        const auto to = std::min (n, i + (size_t) neighbourhood + 1);
        const int size = i >= 2 && i + 2 < n ? 3 : 2;

        double m[3][4] {}; // normal equations, right-hand side in the last column
        int used = 0;
        for (auto j = from; j < to; ++j)
        {
            const double d = std::abs ((double) j - (double) i) / (neighbourhood + 1);
            const double w = j == i ? 0.0 : std::pow (1.0 - d * d * d, 3.0) * robustness[j];
            if (w <= 0.0)
                continue;
            ++used;
            const double x = grid[j] - grid[i];
            const double powers[3] { 1.0, x, x * x };
            for (int r = 0; r < size; ++r)
            {
                for (int c = 0; c < size; ++c)
                    m[r][c] += w * powers[r] * powers[c];
                m[r][3] += w * powers[r] * seconds[j];
            }
        }
        if (used < size)
            return std::nullopt;

        // Gauss-Jordan with partial pivoting; centred on marker i, the constant term is the prediction.
        for (int col = 0; col < size; ++col)
        {
            int pivot = col;
            for (int r = col + 1; r < size; ++r)
                if (std::abs (m[r][col]) > std::abs (m[pivot][col]))
                    pivot = r;
            if (std::abs (m[pivot][col]) < 1.0e-12)
                return std::nullopt;
            std::swap (m[pivot], m[col]);

            for (int r = 0; r < size; ++r)
            {
                if (r == col)
                    continue;
                const double f = m[r][col] / m[col][col];
                for (int c = col; c < size; ++c)
                    m[r][c] -= f * m[col][c];
                m[r][3] -= f * m[col][3];
            }
        }
        return m[0][3] / m[0][0];
    };

    // Pass 1: how far each marker is off the curve its neighbours describe.
    std::vector<double> residuals (n, 0.0);
    for (size_t i = 0; i < n; ++i)
        if (auto p = predict (i))
            residuals[i] = seconds[i] - *p;

    std::vector<double> magnitudes;
    for (auto r : residuals)
        magnitudes.push_back (std::abs (r));
    const double scale = std::max (0.002, 6.0 * median (magnitudes));

    // Bisquare: clear outliers stop guiding their neighbours. Hand-placed markers are trusted;
    // with the audio at hand, markers resting on an attack are more reliable than those without.
    for (size_t i = 0; i < n; ++i)
    {
        const double u = residuals[i] / scale;
        const double confidence = detector != nullptr && ! markers[i].snappedToAttack ? 0.5 : 1.0;
        robustness[i] = markers[i].origin == MarkerOrigin::manual ? 1.0 : confidence * (std::abs (u) < 1.0 ? std::pow (1.0 - u * u, 2.0) : 0.0);
    }

    // Pass 2: move towards the robust curve, or onto the attack found there.
    std::vector<double> result (seconds);
    for (size_t i = 0; i < n; ++i)
    {
        if (markers[i].origin == MarkerOrigin::manual)
            continue;
        auto p = predict (i);
        if (! p)
            continue;

        double target = *p;
        const bool outlier = std::abs (residuals[i]) >= scale;
        if (detector != nullptr && markers[i].snappedToAttack && ! outlier)
        {
            // Already on a plausible attack: that is the actual beat, better evidence than any curve.
            target = seconds[i];
            if (onAttack != nullptr)
                (*onAttack)[i] = true;
        }
        else if (detector != nullptr)
        {
            // Tight window, so a neighbouring 16th or a ghost note is not mistaken for the beat.
            double spacing = 1.0e9;
            if (i > 0)
                spacing = std::min (spacing, seconds[i] - seconds[i - 1]);
            if (i + 1 < n)
                spacing = std::min (spacing, seconds[i + 1] - seconds[i]);
            if (auto attack = detector->findAttackNear (*p, std::clamp (spacing / 8.0, 0.002, 0.025)))
            {
                target = *attack;
                if (onAttack != nullptr)
                    (*onAttack)[i] = true;
            }
        }
        result[i] = seconds[i] + amount * (target - seconds[i]);
    }

    // Never swap two markers.
    for (size_t i = 1; i < n; ++i)
        if (result[i] <= result[i - 1])
            result[i] = result[i - 1] + std::max (0.001, 0.5 * (seconds[i] - seconds[i - 1]));

    return result;
}

} // namespace amt
