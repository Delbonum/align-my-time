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
            .getChildFile ("Align my Time")
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
    loader.reset();
}

void AlignMyTimeProcessor::didBindToARA() noexcept
{
    juce::AudioProcessorARAExtension::didBindToARA();

    if (auto* renderer = getPlaybackRenderer<PlaybackRenderer>())
        renderer->setReplacementSource (&session.getReplacementSlot());

    // Load the track as soon as the host has told us about it.
    juce::MessageManager::callAsync ([safe = juce::WeakReference<AlignMyTimeProcessor> (this)] {
        if (auto* p = safe.get(); p != nullptr && ! p->session.hasSource())
            p->reloadTrack();
    });
}

void AlignMyTimeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    preview.prepare (sampleRate);
    recorder.prepare (sampleRate, getTotalNumInputChannels());
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
    // Taps from MIDI
    while (midiTapFifo.getNumReady() > 0)
    {
        const auto scope = midiTapFifo.read (1);
        if (scope.blockSize1 > 0 && session.getStep() == Step::tap)
        {
            session.addTap (midiTaps[(size_t) scope.startIndex1]);
            if (onTapFromMidi)
                onTapFromMidi();
        }
    }

    if (preview.consumeFinished())
        session.sendChangeMessage();

    session.getReplacementSlot().collectGarbage();

    if (isBoundToARA())
        return;

    // Without ARA: follow the host's tempo and record the track while it plays.
    juce::Optional<juce::AudioPlayHead::PositionInfo> info;
    {
        const juce::SpinLock::ScopedLockType lock (playHeadLock);
        info = lastPlayHead;
    }
    if (info.hasValue())
        if (auto tempo = tempoFromPlayHead (*info))
            session.setProjectTempo (*tempo);

    recorder.setArmed (! session.hasSource() && session.getStep() == Step::tap);
    if (recorder.drain())
        session.sendChangeMessage();

    const bool playingNow = hostPlaying.load();
    if (wasHostPlaying && ! playingNow && recorder.isArmed() && recorder.hasAudio())
        loadFromRecorder();
    wasHostPlaying = playingNow;
}

void AlignMyTimeProcessor::loadFromRecorder()
{
    recorder.setArmed (false);
    auto clip = std::make_shared<AudioClip> (recorder.getClip());
    recorder.reset();

    session.setSource (clip, juce::String::fromUTF8 ("Aufnahme vom Spureingang"));
}

void AlignMyTimeProcessor::reloadTrack()
{
    loadError.clear();

    if (! isBoundToARA())
    {
        // Start over: the next host playback is recorded.
        session.setSource (nullptr, {});
        recorder.reset();
        return;
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
                                                    session.setProjectTempo (*result.tempo);
                                                if (result.clip != nullptr)
                                                    session.setSource (result.clip, result.description);
                                                else
                                                    session.sendChangeMessage();
                                            });
    session.sendChangeMessage();
}

void AlignMyTimeProcessor::tapNow()
{
    if (tracker.isRunning())
        session.addTap (tracker.nowSeconds());
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

    // Without ARA the recorded audio is not in the project: keep it next to it.
    if (! isBoundToARA() && session.hasSource())
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
