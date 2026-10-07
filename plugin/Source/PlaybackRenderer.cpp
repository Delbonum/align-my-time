#include "PlaybackRenderer.h"
#include "DocumentController.h"

namespace amt::plugin
{

void PlaybackRenderer::prepareToPlay (double rate, int maximumSamplesPerBlock, int numChannels,
                                      juce::AudioProcessor::ProcessingPrecision, AlwaysNonRealtime alwaysNonRealtime)
{
    sampleRate = rate;
    maxBlock = maximumSamplesPerBlock;
    channels = numChannels;
    buffered = alwaysNonRealtime == AlwaysNonRealtime::no;
    tempBuffer = std::make_unique<juce::AudioBuffer<float>> (numChannels, maximumSamplesPerBlock);

    readers.clear();
    for (auto* region : getPlaybackRegions())
        readerFor (region->getAudioModification()->getAudioSource());

    // Another instance may align this track along with its own: look up where it publishes.
    auto* track = getTrack();
    auto* controller = DocumentController::of (getDocumentController());
    linkedReplacement.store (track != nullptr && controller != nullptr ? &controller->getLinkedReplacement (track) : nullptr);
}

juce::ARARegionSequence* PlaybackRenderer::getTrack() const
{
    for (auto* region : getPlaybackRegions())
        if (auto* sequence = region->getRegionSequence())
            return sequence;
    return nullptr;
}

void PlaybackRenderer::releaseResources()
{
    readers.clear();
    tempBuffer.reset();
}

PlaybackRenderer::Reader& PlaybackRenderer::readerFor (juce::ARAAudioSource* source)
{
    auto it = readers.find (source);
    if (it != readers.end())
        return it->second;

    Reader r;
    auto direct = std::make_unique<juce::ARAAudioSourceReader> (source);
    if (buffered)
    {
        const int readAhead = juce::jmax (4 * maxBlock, juce::roundToInt (2.0 * sampleRate));
        auto b = std::make_unique<juce::BufferingAudioReader> (direct.release(), *readThread, readAhead);
        r.buffering = b.get();
        r.reader = std::move (b);
    }
    else
    {
        r.reader = std::move (direct);
    }
    return readers.emplace (source, std::move (r)).first->second;
}

bool PlaybackRenderer::processBlock (juce::AudioBuffer<float>& buffer, juce::AudioProcessor::Realtime realtime,
                                     const juce::AudioPlayHead::PositionInfo& positionInfo) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const auto blockStart = positionInfo.getTimeInSamples().orFallback (0);

    if (! positionInfo.getIsPlaying())
    {
        buffer.clear();
        return true;
    }

    // "Replace in track": play the aligned audio instead of the events.
    std::shared_ptr<const AudioClip> clip = replacement != nullptr ? replacement->get() : nullptr;
    if (clip == nullptr)
        if (auto* linked = linkedReplacement.load())
            clip = linked->get();

    if (clip != nullptr)
    {
        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            auto* out = buffer.getWritePointer (c);
            const int sourceChannel = juce::jmin (c, clip->numChannels() - 1);
            for (int i = 0; i < numSamples; ++i)
                out[i] = clip->sampleAt (sourceChannel, blockStart + i);
        }
        return true;
    }

    bool success = true;
    bool renderedAny = false;
    const auto blockRange = juce::Range<juce::int64>::withStartAndLength (blockStart, numSamples);

    for (auto* region : getPlaybackRegions())
    {
        const auto playbackRange = region->getSampleRange (sampleRate, juce::ARAPlaybackRegion::IncludeHeadAndTail::no);
        auto renderRange = blockRange.getIntersectionWith (playbackRange);
        if (renderRange.isEmpty())
            continue;

        const juce::Range<juce::int64> modificationRange { region->getStartInAudioModificationSamples(),
                                                           region->getEndInAudioModificationSamples() };
        const auto offset = modificationRange.getStart() - playbackRange.getStart();
        renderRange = renderRange.getIntersectionWith (modificationRange.movedToStartAt (playbackRange.getStart()));
        if (renderRange.isEmpty())
            continue;

        auto it = readers.find (region->getAudioModification()->getAudioSource());
        if (it == readers.end() || it->second.reader == nullptr)
        {
            success = false;
            continue;
        }

        auto& reader = it->second;
        if (reader.buffering != nullptr)
            reader.buffering->setReadTimeout (realtime == juce::AudioProcessor::Realtime::no ? 100 : 0);

        const int count = (int) renderRange.getLength();
        const int startInBuffer = (int) (renderRange.getStart() - blockStart);
        auto& target = renderedAny ? *tempBuffer : buffer;

        if (! reader.reader->read (&target, startInBuffer, count, renderRange.getStart() + offset, true, true))
        {
            success = false;
            continue;
        }

        if (renderedAny)
        {
            for (int c = 0; c < buffer.getNumChannels(); ++c)
                buffer.addFrom (c, startInBuffer, *tempBuffer, c, startInBuffer, count);
        }
        else
        {
            if (startInBuffer > 0)
                buffer.clear (0, startInBuffer);
            const int end = startInBuffer + count;
            if (end < numSamples)
                buffer.clear (end, numSamples - end);
            renderedAny = true;
        }
    }

    if (! renderedAny)
        buffer.clear();

    return success;
}

} // namespace amt::plugin
