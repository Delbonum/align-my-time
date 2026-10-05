#include "PluginProcessor.h"
#include "PlaybackRenderer.h"
#include "PluginEditor.h"

namespace amt::plugin
{

namespace
{
    juce::File captureFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("Align My Time")
            .getChildFile ("Captures");
    }

    bool writeCapture (const AudioClip& clip, const juce::File& file)
    {
        file.getParentDirectory().createDirectory();
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        if (stream == nullptr)
            return false;

        juce::WavAudioFormat wav;
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions {}
                                                       .withSampleRate (clip.sampleRate)
                                                       .withNumChannels (clip.numChannels())
                                                       .withBitsPerSample (32)
                                                       .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        if (writer == nullptr)
            return false;

        std::vector<const float*> channels;
        for (const auto& ch : clip.channels)
            channels.push_back (ch.data());
        return writer->writeFromFloatArrays (channels.data(), (int) channels.size(), (int) clip.numSamples());
    }

    std::shared_ptr<AudioClip> readCapture (const juce::File& file, juce::int64 startSample)
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (file.createInputStream().release(), true));
        if (reader == nullptr)
            return nullptr;

        auto clip = std::make_shared<AudioClip> ((int) reader->numChannels, reader->lengthInSamples, reader->sampleRate, startSample);
        juce::AudioBuffer<float> buffer ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, true);
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            std::copy (buffer.getReadPointer (c), buffer.getReadPointer (c) + buffer.getNumSamples(), clip->channels[(size_t) c].begin());
        return clip;
    }
}

//==============================================================================
AlignMyTimeProcessor::AlignMyTimeProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    startTimerHz (30);
}

AlignMyTimeProcessor::~AlignMyTimeProcessor()
{
    stopTimer();
    *alive = false;
    if (fileLoader != nullptr)
        fileLoader->stopThread (5000);
    loader.reset();
}

void AlignMyTimeProcessor::didBindToARA() noexcept
{
    juce::AudioProcessorARAExtension::didBindToARA();

    if (auto* renderer = getPlaybackRenderer<PlaybackRenderer>())
        renderer->setReplacementSource (&session.getReplacementSlot());

    // Load the track as soon as the host has told us about it.
    juce::MessageManager::callAsync ([safe = juce::WeakReference<AlignMyTimeProcessor> (this)] {
        if (auto* p = safe.get(); p != nullptr && ! p->session.hasSource() && ! p->isUsingAudioFile())
            p->reloadTrack();
    });
}

void AlignMyTimeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // A loaded file must match the playback rate (the standalone app may change devices).
    const bool rateChanged = ! juce::approximatelyEqual (currentSampleRate, sampleRate);
    currentSampleRate = sampleRate;
    if (rateChanged && isUsingAudioFile())
        juce::MessageManager::callAsync ([safe = juce::WeakReference<AlignMyTimeProcessor> (this)] {
            if (auto* p = safe.get())
                p->loadAudioFile (p->sourceFile);
        });

    preview.prepare (sampleRate);
    recorder.prepare (sampleRate, getTotalNumInputChannels());
    recorder.setArmed (session.getStep() == Step::tap && ! isUsingAudioFile()); // ready before the first block arrives
    prepareToPlayForARA (sampleRate, samplesPerBlock, getMainBusNumOutputChannels(), getProcessingPrecision());
}

void AlignMyTimeProcessor::releaseResources()
{
    releaseResourcesForARA();
}

bool AlignMyTimeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == out;
}

double AlignMyTimeProcessor::getTailLengthSeconds() const
{
    double tail = 0.0;
    getTailLengthSecondsForARA (tail);
    return tail;
}

void AlignMyTimeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    auto* hostPlayHead = getPlayHead();
    const auto position = hostPlayHead != nullptr ? hostPlayHead->getPosition() : juce::nullopt;
    const bool playing = position.hasValue() && position->getIsPlaying();
    const auto songSample = position.hasValue() ? position->getTimeInSamples().orFallback (0) : 0;
    hostPlaying.store (playing);

    {
        const juce::SpinLock::ScopedTryLockType lock (playHeadLock);
        if (lock.isLocked())
            lastPlayHead = position;
    }

    const bool previewing = preview.isPlaying();
    const double blockSeconds = previewing ? preview.positionSeconds() : (double) songSample / currentSampleRate;

    // MIDI note or sustain pedal = tap (foot switch!)
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn() || (msg.isController() && msg.getControllerNumber() == 64 && msg.getControllerValue() >= 64))
        {
            const auto scope = midiTapFifo.write (1);
            if (scope.blockSize1 > 0)
                midiTaps[(size_t) scope.startIndex1] = blockSeconds + meta.samplePosition / currentSampleRate;
        }
    }
    midi.clear();

    if (isBoundToARA())
    {
        processBlockForARA (buffer, isRealtime(), hostPlayHead);
    }
    else
    {
        if (playing)
            recorder.push (buffer, songSample);

        // "Replace in track" without ARA: play the aligned audio instead of the live input.
        if (playing)
        {
            if (auto clip = session.getReplacementSlot().get())
            {
                for (int c = 0; c < buffer.getNumChannels(); ++c)
                {
                    auto* out = buffer.getWritePointer (c);
                    const int sourceChannel = juce::jmin (c, clip->numChannels() - 1);
                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                        out[i] = clip->sampleAt (sourceChannel, songSample + i);
                }
            }
        }
    }

    preview.render (buffer);
    tracker.update (previewing || playing, blockSeconds);
}

//==============================================================================
void AlignMyTimeProcessor::timerCallback()
{
    // Before the taps are handled: did a host playback just start?
    {
        const bool playingNow = hostPlaying.load();
        if (playingNow && ! wasHostPlayingForTaps && ! preview.isPlaying())
            hostPassStart = tracker.nowSeconds();
        if (! playingNow)
            hostPassStart.reset();
        wasHostPlayingForTaps = playingNow;
    }

    // Taps from the key poller
    while (keyTapFifo.getNumReady() > 0)
    {
        const auto scope = keyTapFifo.read (1);
        if (scope.blockSize1 > 0 && session.getStep() == Step::tap)
            tapAt (keyTaps[(size_t) scope.startIndex1]);
    }

    // Taps from MIDI
    while (midiTapFifo.getNumReady() > 0)
    {
        const auto scope = midiTapFifo.read (1);
        if (scope.blockSize1 > 0 && session.getStep() == Step::tap)
        {
            tapAt (midiTaps[(size_t) scope.startIndex1]);
            if (onTapFromMidi)
                onTapFromMidi();
        }
    }

    if (preview.consumeFinished())
        session.sendChangeMessage();

    session.getReplacementSlot().collectGarbage();

    if (isBoundToARA())
    {
        // The host may hand over the events (or enable sample access) only after binding:
        // keep trying quietly once a second until the track is there.
        if (! session.hasSource() && ! isUsingAudioFile() && loader == nullptr && ++araRetryTicks >= 30)
        {
            araRetryTicks = 0;
            reloadTrack();
        }
        return;
    }

    // Without ARA: follow the host's tempo and record the track while it plays.
    juce::Optional<juce::AudioPlayHead::PositionInfo> info;
    {
        const juce::SpinLock::ScopedLockType lock (playHeadLock);
        info = lastPlayHead;
    }
    if (info.hasValue())
        if (auto tempo = tempoFromPlayHead (*info))
            session.setHostTempo (*tempo);

    // Record whenever the host plays during step 1. Every pass fills in or refreshes the part
    // that was played, so stopping early or starting in the middle is never a dead end.
    recorder.setArmed (session.getStep() == Step::tap && ! isUsingAudioFile());
    if (recorder.drain())
        session.sendChangeMessage();

    const bool playingNow = hostPlaying.load();
    if (wasHostPlaying && ! playingNow && recorder.hasAudio())
        loadFromRecorder();
    wasHostPlaying = playingNow;
}

void AlignMyTimeProcessor::loadFromRecorder()
{
    const auto recorded = recorder.getClip();
    recorder.reset();

    float peak = 0.0f;
    for (const auto& ch : recorded.channels)
        for (auto v : ch)
            peak = juce::jmax (peak, std::abs (v));

    if (peak < 1.0e-5f)
    {
        loadError = tr ("Beim Abspielen kam am Plugin kein Signal an – ist die Spur stummgeschaltet?");
        session.sendChangeMessage();
        return;
    }
    loadError.clear();

    // Merge with what was recorded before: the new pass wins where they overlap.
    auto merged = std::make_shared<AudioClip> (recorded);
    if (auto previous = session.getSource(); previous != nullptr && juce::approximatelyEqual (previous->sampleRate, recorded.sampleRate))
    {
        const auto start = std::min<int64_t> (previous->startSample, recorded.startSample);
        const auto end = std::max<int64_t> (previous->endSample(), recorded.endSample());
        merged = std::make_shared<AudioClip> (recorded.numChannels(), end - start, recorded.sampleRate, start);
        merged->mixIn (*previous);
        for (int c = 0; c < merged->numChannels(); ++c)
            std::copy (recorded.channels[(size_t) c].begin(), recorded.channels[(size_t) c].end(),
                       merged->channels[(size_t) c].begin() + (recorded.startSample - start));
    }

    session.setSource (merged, tr ("Aufnahme vom Spureingang"));
}

void AlignMyTimeProcessor::discardRecording()
{
    recorder.reset();
    loadError.clear();
    session.clearMarkers();
    session.setSource (nullptr, {});
}

juce::String AlignMyTimeProcessor::getBlockingReason (bool forRendering) const
{
    if (! session.hasSource())
    {
        if (fileLoader != nullptr)
            return tr ("Datei wird geladen …");
        if (isStandalone())
            return tr ("Lade zuerst eine Audiodatei (Button „Datei laden“ oder per Drag & Drop).");
        if (isBoundToARA())
            return loader != nullptr ? tr ("Spur wird geladen …")
                                     : withHostName (tr ("Warte auf die Audiodaten von Cubase …"));
        return withHostName (tr ("Spiele die Spur erst einmal in Cubase ab – oder lade eine Audiodatei."));
    }
    if (session.getMarkers().size() < 2)
        return tr ("Mindestens 2 Marker tappen.");
    if (forRendering && session.isRendering())
        return tr ("Wird berechnet …");
    return {};
}

void AlignMyTimeProcessor::reloadTrack()
{
    loadError.clear();

    if (isUsingAudioFile())
    {
        // Back from a file to the host's track.
        sourceFile = juce::File();
        session.setSource (nullptr, {});
    }

    if (! isBoundToARA())
    {
        session.sendChangeMessage();
        return; // without ARA the track arrives by playing it in the host
    }

    std::vector<juce::ARAPlaybackRegion*> regions;
    if (auto* renderer = getPlaybackRenderer())
        regions = renderer->getPlaybackRegions();
    if (regions.empty())
        if (auto* editorRenderer = getEditorRenderer())
            regions = editorRenderer->getPlaybackRegions();

    loader = std::make_unique<TrackLoader> (regions, currentSampleRate, juce::jmax (1, getMainBusNumOutputChannels()),
                                            [this] (TrackLoader::Result result) {
                                                auto finished = std::move (loader);
                                                loadError = result.error;
                                                if (result.tempo.has_value())
                                                    session.setHostTempo (*result.tempo);
                                                if (result.clip != nullptr && ! isUsingAudioFile())
                                                    session.setSource (result.clip, result.description);
                                                else
                                                    session.sendChangeMessage();
                                            });
    session.sendChangeMessage();
}

juce::String AlignMyTimeProcessor::audioFileWildcard()
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    return formats.getWildcardForAllFormats();
}

void AlignMyTimeProcessor::loadAudioFile (const juce::File& file)
{
    loadError.clear();
    if (fileLoader != nullptr)
        fileLoader->stopThread (5000);

    sourceFile = file;
    recorder.setArmed (false);
    recorder.reset();

    const double rate = currentSampleRate;
    const int channels = juce::jmax (1, getMainBusNumOutputChannels());
    std::weak_ptr<bool> weakAlive = alive;

    struct Loader : juce::Thread
    {
        Loader (juce::File f, double r, int c, std::function<void (std::shared_ptr<AudioClip>, juce::String)> done)
            : juce::Thread ("Align My Time file loader"), file (std::move (f)), rate (r), channels (c), onDone (std::move (done)) {}

        void run() override
        {
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
            if (reader == nullptr || reader->lengthInSamples <= 0)
            {
                onDone (nullptr, tr ("Diese Datei kann nicht gelesen werden: ") + file.getFileName());
                return;
            }

            const int length = (int) juce::jmin<juce::int64> (reader->lengthInSamples, std::numeric_limits<int>::max() / 2);
            juce::AudioBuffer<float> buffer ((int) juce::jmax (1u, reader->numChannels), length);
            reader->read (&buffer, 0, length, 0, true, true);
            if (threadShouldExit())
                return;

            if (! juce::approximatelyEqual (reader->sampleRate, rate))
            {
                const int outLength = (int) std::llround (length * rate / reader->sampleRate);
                juce::AudioBuffer<float> resampled (buffer.getNumChannels(), outLength);
                for (int c = 0; c < buffer.getNumChannels(); ++c)
                {
                    juce::LagrangeInterpolator interpolator;
                    interpolator.process (reader->sampleRate / rate, buffer.getReadPointer (c), resampled.getWritePointer (c), outLength);
                }
                buffer = std::move (resampled);
            }

            auto clip = std::make_shared<AudioClip> (channels, buffer.getNumSamples(), rate, 0);
            for (int c = 0; c < channels; ++c)
            {
                const auto* src = buffer.getReadPointer (juce::jmin (c, buffer.getNumChannels() - 1));
                std::copy (src, src + buffer.getNumSamples(), clip->channels[(size_t) c].begin());
            }
            onDone (clip, {});
        }

        juce::File file;
        double rate;
        int channels;
        std::function<void (std::shared_ptr<AudioClip>, juce::String)> onDone;
    };

    fileLoader = std::make_unique<Loader> (file, rate, channels, [this, weakAlive, file] (std::shared_ptr<AudioClip> clip, juce::String error) {
        juce::MessageManager::callAsync ([this, weakAlive, file, clip, error] {
            if (auto a = weakAlive.lock(); a == nullptr || ! *a)
                return;
            if (fileLoader != nullptr)
                fileLoader->stopThread (1000);
            fileLoader.reset();
            if (file != sourceFile)
                return; // another file was chosen meanwhile

            loadError = error;
            if (clip != nullptr)
                session.setSource (clip, tr ("Datei") + ": " + file.getFileName());
            else
                sourceFile = juce::File();
            session.sendChangeMessage();
        });
    });
    fileLoader->startThread();
    session.sendChangeMessage();
}

void AlignMyTimeProcessor::pushKeyTap (double songSeconds) noexcept
{
    const auto scope = keyTapFifo.write (1);
    if (scope.blockSize1 > 0)
        keyTaps[(size_t) scope.startIndex1] = songSeconds;
}

void AlignMyTimeProcessor::tapNow()
{
    if (tracker.isRunning())
        tapAt (tracker.nowSeconds());
}

void AlignMyTimeProcessor::tapAt (double songSeconds)
{
    if (hostPassStart.has_value())
    {
        session.beginTapping (juce::jmin (*hostPassStart, songSeconds - 0.1));
        hostPassStart.reset();
    }
    session.addTap (songSeconds);
}

void AlignMyTimeProcessor::startPreview (double fromSeconds, bool aligned, bool withLeadIn, bool withClick)
{
    auto clip = aligned ? session.getAligned() : session.getSource();
    if (clip == nullptr)
        return;

    preview.setClip (clip);
    preview.setClick (withClick, session.getProjectTempoShared());
    preview.setEndSeconds (clip->endSeconds() + 0.5);
    preview.play (fromSeconds, withLeadIn ? 2.0 : 0.0);
    session.sendChangeMessage();
}

void AlignMyTimeProcessor::stopPreview()
{
    preview.stop();
    session.sendChangeMessage();
}

//==============================================================================
void AlignMyTimeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto tree = session.toValueTree();

    if (isUsingAudioFile())
        tree.setProperty ("audioFile", sourceFile.getFullPathName(), nullptr);

    // Without ARA the recorded audio is not in the project: keep it next to it.
    if (! isBoundToARA() && session.hasSource() && ! isUsingAudioFile())
    {
        if (capturedClip != session.getSource().get() || ! captureFile.existsAsFile())
        {
            captureFile = captureFolder().getChildFile (juce::Uuid().toString() + ".wav");
            if (writeCapture (*session.getSource(), captureFile))
                capturedClip = session.getSource().get();
        }
        tree.setProperty ("captureFile", captureFile.getFullPathName(), nullptr);
        tree.setProperty ("captureStart", (juce::int64) session.getSource()->startSample, nullptr);
        tree.setProperty ("captureDescription", session.getSourceDescription(), nullptr);
    }

    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, destData);
}

void AlignMyTimeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    const auto tree = juce::ValueTree::fromXml (*xml);
    session.restoreFromValueTree (tree);

    const juce::File audioFile (tree.getProperty ("audioFile").toString());
    if (audioFile != juce::File() && audioFile.existsAsFile())
    {
        loadAudioFile (audioFile);
        return;
    }

    const juce::File capture (tree.getProperty ("captureFile").toString());
    if (! isBoundToARA() && capture.existsAsFile())
        if (auto clip = readCapture (capture, (juce::int64) tree.getProperty ("captureStart", 0)))
        {
            session.setSource (clip, tree.getProperty ("captureDescription").toString());
            captureFile = capture;
            capturedClip = clip.get();
        }
}

juce::AudioProcessorEditor* AlignMyTimeProcessor::createEditor()
{
    return new AlignMyTimeEditor (*this);
}

} // namespace amt::plugin

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new amt::plugin::AlignMyTimeProcessor();
}
