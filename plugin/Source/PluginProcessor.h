#pragma once

#include "AlignSession.h"
#include "DocumentController.h"
#include "PreviewPlayer.h"
#include "SourceLoader.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace juce
{
class AudioDeviceManager;
}

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
    const AlignSession& getSession() const { return session; }
    PreviewPlayer& getPreview() { return preview; }

    /** True when the host gave us the track's events via ARA. */
    bool usesARA() const { return isBoundToARA(); }

    /** (Re)loads the track from the host: via ARA, or (without ARA) by recording the next playback.
        Switches back from a loaded audio file to the host's track. */
    void reloadTrack();

    /** Alternative source: an audio file (always in the standalone app, optional in the plug-in).
        It is placed at song position 0. */
    void loadAudioFile (const juce::File& file);
    static juce::String audioFileWildcard();
    bool isUsingAudioFile() const { return sourceFile != juce::File(); }
    const juce::File& getAudioFile() const { return sourceFile; }

    bool isStandalone() const { return wrapperType == wrapperType_Standalone; }

    /** Standalone app: the audio device manager, shown in the settings ("Audio & MIDI"). */
    void setDeviceManager (juce::AudioDeviceManager* manager) { deviceManager = manager; }
    juce::AudioDeviceManager* getDeviceManager() const { return deviceManager; }

    /** Starts over: no audio, no markers, default settings (standalone "Neues Projekt"). */
    void newProject();

    /** Restores a state written by getStateInformation() (or a project file's session).
        "audioFile" names the audio file to load; extra tracks are loaded from their ids. */
    void restoreState (const juce::ValueTree& tree);

    //==============================================================================
    // Extra tracks: several tracks recorded together (e.g. all drum microphones) are aligned with
    // the same markers, so they stay in phase.
    /** Adds audio files as extra tracks (placed at song position 0, like a loaded file). Without a
        track yet, the first file becomes this plug-in's track. */
    void addExtraFiles (const juce::Array<juce::File>& files);
    void removeExtraTrack (ExtraTrack::Kind kind, const juce::String& id);

    /** ARA: another track of the project that Align My Time is active on. */
    struct HostTrack
    {
        juce::String id, name;
        bool linked = false;         ///< aligned along with this track
        juce::String linkedElsewhere; ///< the track whose instance already aligns it, if another one
    };
    std::vector<HostTrack> getHostTracks() const;
    void setHostTrackLinked (const juce::String& id, bool shouldBeLinked);

    /** ARA: the track whose instance aligns this track along with its own (empty if none). */
    juce::String getLinkedByName() const;

    /** A short name for this plug-in's own track (for file names): host track, file or recording. */
    juce::String getOwnTrackName() const;

    /** Taps from the key poller (any thread): song time at the moment of the key press. */
    void pushKeyTap (double songSeconds) noexcept;
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

    juce::ARARegionSequence* getOwnHostTrack() const;
    std::vector<juce::ARARegionSequence*> getOtherHostTracks() const;
    juce::ARARegionSequence* findHostTrack (const juce::String& id) const;
    static juce::String hostTrackId (juce::ARARegionSequence* track);
    static juce::String hostTrackName (juce::ARARegionSequence* track);
    void loadExtraTrack (const ExtraTrack& track);
    void loadMissingExtraTracks();
    /** Tells the linked host tracks what to play (aligned audio or their own events). */
    void publishLinkedTracks();

    AlignSession session;
    PreviewPlayer preview;
    PlayPositionTracker tracker;
    InputRecorder recorder;
    std::unique_ptr<TrackLoader> loader;
    juce::File sourceFile;
    std::unique_ptr<juce::Thread> fileLoader;
    std::map<juce::String, std::unique_ptr<juce::Thread>> extraFileLoaders;
    std::map<juce::String, std::unique_ptr<TrackLoader>> hostTrackLoaders;
    juce::WeakReference<DocumentController> documentController;
    juce::AudioDeviceManager* deviceManager = nullptr;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
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
    juce::AbstractFifo keyTapFifo { 64 };
    std::array<double, 64> keyTaps {};

    // Non-ARA host tempo, handed to the message thread.
    juce::SpinLock playHeadLock;
    juce::Optional<juce::AudioPlayHead::PositionInfo> lastPlayHead;

    // Non-ARA: where the recorded track is kept between sessions.
    juce::File captureFile;
    const AudioClip* capturedClip = nullptr;

    JUCE_DECLARE_WEAK_REFERENCEABLE (AlignMyTimeProcessor)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AlignMyTimeProcessor)
};

/** Replaces "Cubase" in a text with the actual host's name ("DAW" if unknown). */
inline juce::String withHostName (const juce::String& text)
{
    const juce::PluginHostType host;
    if (host.isCubase() || host.isNuendo())
        return text;
    const juce::String name (host.getHostDescription());
    return text.replace ("Cubase", name.isEmpty() || name == "Unknown" ? juce::String ("DAW") : name);
}

} // namespace amt::plugin
