#pragma once

#include <amt/AudioClip.h>
#include <amt/TempoMap.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <optional>

namespace amt::plugin
{

/** Brings `buffer` from `fromRate` to `toRate` (no-op if they match). */
void resampleInPlace (juce::AudioBuffer<float>& buffer, double fromRate, double toRate);

/** Copies `buffer` into a clip at `startSample`, with exactly `numChannels` channels
    (missing ones repeat the last channel, extra ones are dropped). */
std::shared_ptr<AudioClip> makeClip (const juce::AudioBuffer<float>& buffer, int numChannels, double sampleRate, int64_t startSample);

/** Reads the tempo map and time signatures the host provides for a musical context. */
std::optional<TempoMap> tempoFromMusicalContext (juce::ARAMusicalContext* context);

/** Builds a TempoMap from the host play head (non-ARA hosts): constant tempo, anchored so that
    the current ppq position matches the current time. */
std::optional<TempoMap> tempoFromPlayHead (const juce::AudioPlayHead::PositionInfo& info);

/** Merges all events of a track into one clip on a background thread ("one big event").
    Construct and destroy on the message thread; the ARA readers are created there. */
class TrackLoader : private juce::Thread
{
public:
    struct Result
    {
        std::shared_ptr<const AudioClip> clip;
        juce::String description;
        std::optional<TempoMap> tempo;
        juce::String error;
    };

    TrackLoader (const std::vector<juce::ARAPlaybackRegion*>& regions, double targetSampleRate, int numChannels,
                 std::function<void (Result)> onDone);
    ~TrackLoader() override;

private:
    struct Region
    {
        std::unique_ptr<juce::ARAAudioSourceReader> reader;
        double playbackStart = 0.0, playbackEnd = 0.0, modificationStart = 0.0;
    };

    void run() override;

    std::vector<Region> regions;
    Result result;
    double sampleRate;
    int channels;
    std::function<void (Result)> onDone;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
};

/** Records the plug-in's input while the host plays (fallback for hosts without ARA). The audio
    thread pushes blocks into a FIFO; the message thread drains them into a growing clip. */
class InputRecorder
{
public:
    void prepare (double sampleRate, int numChannels);

    void setArmed (bool shouldRecord) noexcept { armed.store (shouldRecord); }

    /** Audio thread. */
    void push (const juce::AudioBuffer<float>& input, juce::int64 songSample) noexcept;

    /** Message thread: moves pending audio into the clip. Returns true if anything arrived. */
    bool drain();

    void reset();
    const AudioClip& getClip() const { return clip; }
    bool hasAudio() const { return ! clip.isEmpty(); }

private:
    static constexpr int blockFrames = 1024;
    static constexpr int numBlocks = 512; // ~10 s at 48 kHz in flight

    struct Block
    {
        juce::int64 songSample = 0;
        int frames = 0;
        float data[2][blockFrames];
    };

    std::atomic<bool> armed { false };
    juce::AbstractFifo fifo { numBlocks };
    std::vector<Block> blocks = std::vector<Block> (numBlocks);
    AudioClip clip;
    double rate = 48000.0;
    int channels = 2;
};

} // namespace amt::plugin
