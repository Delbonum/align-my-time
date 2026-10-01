// Unit tests for the DAW-independent core. Deliberately framework-free so they build in seconds.

#include "amt/Alignment.h"
#include "amt/AudioClip.h"
#include "amt/Markers.h"
#include "amt/OnsetDetector.h"
#include "amt/Renderers.h"
#include "amt/TempoMap.h"
#include "amt/WarpMap.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <vector>

namespace
{
int failures = 0;
int checks = 0;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        ++checks;                                                                     \
        if (! (cond)) {                                                               \
            ++failures;                                                               \
            std::printf ("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                             \
    } while (false)

#define CHECK_NEAR(a, b, tol)                                                         \
    do {                                                                              \
        ++checks;                                                                     \
        const double va = (a), vb = (b);                                              \
        if (! (std::abs (va - vb) <= (tol))) {                                        \
            ++failures;                                                               \
            std::printf ("  FAILED %s:%d: %s = %.6f, expected %.6f (+-%g)\n",         \
                         __FILE__, __LINE__, #a, va, vb, (double) (tol));             \
        }                                                                             \
    } while (false)

constexpr double sampleRate = 48000.0;
constexpr double pi = 3.14159265358979323846;

/** A plucked, slightly noisy tone at every onset; downbeats louder. */
amt::AudioClip makeRecording (const std::vector<double>& onsets, const std::vector<bool>& accents, double startSeconds, double lengthSeconds)
{
    amt::AudioClip clip (2, (int64_t) (lengthSeconds * sampleRate), sampleRate, (int64_t) (startSeconds * sampleRate));
    std::mt19937 rng (1);
    std::uniform_real_distribution<float> noise (-1.0f, 1.0f);

    for (size_t k = 0; k < onsets.size(); ++k)
    {
        const auto start = (int64_t) std::llround (onsets[k] * sampleRate) - clip.startSample;
        const float amp = accents[k] ? 0.8f : 0.45f;
        for (int64_t i = 0; i < (int64_t) (0.25 * sampleRate); ++i)
        {
            const auto idx = start + i;
            if (idx < 0 || idx >= clip.numSamples())
                continue;
            const double t = i / sampleRate;
            const float env = amp * (float) std::exp (-t * 18.0);
            const float tone = (float) std::sin (2.0 * pi * 110.0 * t) + 0.3f * (float) std::sin (2.0 * pi * 330.0 * t);
            const float click = i < 240 ? noise (rng) * 0.6f : 0.0f;
            const float v = env * (tone + click);
            clip.channels[0][(size_t) idx] += v;
            clip.channels[1][(size_t) idx] += 0.9f * v;
        }
    }
    return clip;
}

struct DriftingTake
{
    std::vector<double> downbeats;
    std::vector<double> onsets;
    std::vector<bool> accents;
    amt::AudioClip clip;
};

/** 12 bars of 4/4 played around 114 BPM with a slow drift, starting at 1.03 s. */
DriftingTake makeDriftingTake()
{
    DriftingTake take;
    double t = 1.03;
    for (int bar = 0; bar < 12; ++bar)
    {
        const double bpm = 114.0 + 2.0 * std::sin (bar * 0.7);
        const double beat = 60.0 / bpm;
        take.downbeats.push_back (t);
        for (int b = 0; b < 4; ++b)
        {
            take.onsets.push_back (t + b * beat);
            take.accents.push_back (b == 0);
        }
        t += 4 * beat;
    }
    take.downbeats.push_back (t); // final "one" so the last bar is tapped too
    take.onsets.push_back (t);
    take.accents.push_back (true);
    take.clip = makeRecording (take.onsets, take.accents, 0.5, t + 1.0);
    return take;
}

double nearestDistance (const std::vector<double>& values, double x)
{
    double best = 1.0e9;
    for (auto v : values)
        best = std::min (best, std::abs (v - x));
    return best;
}

void testTempoMap()
{
    const auto constant = amt::TempoMap::constant (120.0);
    CHECK_NEAR (constant.secondsToQuarters (1.0), 2.0, 1e-9);
    CHECK_NEAR (constant.quartersToSeconds (8.0), 4.0, 1e-9);
    CHECK_NEAR (constant.barsToQuarters (3.0), 12.0, 1e-9);
    CHECK_NEAR (constant.bpmAt (100.0), 120.0, 1e-9);
    CHECK_NEAR (constant.secondsToQuarters (-1.0), -2.0, 1e-9);

    // 120 BPM for 8 quarters, then 60 BPM; 4/4 for two bars, then 3/4.
    amt::TempoMap changing ({ { 0.0, 0.0 }, { 4.0, 8.0 }, { 8.0, 12.0 } }, { { 0.0, 4, 4 }, { 8.0, 3, 4 } });
    CHECK_NEAR (changing.quartersToSeconds (10.0), 6.0, 1e-9);
    CHECK_NEAR (changing.secondsToQuarters (6.0), 10.0, 1e-9);
    CHECK_NEAR (changing.bpmAt (5.0), 60.0, 1e-9);
    CHECK_NEAR (changing.barsToQuarters (3.0), 11.0, 1e-9);
    CHECK_NEAR (changing.quartersToBars (11.0), 3.0, 1e-9);
    CHECK_NEAR (changing.quartersToBars (6.0), 1.5, 1e-9);
    CHECK (changing.signatureAt (9.0).numerator == 3);

    // Beat grid follows the signature change: bar 1 starts at quarter 4, its beats are quarters.
    CHECK_NEAR (amt::gridQuarters (changing, amt::TapMode::beats, 1, 5), 9.0, 1e-9);

    amt::TempoMap pickup ({ { 0.0, 0.0 }, { 1.0, 2.0 } }, { { 1.0, 4, 4 } }); // first bar line at quarter 1
    CHECK_NEAR (pickup.barsToQuarters (2.0), 9.0, 1e-9);
    CHECK_NEAR (pickup.quartersToBars (0.0), -0.25, 1e-9);

    amt::TempoMap sixEight ({ { 0.0, 0.0 }, { 1.0, 2.0 } }, { { 0.0, 6, 8 } });
    CHECK_NEAR (sixEight.barsToQuarters (1.0), 3.0, 1e-9);
    CHECK_NEAR (amt::gridQuarters (sixEight, amt::TapMode::beats, 0, 6), 3.0, 1e-9);
}

void testWarpMap()
{
    amt::WarpMap warp ({ { 1.0, 2.0 }, { 3.0, 3.0 }, { 5.0, 5.0 } });
    CHECK_NEAR (warp.sourceToTarget (2.0), 2.5, 1e-9);
    CHECK_NEAR (warp.targetToSource (2.5), 2.0, 1e-9);
    CHECK_NEAR (warp.sourceToTarget (0.0), 1.5, 1e-9);  // continues the first segment's factor 0.5
    CHECK_NEAR (warp.sourceToTarget (6.0), 6.0, 1e-9);
    CHECK_NEAR (warp.stretchFactorAtTarget (2.2), 0.5, 1e-9);

    amt::WarpMap oneToOne ({ { 1.0, 2.0 }, { 3.0, 3.0 } }, amt::WarpMap::Ends::keepOriginalSpeed);
    CHECK_NEAR (oneToOne.sourceToTarget (0.0), 1.0, 1e-9);
    CHECK_NEAR (oneToOne.targetToSource (4.0), 4.0, 1e-9);

    for (double s = -2.0; s < 8.0; s += 0.37)
        CHECK_NEAR (warp.targetToSource (warp.sourceToTarget (s)), s, 1e-9);
}

void testTapCleanup()
{
    std::vector<double> taps;
    for (int i = 0; i < 16; ++i)
        taps.push_back (1.0 + 2.0 * i + (i % 3 == 0 ? 0.02 : -0.015));

    taps.erase (taps.begin() + 6);        // missed one tap
    taps.erase (taps.begin() + 9, taps.begin() + 11); // missed two in a row
    taps.push_back (taps[3] + 0.12);      // nervous double tap

    std::vector<amt::MarkerIssue> issues;
    const auto markers = amt::cleanUpTaps (taps, {}, &issues);

    CHECK (markers.size() == 16);
    int inserted = 0;
    for (const auto& m : markers)
        inserted += m.origin == amt::MarkerOrigin::inserted ? 1 : 0;
    CHECK (inserted == 3);
    CHECK_NEAR (markers[6].seconds, 13.0, 0.05);

    int doubles = 0, filled = 0;
    for (const auto& issue : issues)
    {
        doubles += issue.kind == amt::MarkerIssue::Kind::doubleTapRemoved ? 1 : 0;
        filled += issue.kind == amt::MarkerIssue::Kind::missedTapFilled ? 1 : 0;
    }
    CHECK (doubles == 1);
    CHECK (filled == 2);
}

void testOnsetSnapping()
{
    auto take = makeDriftingTake();
    amt::OnsetDetector detector (take.clip);

    const auto detected = detector.detectAll();
    CHECK (detected.size() == take.onsets.size());
    for (auto onset : take.onsets)
        CHECK (nearestDistance (detected, onset) < 0.004);

    // Taps that are up to 45 ms early or late snap back onto the true attack.
    std::mt19937 rng (5);
    std::uniform_real_distribution<double> jitter (-0.045, 0.045);
    std::vector<double> taps;
    for (auto d : take.downbeats)
        taps.push_back (d + jitter (rng));

    auto markers = amt::cleanUpTaps (taps);
    const int snapped = amt::snapMarkersToAttacks (markers, detector);
    CHECK (snapped == (int) markers.size());
    for (size_t i = 0; i < markers.size(); ++i)
        CHECK_NEAR (markers[i].seconds, take.downbeats[i], 0.004);
}

amt::AlignmentPlan planForTake (const DriftingTake& take, amt::TapMode mode)
{
    std::vector<amt::Marker> markers;
    const auto& positions = mode == amt::TapMode::downbeats ? take.downbeats : take.onsets;
    for (auto p : positions)
        markers.push_back ({ p, p, amt::MarkerOrigin::tapped, true });
    return amt::planAlignment (markers, amt::TempoMap::constant (120.0), mode);
}

void testPlan()
{
    const auto take = makeDriftingTake();
    const auto plan = planForTake (take, amt::TapMode::downbeats);

    CHECK (plan.firstBar == 1); // 1.03 s is nearest to the bar line at 2.0 s
    CHECK (plan.targetSeconds.size() == take.downbeats.size());
    CHECK_NEAR (plan.targetSeconds[0], 2.0, 1e-9);
    CHECK_NEAR (plan.targetSeconds[4], 10.0, 1e-9);
    CHECK (plan.averageBpm > 112.0 && plan.averageBpm < 116.0);
    CHECK (plan.minBpm >= 111.9 && plan.maxBpm <= 116.1);

    const auto beatPlan = planForTake (take, amt::TapMode::beats);
    CHECK_NEAR (beatPlan.targetSeconds[5], 2.0 + 5 * 0.5, 1e-9);

    const auto forced = amt::planAlignment ({ { 1.0, 1.0 }, { 3.0, 3.0 } }, amt::TempoMap::constant (120.0), amt::TapMode::downbeats, 4);
    CHECK_NEAR (forced.targetSeconds[0], 8.0, 1e-9);
}

/** After alignment every beat of the take must sit on the 120 BPM grid. */
void checkAlignedToGrid (const amt::AudioClip& rendered, const DriftingTake& take, const amt::AlignmentPlan& plan, double tolerance, const char* what)
{
    amt::OnsetDetector detector (rendered);
    const auto detected = detector.detectAll (1.5);

    double worst = 0.0;
    for (size_t k = 0; k + 1 < take.onsets.size(); ++k)
    {
        const double expected = plan.targetSeconds[0] + 0.5 * (double) k; // one beat = 0.5 s at 120 BPM
        worst = std::max (worst, nearestDistance (detected, expected));
    }
    std::printf ("  %s: worst beat deviation from grid %.2f ms (%zu attacks found)\n", what, worst * 1000.0, detected.size());
    CHECK (worst < tolerance);
}

void testTimeStretchAlignment()
{
    const auto take = makeDriftingTake();
    const auto plan = planForTake (take, amt::TapMode::downbeats);

    // Downbeat markers only: beats in between follow from the bar-wise stretch.
    std::vector<double> attacks;
    for (auto d : take.downbeats)
        attacks.push_back (d);
    const auto rendered = amt::renderTimeStretch (take.clip, plan.warp, amt::StretchQuality::rhythmic,
                                                  amt::defaultRenderRange (take.clip, plan.warp), {}, attacks);
    CHECK (! rendered.isEmpty());
    CHECK_NEAR (rendered.startSeconds(), plan.warp.sourceToTarget (take.clip.startSeconds()), 1.0 / sampleRate);
    checkAlignedToGrid (rendered, take, plan, 0.006, "time-stretch (downbeats)");
}

void testSliceAlignment()
{
    const auto take = makeDriftingTake();
    const auto plan = planForTake (take, amt::TapMode::beats);
    const auto rendered = amt::renderSlices (take.clip, plan.warp, 0.010, amt::defaultRenderRange (take.clip, plan.warp));
    checkAlignedToGrid (rendered, take, plan, 0.004, "cut + crossfade (beats)");
}

void testSlicesAreTransparentWithoutTempoChange()
{
    const auto take = makeDriftingTake();
    std::vector<amt::WarpAnchor> anchors;
    for (auto d : take.downbeats)
        anchors.push_back ({ d, d });
    const amt::WarpMap identity (anchors, amt::WarpMap::Ends::keepOriginalSpeed);

    const auto rendered = amt::renderSlices (take.clip, identity, 0.010, amt::defaultRenderRange (take.clip, identity));
    double maxError = 0.0;
    for (int64_t s = rendered.startSample; s < rendered.endSample(); ++s)
        maxError = std::max (maxError, (double) std::abs (rendered.sampleAt (0, s) - take.clip.sampleAt (0, s)));
    CHECK (maxError < 1.0e-5);
}

double estimateFrequency (const amt::AudioClip& clip, double fromSeconds, double toSeconds)
{
    const auto from = (int64_t) (fromSeconds * clip.sampleRate);
    const auto to = (int64_t) (toSeconds * clip.sampleRate);
    int crossings = 0;
    double first = -1.0, last = -1.0;
    for (auto s = from + 1; s < to; ++s)
    {
        const float a = clip.sampleAt (0, s - 1), b = clip.sampleAt (0, s);
        if (a < 0.0f && b >= 0.0f)
        {
            const double t = (double) (s - 1) + a / (a - b);
            if (first < 0.0)
                first = t;
            last = t;
            ++crossings;
        }
    }
    return crossings > 1 ? (crossings - 1) * clip.sampleRate / (last - first) : 0.0;
}

void testPitchIsPreserved()
{
    amt::AudioClip sine (1, (int64_t) (4.0 * sampleRate), sampleRate, 0);
    for (size_t i = 0; i < sine.channels[0].size(); ++i)
        sine.channels[0][i] = 0.5f * (float) std::sin (2.0 * pi * 440.0 * (double) i / sampleRate);

    const amt::WarpMap slower ({ { 0.0, 0.0 }, { 4.0, 4.6 } }); // 120 -> ~104 BPM worth of stretch
    for (auto quality : { amt::StretchQuality::rhythmic, amt::StretchQuality::melodic, amt::StretchQuality::complex })
    {
        const auto rendered = amt::renderTimeStretch (sine, slower, quality, { 0.0, 4.6 });
        CHECK_NEAR ((double) rendered.numSamples(), 4.6 * sampleRate, 1.0);
        CHECK_NEAR (estimateFrequency (rendered, 1.0, 3.5), 440.0, 0.5);
    }
}

void testCancellation()
{
    const auto take = makeDriftingTake();
    const auto plan = planForTake (take, amt::TapMode::downbeats);
    const auto rendered = amt::renderTimeStretch (take.clip, plan.warp, amt::StretchQuality::melodic,
                                                  amt::defaultRenderRange (take.clip, plan.warp), [] (double) { return false; });
    CHECK (rendered.isEmpty());
}
} // namespace

int main()
{
    const std::vector<std::pair<const char*, std::function<void()>>> tests {
        { "tempo map", testTempoMap },
        { "warp map", testWarpMap },
        { "tap clean-up", testTapCleanup },
        { "onset snapping", testOnsetSnapping },
        { "alignment plan", testPlan },
        { "time-stretch alignment", testTimeStretchAlignment },
        { "slice alignment", testSliceAlignment },
        { "slices are transparent", testSlicesAreTransparentWithoutTempoChange },
        { "pitch is preserved", testPitchIsPreserved },
        { "cancellation", testCancellation },
    };

    for (const auto& [name, run] : tests)
    {
        const int before = failures;
        std::printf ("%s\n", name);
        run();
        std::printf ("  %s\n", failures == before ? "ok" : "FAILED");
    }

    std::printf ("\n%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
