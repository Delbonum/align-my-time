#pragma once

#include "SharedClip.h"

#include <amt/TempoMap.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>

namespace amt::plugin
{

/** Remembers which song position is audible right now, so a key press on the message thread
    can be turned into a song time with sub-block accuracy. Written by the audio thread only. */
class PlayPositionTracker
{
public:
    void update (bool isRunning, double songSecondsAtBlockStart) noexcept;

    bool isRunning() const noexcept { return running.load(); }

    /** Song time audible at this very moment (interpolated with the wall clock). */
    double nowSeconds() const noexcept;

    /** Song time of a sample inside the current audio block (for MIDI taps). */
    double secondsAtSample (int sampleOffset, double sampleRate) const noexcept;

private:
    std::atomic<bool> running { false };
    std::atomic<uint32_t> sequence { 0 };
    double blockSeconds = 0.0;
    double blockWallMs = 0.0;
};

/** The plug-in's own transport: plays the merged track (or the aligned result) while the
    host stays stopped, optionally with a metronome on the project grid. Lock-free on the audio side. */
class PreviewPlayer
{
public:
    void prepare (double sampleRate);

    /** Starts at `fromSeconds`; with a lead-in, playback starts that many seconds earlier. */
    void play (double fromSeconds, double leadInSeconds = 0.0);
    void stop();
    bool isPlaying() const noexcept { return playing.load(); }

    /** Current position (song seconds) as last rendered. */
    double positionSeconds() const noexcept { return (double) position.load() / rate; }

    /** Seconds still left in the lead-in (0 once audio is running). */
    double leadInRemaining() const noexcept;

    void setClip (std::shared_ptr<const AudioClip> clip) { clipSlot.set (std::move (clip)); }
    void setClick (bool enabled, std::shared_ptr<const TempoMap> tempo);

    /** Balance between track and click: 0 = only the track, 0.5 = both at full level, 1 = only the click. */
    void setMix (float blend) noexcept
    {
        blend = juce::jlimit (0.0f, 1.0f, blend);
        trackGain.store (juce::jmin (1.0f, 2.0f * (1.0f - blend)));
        clickLevel.store (juce::jmin (1.0f, 2.0f * blend));
    }

    /** Replaces `buffer` with preview audio if playing; returns false if idle. */
    bool render (juce::AudioBuffer<float>& buffer) noexcept;

    /** Where playback stops by itself (song seconds). */
    void setEndSeconds (double end) noexcept { endSeconds.store (end); }

    /** True once after playback reached the end by itself (poll from the message thread). */
    bool consumeFinished() noexcept { return finished.exchange (false); }

private:
    double rate = 48000.0;
    std::atomic<bool> playing { false };
    std::atomic<int64_t> position { 0 };
    std::atomic<int64_t> leadInEnd { 0 };
    std::atomic<double> endSeconds { 1.0e9 };
    std::atomic<bool> clickEnabled { false };
    std::atomic<bool> finished { false };
    std::atomic<float> trackGain { 1.0f }, clickLevel { 1.0f };
    SharedClip clipSlot;
    SharedObject<TempoMap> tempoSlot;
    double clickPhase = 0.0;
    int clickSamplesLeft = 0;
    float clickGain = 0.0f;
};

} // namespace amt::plugin
