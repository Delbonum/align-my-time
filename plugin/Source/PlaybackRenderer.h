#pragma once

#include "SharedClip.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <map>

namespace amt::plugin
{

/** One background thread shared by all instances for reading ARA audio ahead of playback. */
struct SharedReadThread : juce::TimeSliceThread
{
    SharedReadThread() : juce::TimeSliceThread ("Align My Time ARA reader") { startThread (juce::Thread::Priority::high); }
    ~SharedReadThread() override { stopThread (2000); }
};

/** Plays the track's events the way the host asks for them, or - once the user chose
    "replace in track" - the aligned audio in their place. The aligned audio comes from this
    instance, or from another instance that aligns this track along with its own (multitrack). */
class PlaybackRenderer final : public juce::ARAPlaybackRenderer
{
public:
    using juce::ARAPlaybackRenderer::ARAPlaybackRenderer;

    /** Set by the processor when the plug-in instance binds to ARA. */
    void setReplacementSource (SharedClip* slot) noexcept { replacement = slot; }

    /** The track (region sequence) this renderer plays, or nullptr. */
    juce::ARARegionSequence* getTrack() const;

    void prepareToPlay (double sampleRate, int maximumSamplesPerBlock, int numChannels,
                        juce::AudioProcessor::ProcessingPrecision, AlwaysNonRealtime alwaysNonRealtime) override;
    void releaseResources() override;

    bool processBlock (juce::AudioBuffer<float>& buffer, juce::AudioProcessor::Realtime realtime,
                       const juce::AudioPlayHead::PositionInfo& positionInfo) noexcept override;

    using juce::ARAPlaybackRenderer::processBlock;

private:
    struct Reader
    {
        std::unique_ptr<juce::AudioFormatReader> reader;
        juce::BufferingAudioReader* buffering = nullptr;
    };

    Reader& readerFor (juce::ARAAudioSource* source);

    SharedClip* replacement = nullptr;
    std::atomic<SharedClip*> linkedReplacement { nullptr };
    juce::SharedResourcePointer<SharedReadThread> readThread;
    std::map<juce::ARAAudioSource*, Reader> readers;
    std::unique_ptr<juce::AudioBuffer<float>> tempBuffer;
    double sampleRate = 48000.0;
    int maxBlock = 512;
    int channels = 2;
    bool buffered = true;
};

} // namespace amt::plugin
