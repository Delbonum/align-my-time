#include "amt/AudioClip.h"

#include <algorithm>

namespace amt
{

AudioClip::AudioClip (int numChannels, int64_t numSamples, double rate, int64_t start)
    : sampleRate (rate), startSample (start),
      channels ((size_t) std::max (0, numChannels), std::vector<float> ((size_t) std::max<int64_t> (0, numSamples), 0.0f))
{
}

void AudioClip::mixIn (const AudioClip& other, float gain)
{
    const auto from = std::max (startSample, other.startSample);
    const auto to = std::min (endSample(), other.endSample());
    if (to <= from || other.numChannels() == 0)
        return;

    for (int c = 0; c < numChannels(); ++c)
    {
        const auto& src = other.channels[(size_t) std::min (c, other.numChannels() - 1)];
        auto& dst = channels[(size_t) c];
        for (auto s = from; s < to; ++s)
            dst[(size_t) (s - startSample)] += gain * src[(size_t) (s - other.startSample)];
    }
}

std::vector<float> AudioClip::mono() const
{
    std::vector<float> out ((size_t) numSamples(), 0.0f);
    if (channels.empty())
        return out;

    const float scale = 1.0f / (float) channels.size();
    for (const auto& ch : channels)
        for (size_t i = 0; i < out.size(); ++i)
            out[i] += ch[i] * scale;
    return out;
}

} // namespace amt
