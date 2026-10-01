#pragma once

#include "AlignSession.h"
#include "PreviewPlayer.h"
#include "SourceLoader.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace amt::plugin
{

class AlignMyTimeProcessor final : public juce::AudioProcessor,
                                   public juce::AudioProcessorARAExtension,
                                   private juce::Timer
{
public:
    AlignMyTimeProcessor();
    ~AlignMyTimeProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    AlignSession& getSession() { return session; }
    PreviewPlayer& getPreview() { return preview; }

    /** True when the host gave us the track's events via ARA. */
    bool usesARA() const { return isBoundToARA(); }

    /** (Re)loads the track: via ARA, or by finishing the input recording. */
    void reloadTrack();
    bool isLoadingTrack() const { return loader != nullptr; }
    juce::String getLoadError() const { return loadError; }

    /** Non-ARA: records the input while the host plays. */
    InputRecorder& getRecorder() { return recorder; }
    bool isHostPlaying() const { return hostPlaying.load(); }

    /** Why the user cannot go on right now (empty if nothing blocks). Shown next to disabled buttons. */
    juce::String getBlockingReason (bool forRendering) const;

    /** Non-ARA: throws away the recorded track and markers. */
    void discardRecording();

    /** Tap with the space bar / mouse: the song time that is audible right now. */
    void tapNow();
    double getAudiblePositionSeconds() const { return tracker.nowSeconds(); }
    bool isAudioRunning() const { return tracker.isRunning(); }

    /** Plays the original track (A) or the aligned result (B) in the plug-in. */
    void startPreview (double fromSeconds, bool aligned, bool withLeadIn, bool withClick);
    void setPreviewMix (float blend) { preview.setMix (blend); }
    void stopPreview();

    std::function<void()> onTapFromMidi; // message thread

private:
    void didBindToARA() noexcept override;
    void timerCallback() override;
    void loadFromRecorder();

    AlignSession session;
    PreviewPlayer preview;
    PlayPositionTracker tracker;
    InputRecorder recorder;
    std::unique_ptr<TrackLoader> loader;
    juce::String loadError;

    std::atomic<bool> hostPlaying { false };
    bool wasHostPlaying = false;
    bool wasHostPlayingForTaps = false;
    int araRetryTicks = 0;

    // A host playback in step 1 is a tapping pass from where it started; the pass begins
    // with its first tap, so merely listening never discards markers.
    std::optional<double> hostPassStart;
    void tapAt (double songSeconds);
    double currentSampleRate = 48000.0;

    // MIDI taps: song times collected on the audio thread.
    juce::AbstractFifo midiTapFifo { 64 };
    std::array<double, 64> midiTaps {};

    // Non-ARA host tempo, handed to the message thread.
    juce::SpinLock playHeadLock;
    juce::Optional<juce::AudioPlayHead::PositionInfo> lastPlayHead;

    // Non-ARA: where the recorded track is kept between sessions.
    juce::File captureFile;
    const AudioClip* capturedClip = nullptr;

    JUCE_DECLARE_WEAK_REFERENCEABLE (AlignMyTimeProcessor)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlignMyTimeProcessor)
};

} // namespace amt::plugin
