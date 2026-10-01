#include "SourceLoader.h"

#include <cmath>

namespace amt::plugin
{

std::optional<TempoMap> tempoFromMusicalContext (juce::ARAMusicalContext* context)
{
    if (context == nullptr)
        return std::nullopt;

    const ARA::PlugIn::HostContentReader<ARA::kARAContentTypeTempoEntries> tempoReader (context);
    if (! tempoReader || tempoReader.getEventCount() < 2)
        return std::nullopt;

    std::vector<TempoPoint> points;
    for (ARA::ARAInt32 i = 0; i < tempoReader.getEventCount(); ++i)
    {
        const auto entry = tempoReader.getDataForEvent (i);
        points.push_back ({ entry.timePosition, entry.quarterPosition });
    }

    std::vector<TimeSignature> signatures;
    const ARA::PlugIn::HostContentReader<ARA::kARAContentTypeBarSignatures> signatureReader (context);
    if (signatureReader)
    {
        for (ARA::ARAInt32 i = 0; i < signatureReader.getEventCount(); ++i)
        {
            const auto sig = signatureReader.getDataForEvent (i);
            signatures.push_back ({ sig.position, (int) sig.numerator, (int) sig.denominator });
        }
    }

    return TempoMap (std::move (points), std::move (signatures));
}

std::optional<TempoMap> tempoFromPlayHead (const juce::AudioPlayHead::PositionInfo& info)
{
    const auto bpm = info.getBpm();
    if (! bpm.hasValue() || *bpm <= 0.0)
        return std::nullopt;

    const auto sig = info.getTimeSignature().orFallback (juce::AudioPlayHead::TimeSignature {});
    const double seconds = info.getTimeInSeconds().orFallback (0.0);
    const double ppq = info.getPpqPosition().orFallback (seconds * *bpm / 60.0);
    const double barStart = info.getPpqPositionOfLastBarStart().orFallback (0.0);

    // Anchor the constant tempo at the current position; bars are counted from the host's last
    // bar start (which keeps the grid right even if the song starts with a pick-up).
    const double quartersPerBar = sig.numerator * 4.0 / sig.denominator;
    const double firstBarQuarters = barStart - std::floor (barStart / quartersPerBar) * quartersPerBar;

    return TempoMap ({ { seconds, ppq }, { seconds + 60.0, ppq + *bpm } },
                     { { firstBarQuarters, sig.numerator, sig.denominator } });
}

//==============================================================================
TrackLoader::TrackLoader (const std::vector<juce::ARAPlaybackRegion*>& playbackRegions, double targetSampleRate, int numChannels,
                          std::function<void (Result)> done)
    : juce::Thread ("Align my Time track loader"), sampleRate (targetSampleRate), channels (juce::jmax (1, numChannels)), onDone (std::move (done))
{
    juce::String trackName;
    juce::ARAMusicalContext* musicalContext = nullptr;

    for (auto* region : playbackRegions)
    {
        auto* source = region->getAudioModification()->getAudioSource();
        if (! source->isSampleAccessEnabled())
            continue;

        Region r;
        r.reader = std::make_unique<juce::ARAAudioSourceReader> (source);
        r.playbackStart = region->getStartInPlaybackTime();
        r.playbackEnd = region->getEndInPlaybackTime();
        r.modificationStart = region->getStartInAudioModificationTime();
        regions.push_back (std::move (r));

        if (auto* sequence = region->getRegionSequence())
        {
            if (trackName.isEmpty() && sequence->getName() != nullptr)
                trackName = juce::String::fromUTF8 (sequence->getName());
            if (musicalContext == nullptr)
                musicalContext = sequence->getMusicalContext();
        }
    }

    result.tempo = tempoFromMusicalContext (musicalContext);

    const auto events = juce::String ((int) regions.size()) + (regions.size() == 1 ? " Event" : " Events");
    result.description = (trackName.isNotEmpty() ? juce::String::fromUTF8 ("Spur \xe2\x80\x9e") + trackName + juce::String::fromUTF8 ("\xe2\x80\x9c") : juce::String ("Spur"))
                         + juce::String::fromUTF8 (" \xc2\xb7 ") + events
                         + (regions.size() > 1 ? juce::String (" zu einem zusammengefasst") : juce::String());

    if (regions.empty())
    {
        result.error = playbackRegions.empty() ? "Keine Events gefunden." : "Der Host gibt die Audiodaten (noch) nicht frei.";
        juce::MessageManager::callAsync ([weak = std::weak_ptr<bool> (alive), this] {
            if (auto a = weak.lock(); a != nullptr && *a)
                onDone (result);
        });
        return;
    }

    startThread();
}

TrackLoader::~TrackLoader()
{
    *alive = false;
    stopThread (5000);
}

void TrackLoader::run()
{
    double start = 1.0e12, end = -1.0e12;
    for (const auto& r : regions)
    {
        start = juce::jmin (start, r.playbackStart);
        end = juce::jmax (end, r.playbackEnd);
    }

    auto clip = std::make_shared<AudioClip> (channels, (int64_t) std::ceil ((end - start) * sampleRate), sampleRate,
                                             (int64_t) std::llround (start * sampleRate));

    for (size_t i = 0; i < regions.size() && ! threadShouldExit(); ++i)
    {
        auto& r = regions[i];
        auto* reader = r.reader.get();
        if (! reader->isValid())
            continue;

        const double sourceRate = reader->sampleRate;
        const auto sourceStart = (juce::int64) std::llround (r.modificationStart * sourceRate);
        const int sourceLength = (int) std::llround ((r.playbackEnd - r.playbackStart) * sourceRate);
        if (sourceLength <= 0)
            continue;

        juce::AudioBuffer<float> buffer ((int) juce::jmax (1u, reader->numChannels), sourceLength);
        reader->read (&buffer, 0, sourceLength, sourceStart, true, true);

        // Bring everything to the session's sample rate.
        if (std::abs (sourceRate - sampleRate) > 0.5)
        {
            const int outLength = (int) std::llround (sourceLength * sampleRate / sourceRate);
            juce::AudioBuffer<float> resampled (buffer.getNumChannels(), outLength);
            for (int c = 0; c < buffer.getNumChannels(); ++c)
            {
                juce::LagrangeInterpolator interpolator;
                interpolator.process (sourceRate / sampleRate, buffer.getReadPointer (c), resampled.getWritePointer (c), outLength);
            }
            buffer = std::move (resampled);
        }

        AudioClip regionClip (buffer.getNumChannels(), buffer.getNumSamples(), sampleRate, (int64_t) std::llround (r.playbackStart * sampleRate));
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            std::copy (buffer.getReadPointer (c), buffer.getReadPointer (c) + buffer.getNumSamples(), regionClip.channels[(size_t) c].begin());

        clip->mixIn (regionClip);
        progress.store ((double) (i + 1) / (double) regions.size());
    }

    if (threadShouldExit())
        return;

    result.clip = clip;
    juce::MessageManager::callAsync ([weak = std::weak_ptr<bool> (alive), this] {
        if (auto a = weak.lock(); a != nullptr && *a)
            onDone (result);
    });
}

//==============================================================================
void InputRecorder::prepare (double sampleRate, int numChannels)
{
    rate = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    if (! juce::approximatelyEqual (clip.sampleRate, rate))
        clip = AudioClip (channels, 0, rate, 0);
}

void InputRecorder::push (const juce::AudioBuffer<float>& input, juce::int64 songSample) noexcept
{
    if (! armed.load())
        return;

    const int numSamples = input.getNumSamples();
    for (int offset = 0; offset < numSamples; offset += blockFrames)
    {
        const int frames = juce::jmin (blockFrames, numSamples - offset);
        const auto scope = fifo.write (1);
        if (scope.blockSize1 + scope.blockSize2 == 0)
            return; // message thread is not keeping up: drop rather than block

        auto& block = blocks[(size_t) (scope.blockSize1 > 0 ? scope.startIndex1 : scope.startIndex2)];
        block.songSample = songSample + offset;
        block.frames = frames;
        for (int c = 0; c < 2; ++c)
        {
            const auto* src = input.getReadPointer (juce::jmin (c, input.getNumChannels() - 1), offset);
            std::copy (src, src + frames, block.data[c]);
        }
    }
}

bool InputRecorder::drain()
{
    bool any = false;
    while (fifo.getNumReady() > 0)
    {
        const auto scope = fifo.read (1);
        const auto& block = blocks[(size_t) (scope.blockSize1 > 0 ? scope.startIndex1 : scope.startIndex2)];
        any = true;

        // Grow the clip to cover the block (front or back), then overwrite.
        if (clip.isEmpty())
            clip = AudioClip (channels, 0, rate, block.songSample);

        const auto newStart = std::min<int64_t> (clip.startSample, block.songSample);
        const auto newEnd = std::max<int64_t> (clip.endSample(), block.songSample + block.frames);
        if (newStart < clip.startSample || newEnd > clip.endSample())
        {
            for (auto& ch : clip.channels)
            {
                ch.insert (ch.begin(), (size_t) (clip.startSample - newStart), 0.0f);
                ch.resize ((size_t) (newEnd - newStart), 0.0f);
            }
            clip.startSample = newStart;
        }

        for (int c = 0; c < clip.numChannels(); ++c)
            std::copy (block.data[c], block.data[c] + block.frames,
                       clip.channels[(size_t) c].begin() + (block.songSample - clip.startSample));
    }
    return any;
}

void InputRecorder::reset()
{
    while (fifo.getNumReady() > 0)
        fifo.read (fifo.getNumReady());
    clip = AudioClip (channels, 0, rate, 0);
}

} // namespace amt::plugin
