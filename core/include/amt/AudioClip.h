#pragma once

#include <cstdint>
#include <vector>

namespace amt
{

/** A block of multichannel audio placed on the song timeline.
    `startSample` is the song position (in samples at `sampleRate`) of the first sample,
    so all events of a track can be merged into one clip and addressed in song time. */
struct AudioClip
{
    double sampleRate = 48000.0;
    int64_t startSample = 0;
    std::vector<std::vector<float>> channels;

    AudioClip() = default;
    AudioClip (int numChannels, int64_t numSamples, double rate, int64_t start = 0);

    int numChannels() const noexcept { return (int) channels.size(); }
    int64_t numSamples() const noexcept { return channels.empty() ? 0 : (int64_t) channels[0].size(); }
    bool isEmpty() const noexcept { return numSamples() == 0; }

    int64_t endSample() const noexcept { return startSample + numSamples(); }
    double startSeconds() const noexcept { return (double) startSample / sampleRate; }
    double endSeconds() const noexcept { return (double) endSample() / sampleRate; }

    /** Sample at an absolute song position; silence outside the clip. */
    float sampleAt (int channel, int64_t songSample) const noexcept
    {
        const auto i = songSample - startSample;
        if (i < 0 || i >= numSamples())
            return 0.0f;
        return channels[(size_t) channel][(size_t) i];
    }

    /** Adds `other` into this clip where they overlap (both must share the sample rate). */
    void mixIn (const AudioClip& other, float gain = 1.0f);

    /** Average of all channels. */
    std::vector<float> mono() const;
};

} // namespace amt
