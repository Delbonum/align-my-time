#include "amt/OnsetDetector.h"

#include <algorithm>
#include <cmath>

namespace amt
{

OnsetDetector::OnsetDetector (const AudioClip& clip, int hopSize)
    : monoSignal (clip.mono()), sampleRate (clip.sampleRate), startSample (clip.startSample), hop (std::max (16, hopSize))
{
    const auto numFrames = monoSignal.size() / (size_t) hop;
    std::vector<double> energy (numFrames, 0.0);

    // Pre-emphasis makes attacks stand out against sustained low end (bass, kick tails).
    double peakEnergy = 0.0;
    float previous = 0.0f;
    for (size_t f = 0; f < numFrames; ++f)
    {
        double e = 0.0;
        for (size_t i = f * (size_t) hop; i < (f + 1) * (size_t) hop; ++i)
        {
            const float x = monoSignal[i];
            const float emphasised = x - 0.9f * previous;
            previous = x;
            e += 0.5 * (double) x * x + (double) emphasised * emphasised;
        }
        energy[f] = e;
        peakEnergy = std::max (peakEnergy, e);
    }

    // Floor at -60 dB below the loudest frame so that noise in quiet passages never counts.
    const double floorEnergy = std::max (1.0e-12, peakEnergy * 1.0e-6);
    novelty.assign (numFrames, 0.0f);
    for (size_t f = 2; f < numFrames; ++f)
    {
        const double before = std::max (energy[f - 1], energy[f - 2]);
        const double rise = std::log (energy[f] + floorEnergy) - std::log (before + floorEnergy);
        novelty[f] = (float) std::max (0.0, rise);
    }
}

double OnsetDetector::refineToSample (int frame) const
{
    // Look for where the waveform first gets loud within the frames around the novelty peak.
    const auto from = (size_t) std::max (0, (frame - 1) * hop);
    const auto to = std::min (monoSignal.size(), (size_t) (frame + 3) * (size_t) hop);

    float peak = 0.0f;
    for (auto i = from; i < to; ++i)
        peak = std::max (peak, std::abs (monoSignal[i]));

    auto onset = (size_t) frame * (size_t) hop;
    for (auto i = from; i < to; ++i)
    {
        if (std::abs (monoSignal[i]) >= 0.3f * peak)
        {
            onset = i;
            break;
        }
    }

    return (double) (startSample + (int64_t) onset) / sampleRate;
}

std::optional<double> OnsetDetector::findAttackNear (double seconds, double windowSeconds) const
{
    const double centreFrame = (seconds * sampleRate - (double) startSample) / hop;
    const double windowFrames = windowSeconds * sampleRate / hop;

    const int from = std::max (2, (int) std::floor (centreFrame - windowFrames));
    const int to = std::min ((int) novelty.size() - 1, (int) std::ceil (centreFrame + windowFrames));

    int bestFrame = -1;
    double bestScore = 0.0;
    for (int f = from; f <= to; ++f)
    {
        if (novelty[(size_t) f] < snapThreshold)
            continue;

        const bool isLocalPeak = novelty[(size_t) f] >= novelty[(size_t) f - 1]
                                 && (f + 1 >= (int) novelty.size() || novelty[(size_t) f] >= novelty[(size_t) f + 1]);
        if (! isLocalPeak)
            continue;

        const double distance = std::abs ((double) f - centreFrame) / std::max (1.0, windowFrames);
        const double score = novelty[(size_t) f] * (1.0 - 0.6 * distance);
        if (score > bestScore)
        {
            bestScore = score;
            bestFrame = f;
        }
    }

    if (bestFrame < 0)
        return std::nullopt;

    return refineToSample (bestFrame);
}

std::vector<double> OnsetDetector::detectAll (double threshold, double minSpacingSeconds) const
{
    std::vector<double> result;
    const int minSpacingFrames = std::max (1, (int) (minSpacingSeconds * sampleRate / hop));
    int lastFrame = -minSpacingFrames;

    for (int f = 2; f + 1 < (int) novelty.size(); ++f)
    {
        const auto n = novelty[(size_t) f];
        if (n >= threshold && n >= novelty[(size_t) f - 1] && n >= novelty[(size_t) f + 1] && f - lastFrame >= minSpacingFrames)
        {
            result.push_back (refineToSample (f));
            lastFrame = f;
        }
    }
    return result;
}

} // namespace amt
