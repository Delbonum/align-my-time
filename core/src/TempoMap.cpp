#include "amt/TempoMap.h"

#include <algorithm>
#include <cmath>

namespace amt
{

TempoMap TempoMap::constant (double bpm, int numerator, int denominator)
{
    bpm = std::max (1.0, bpm);
    return TempoMap ({ { 0.0, 0.0 }, { 60.0, bpm } }, { { 0.0, numerator, denominator } });
}

TempoMap::TempoMap (std::vector<TempoPoint> points, std::vector<TimeSignature> signatures)
    : tempoPoints (std::move (points)), timeSignatures (std::move (signatures))
{
    // Drop points that would break monotonicity rather than producing nonsense later.
    std::vector<TempoPoint> cleaned;
    for (const auto& p : tempoPoints)
        if (cleaned.empty() || (p.seconds > cleaned.back().seconds && p.quarters > cleaned.back().quarters))
            cleaned.push_back (p);

    if (cleaned.empty())
        cleaned = { { 0.0, 0.0 }, { 60.0, 120.0 } };
    else if (cleaned.size() == 1)
        cleaned.push_back ({ cleaned[0].seconds + 60.0, cleaned[0].quarters + 120.0 });

    tempoPoints = std::move (cleaned);

    auto slope = [] (const TempoPoint& a, const TempoPoint& b) { return (b.quarters - a.quarters) / (b.seconds - a.seconds); };
    startQuartersPerSecond = slope (tempoPoints[0], tempoPoints[1]);
    endQuartersPerSecond = slope (tempoPoints[tempoPoints.size() - 2], tempoPoints.back());

    for (auto& sig : timeSignatures)
    {
        sig.numerator = std::max (1, sig.numerator);
        sig.denominator = std::max (1, sig.denominator);
    }

    std::stable_sort (timeSignatures.begin(), timeSignatures.end(),
                      [] (const TimeSignature& a, const TimeSignature& b) { return a.quarters < b.quarters; });

    // The first signature marks the first bar line (as in ARA); without one, bars start at quarter 0.
    if (timeSignatures.empty())
        timeSignatures.push_back ({});
}

double TempoMap::secondsToQuarters (double seconds) const noexcept
{
    const auto& p = tempoPoints;

    if (seconds <= p.front().seconds)
        return p.front().quarters + (seconds - p.front().seconds) * startQuartersPerSecond;

    if (seconds >= p.back().seconds)
        return p.back().quarters + (seconds - p.back().seconds) * endQuartersPerSecond;

    auto it = std::upper_bound (p.begin(), p.end(), seconds, [] (double s, const TempoPoint& tp) { return s < tp.seconds; });
    const auto& b = *it;
    const auto& a = *(it - 1);
    return a.quarters + (seconds - a.seconds) * (b.quarters - a.quarters) / (b.seconds - a.seconds);
}

double TempoMap::quartersToSeconds (double quarters) const noexcept
{
    const auto& p = tempoPoints;

    if (quarters <= p.front().quarters)
        return p.front().seconds + (quarters - p.front().quarters) / startQuartersPerSecond;

    if (quarters >= p.back().quarters)
        return p.back().seconds + (quarters - p.back().quarters) / endQuartersPerSecond;

    auto it = std::upper_bound (p.begin(), p.end(), quarters, [] (double q, const TempoPoint& tp) { return q < tp.quarters; });
    const auto& b = *it;
    const auto& a = *(it - 1);
    return a.seconds + (quarters - a.quarters) * (b.seconds - a.seconds) / (b.quarters - a.quarters);
}

double TempoMap::bpmAt (double seconds) const noexcept
{
    const auto& p = tempoPoints;

    if (seconds < p.front().seconds)
        return startQuartersPerSecond * 60.0;

    if (seconds >= p.back().seconds)
        return endQuartersPerSecond * 60.0;

    auto it = std::upper_bound (p.begin(), p.end(), seconds, [] (double s, const TempoPoint& tp) { return s < tp.seconds; });
    const auto& b = *it;
    const auto& a = *(it - 1);
    return 60.0 * (b.quarters - a.quarters) / (b.seconds - a.seconds);
}

const TimeSignature& TempoMap::signatureAt (double quarters) const noexcept
{
    auto it = std::upper_bound (timeSignatures.begin(), timeSignatures.end(), quarters,
                                [] (double q, const TimeSignature& s) { return q < s.quarters; });
    return it == timeSignatures.begin() ? timeSignatures.front() : *(it - 1);
}

double TempoMap::quartersToBars (double quarters) const noexcept
{
    const auto& first = timeSignatures.front();
    if (quarters <= first.quarters)
        return (quarters - first.quarters) / first.quartersPerBar();

    double bars = 0.0;
    for (size_t i = 0; i < timeSignatures.size(); ++i)
    {
        const auto& sig = timeSignatures[i];
        const double sectionEnd = i + 1 < timeSignatures.size() ? timeSignatures[i + 1].quarters : quarters;

        if (quarters <= sectionEnd || i + 1 == timeSignatures.size())
            return bars + (quarters - sig.quarters) / sig.quartersPerBar();

        bars += (sectionEnd - sig.quarters) / sig.quartersPerBar();
    }
    return bars;
}

double TempoMap::barsToQuarters (double bars) const noexcept
{
    if (bars <= 0.0)
        return timeSignatures.front().quarters + bars * timeSignatures.front().quartersPerBar();

    double barsSoFar = 0.0;
    for (size_t i = 0; i < timeSignatures.size(); ++i)
    {
        const auto& sig = timeSignatures[i];

        if (i + 1 == timeSignatures.size())
            return sig.quarters + (bars - barsSoFar) * sig.quartersPerBar();

        const double sectionBars = (timeSignatures[i + 1].quarters - sig.quarters) / sig.quartersPerBar();
        if (bars <= barsSoFar + sectionBars)
            return sig.quarters + (bars - barsSoFar) * sig.quartersPerBar();

        barsSoFar += sectionBars;
    }
    return 0.0;
}

} // namespace amt
