#include "PreviewPlayer.h"

#include <cmath>

namespace amt::plugin
{

//==============================================================================
void PlayPositionTracker::update (bool isRunning, double songSecondsAtBlockStart) noexcept
{
    // Seqlock: odd sequence = write in progress.
    sequence.fetch_add (1, std::memory_order_acq_rel);
    blockSeconds = songSecondsAtBlockStart;
    blockWallMs = juce::Time::getMillisecondCounterHiRes();
    sequence.fetch_add (1, std::memory_order_acq_rel);
    running.store (isRunning);
}

double PlayPositionTracker::nowSeconds() const noexcept
{
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        const auto before = sequence.load (std::memory_order_acquire);
        if ((before & 1u) != 0)
            continue;

        const double seconds = blockSeconds;
        const double wallMs = blockWallMs;

        if (sequence.load (std::memory_order_acquire) == before)
        {
            // Never extrapolate further than a typical block or two (host may have stalled).
            const double elapsed = juce::jlimit (0.0, 0.1, (juce::Time::getMillisecondCounterHiRes() - wallMs) / 1000.0);
            return seconds + (running.load() ? elapsed : 0.0);
        }
    }
    return blockSeconds;
}

double PlayPositionTracker::secondsAtSample (int sampleOffset, double sampleRate) const noexcept
{
    return blockSeconds + sampleOffset / sampleRate;
}

//==============================================================================
void PreviewPlayer::prepare (double sampleRate)
{
    rate = sampleRate > 0.0 ? sampleRate : 48000.0;
}

void PreviewPlayer::play (double fromSeconds, double leadInSeconds)
{
    const auto start = (int64_t) std::llround (fromSeconds * rate);
    leadInEnd.store (start);
    position.store (start - (int64_t) std::llround (leadInSeconds * rate));
    clickSamplesLeft = 0;
    playing.store (true);
}

void PreviewPlayer::stop()
{
    playing.store (false);
}

double PreviewPlayer::leadInRemaining() const noexcept
{
    return juce::jmax (0.0, (double) (leadInEnd.load() - position.load()) / rate);
}

void PreviewPlayer::setClick (bool enabled, std::shared_ptr<const TempoMap> tempo)
{
    tempoSlot.set (std::move (tempo));
    clickEnabled.store (enabled);
}

bool PreviewPlayer::render (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! playing.load())
        return false;

    const auto clip = clipSlot.get();
    const auto tempo = clickEnabled.load() ? tempoSlot.get() : nullptr;
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    auto pos = position.load();

    buffer.clear();

    for (int i = 0; i < numSamples; ++i, ++pos)
    {
        float click = 0.0f;
        if (tempo != nullptr)
        {
            // Start a click whenever we cross a beat of the project grid.
            const double q0 = tempo->secondsToQuarters ((double) pos / rate);
            const double q1 = tempo->secondsToQuarters ((double) (pos + 1) / rate);
            const auto& sig = tempo->signatureAt (q1);
            const double beat = sig.quartersPerBeat();
            const auto beatIndex = (int64_t) std::floor ((q1 - sig.quarters) / beat);
            if (beatIndex != (int64_t) std::floor ((q0 - sig.quarters) / beat))
            {
                const bool downbeat = beatIndex % sig.numerator == 0;
                clickSamplesLeft = (int) (0.03 * rate);
                clickGain = downbeat ? 0.5f : 0.3f;
                clickPhase = downbeat ? 1760.0 : 1320.0;
            }

            if (clickSamplesLeft > 0)
            {
                const double t = (0.03 * rate - clickSamplesLeft) / rate;
                click = clickGain * (float) (std::exp (-t * 120.0) * std::sin (2.0 * juce::MathConstants<double>::pi * clickPhase * t));
                --clickSamplesLeft;
            }
        }

        for (int c = 0; c < numChannels; ++c)
        {
            const float v = clip != nullptr ? clip->sampleAt (juce::jmin (c, clip->numChannels() - 1), pos) : 0.0f;
            buffer.setSample (c, i, v + click);
        }
    }

    position.store (pos);

    if ((double) pos / rate >= endSeconds.load())
    {
        playing.store (false);
        finished.store (true);
    }

    return true;
}

} // namespace amt::plugin
