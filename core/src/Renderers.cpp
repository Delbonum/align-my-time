#include "amt/Renderers.h"

#include <signalsmith-stretch/signalsmith-stretch.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace amt
{

namespace
{
    constexpr double pi = 3.14159265358979323846;

    /** Lets the stretcher read `inputs[channel][i]` straight from a clip, with silence outside it. */
    struct ClipReader
    {
        struct Channel
        {
            const AudioClip* clip;
            int channel;
            int64_t offset;
            float operator[] (int i) const noexcept { return clip->sampleAt (channel, offset + i); }
        };

        const AudioClip* clip;
        int64_t offset;

        Channel operator[] (int channel) const noexcept
        {
            return { clip, std::min (channel, clip->numChannels() - 1), offset };
        }
    };

    struct StretchSettings
    {
        double blockSeconds;
        double intervalSeconds;
    };

    StretchSettings settingsFor (StretchQuality quality)
    {
        switch (quality)
        {
            case StretchQuality::rhythmic: return { 0.06, 0.015 };
            case StretchQuality::complex:  return { 0.16, 0.04 };
            case StretchQuality::melodic:
            default:                       return { 0.12, 0.03 };
        }
    }

    /** Raised-cosine gain that rises from 0 to 1 over `length` samples; sums to 1 with its mirror. */
    float fadeIn (double position, double length)
    {
        if (length <= 0.0 || position >= length)
            return 1.0f;
        if (position <= 0.0)
            return 0.0f;
        return (float) (0.5 - 0.5 * std::cos (pi * position / length));
    }
}

namespace
{
    /** Replaces the stretched audio around each attack with the original, placed so the attack
        lands exactly on its warped position. The window starts early enough to cover the
        stretcher's pre-echo (half an analysis block) and joins with short crossfades. */
    void spliceAttacks (AudioClip& out, const AudioClip& source, const WarpMap& warp,
                        std::vector<double> attacks, double preSeconds)
    {
        if (attacks.empty())
            return;

        std::sort (attacks.begin(), attacks.end());

        const double sr = source.sampleRate;
        const auto pre = (int64_t) (preSeconds * sr);
        const auto post = (int64_t) (0.030 * sr);
        const auto fade = (int64_t) (0.006 * sr);
        int64_t previousEnd = std::numeric_limits<int64_t>::min();

        for (size_t i = 0; i < attacks.size(); ++i)
        {
            const auto sourceAttack = (int64_t) std::llround (attacks[i] * sr);
            const auto targetAttack = (int64_t) std::llround (warp.sourceToTarget (attacks[i]) * sr);

            int64_t begin = std::max (targetAttack - pre, previousEnd);
            int64_t end = targetAttack + post;
            if (i + 1 < attacks.size())
            {
                const auto nextTarget = (int64_t) std::llround (warp.sourceToTarget (attacks[i + 1]) * sr);
                end = std::min (end, nextTarget - pre);
            }

            // Attacks too close together: leave this one to the stretcher.
            if (end - begin < 2 * fade || targetAttack - begin < fade)
                continue;

            previousEnd = end;

            const int64_t from = std::max (begin, out.startSample);
            const int64_t to = std::min (end, out.endSample());
            for (int64_t n = from; n < to; ++n)
            {
                const float g = fadeIn ((double) (n - begin), (double) fade)
                                * (1.0f - fadeIn ((double) (n - (end - fade)), (double) fade));
                const int64_t sourceSample = sourceAttack + (n - targetAttack);
                for (int c = 0; c < out.numChannels(); ++c)
                {
                    auto& v = out.channels[(size_t) c][(size_t) (n - out.startSample)];
                    v = (1.0f - g) * v + g * source.sampleAt (c, sourceSample);
                }
            }
        }
    }
}

RenderRange defaultRenderRange (const AudioClip& source, const WarpMap& warp)
{
    return { warp.sourceToTarget (source.startSeconds()), warp.sourceToTarget (source.endSeconds()) };
}

AudioClip renderTimeStretch (const AudioClip& source, const WarpMap& warp, StretchQuality quality,
                             RenderRange range, const ProgressCallback& progress,
                             const std::vector<double>& protectedAttacks)
{
    const double sr = source.sampleRate;
    const int numChannels = std::max (1, source.numChannels());
    const auto targetStart = (int64_t) std::llround (range.startSeconds * sr);
    const auto targetEnd = (int64_t) std::llround (range.endSeconds * sr);
    const auto outLength = std::max<int64_t> (0, targetEnd - targetStart);

    AudioClip out (numChannels, outLength, sr, targetStart);
    if (outLength == 0 || source.isEmpty())
        return out;

    const auto settings = settingsFor (quality);
    signalsmith::stretch::SignalsmithStretch<float> stretch (42); // fixed seed: renders are reproducible
    stretch.configure (numChannels, (int) (settings.blockSeconds * sr), (int) (settings.intervalSeconds * sr));

    const double inputLatency = stretch.inputLatency();
    const double outputLatency = stretch.outputLatency();
    const int64_t preroll = stretch.blockSamples() + stretch.outputLatency();

    // Same convention as SignalsmithStretch::exact(): for output sample k to carry source
    // position warp(k), the stretcher must already have been fed inputLatency samples beyond
    // the source position that will be heard outputLatency samples later. Looking ahead through
    // the warp (instead of multiplying by the current rate) keeps tempo changes at markers exact.
    // Raw output index r corresponds to target sample targetStart - preroll + r; the preroll
    // lets the stretcher settle and is discarded.
    auto suppliedSourceFor = [&] (int64_t rawIndex) {
        const double target = (double) (targetStart - preroll + rawIndex) + outputLatency;
        return (int64_t) std::llround (warp.targetToSource (target / sr) * sr + inputLatency);
    };

    int64_t cursor = (int64_t) std::llround (warp.targetToSource ((double) (targetStart - preroll) / sr) * sr);
    const int64_t discard = preroll;
    const int64_t rawTotal = outLength + discard;

    constexpr int chunk = 256;
    std::vector<std::vector<float>> scratch ((size_t) numChannels, std::vector<float> (chunk));
    std::vector<float*> scratchPointers;
    for (auto& s : scratch)
        scratchPointers.push_back (s.data());

    for (int64_t r0 = 0; r0 < rawTotal; r0 += chunk)
    {
        const int n = (int) std::min<int64_t> (chunk, rawTotal - r0);
        const int64_t needed = suppliedSourceFor (r0 + n);
        const int inputCount = (int) std::max<int64_t> (0, needed - cursor);

        stretch.process (ClipReader { &source, cursor }, inputCount, scratchPointers.data(), n);
        cursor += inputCount;

        for (int i = 0; i < n; ++i)
        {
            const int64_t outIndex = r0 + i - discard;
            if (outIndex < 0)
                continue;
            for (int c = 0; c < numChannels; ++c)
                out.channels[(size_t) c][(size_t) outIndex] = scratch[(size_t) c][(size_t) i];
        }

        if (progress && (r0 / chunk) % 64 == 0 && ! progress ((double) r0 / (double) rawTotal))
            return {};
    }

    spliceAttacks (out, source, warp, protectedAttacks, settings.blockSeconds * 0.5 + 0.005);

    if (progress)
        progress (1.0);
    return out;
}

AudioClip renderSlices (const AudioClip& source, const WarpMap& warp, double crossfadeSeconds,
                        RenderRange range, const ProgressCallback& progress)
{
    const double sr = source.sampleRate;
    const int numChannels = std::max (1, source.numChannels());
    const auto targetStart = (int64_t) std::llround (range.startSeconds * sr);
    const auto targetEnd = (int64_t) std::llround (range.endSeconds * sr);
    const auto outLength = std::max<int64_t> (0, targetEnd - targetStart);

    AudioClip out (numChannels, outLength, sr, targetStart);
    if (outLength == 0 || source.isEmpty())
        return out;

    struct Slice
    {
        int64_t sourceStart, sourceEnd, targetStart, targetEnd;
    };

    auto toSamples = [sr] (double seconds) { return (int64_t) std::llround (seconds * sr); };

    std::vector<Slice> slices;
    const auto& anchors = warp.anchors();

    if (anchors.empty())
    {
        const auto offset = toSamples (warp.sourceToTarget (0.0));
        slices.push_back ({ source.startSample, source.endSample(), source.startSample + offset, source.endSample() + offset });
    }
    else
    {
        const auto first = anchors.front();
        const auto firstSource = toSamples (first.source);
        const auto firstTarget = toSamples (first.target);
        if (source.startSample < firstSource)
            slices.push_back ({ source.startSample, firstSource, firstTarget - (firstSource - source.startSample), firstTarget });

        for (size_t i = 0; i + 1 < anchors.size(); ++i)
            slices.push_back ({ toSamples (anchors[i].source), toSamples (anchors[i + 1].source),
                                toSamples (anchors[i].target), toSamples (anchors[i + 1].target) });

        const auto lastSource = toSamples (anchors.back().source);
        const auto lastTarget = toSamples (anchors.back().target);
        if (source.endSample() > lastSource)
            slices.push_back ({ lastSource, source.endSample(), lastTarget, lastTarget + (source.endSample() - lastSource) });
    }

    const auto crossfade = std::max<int64_t> (0, toSamples (crossfadeSeconds));

    for (size_t s = 0; s < slices.size(); ++s)
    {
        const auto& slice = slices[s];
        const bool isFirst = s == 0;
        const bool isLast = s + 1 == slices.size();

        const int64_t sliceLength = std::max<int64_t> (1, slice.targetEnd - slice.targetStart);
        const int64_t fade = std::min (crossfade, sliceLength / 2);

        // The incoming fade sits *before* the attack, so the attack itself is never softened.
        const int64_t fadeInLength = isFirst ? 0 : fade;
        const int64_t begin = slice.targetStart - fadeInLength;

        // Played at original speed, the material lasts until the next attack in the source.
        const int64_t available = slice.targetStart + (slice.sourceEnd - slice.sourceStart);
        const int64_t end = isLast ? available : std::min (slice.targetEnd, available);
        const int64_t fadeOutLength = isLast ? 0 : std::min (fade, std::max<int64_t> (0, end - slice.targetStart));
        const int64_t fadeOutStart = end - fadeOutLength;

        const int64_t from = std::max (begin, targetStart);
        const int64_t to = std::min (end, targetEnd);

        for (int64_t n = from; n < to; ++n)
        {
            float gain = fadeIn ((double) (n - begin), (double) fadeInLength);
            if (n >= fadeOutStart)
                gain *= 1.0f - fadeIn ((double) (n - fadeOutStart), (double) fadeOutLength);

            const int64_t sourceSample = slice.sourceStart + (n - slice.targetStart);
            for (int c = 0; c < numChannels; ++c)
                out.channels[(size_t) c][(size_t) (n - targetStart)] += gain * source.sampleAt (c, sourceSample);
        }

        if (progress && ! progress ((double) (s + 1) / (double) slices.size()))
            return {};
    }

    return out;
}

} // namespace amt
