#include "Exporter.h"
#include "Localisation.h"
#include "SourceLoader.h"

namespace amt::plugin
{

juce::File Exporter::defaultFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Align My Time");
}

juce::File Exporter::newFileFor (const juce::String& name, const juce::String& extension)
{
    const auto safeName = juce::File::createLegalFileName (name.isNotEmpty() ? name : juce::String ("Align My Time"));
    return defaultFolder().getNonexistentChildFile (safeName, extension, false);
}

bool Exporter::writeWav (const AudioClip& source, const juce::File& file, bool fromProjectStart, const ExportFormat& format,
                         juce::String& errorMessage)
{
    const auto folder = file.getParentDirectory();
    if (! folder.createDirectory())
    {
        errorMessage = tr ("Ordner kann nicht angelegt werden: ") + folder.getFullPathName();
        return false;
    }

    // Another sample rate: convert a copy (positions scale with it, so the grid still lines up).
    std::shared_ptr<const AudioClip> converted;
    if (format.sampleRate > 0.0 && std::abs (format.sampleRate - source.sampleRate) > 0.5)
    {
        juce::AudioBuffer<float> buffer (source.numChannels(), (int) source.numSamples());
        for (int c = 0; c < source.numChannels(); ++c)
            buffer.copyFrom (c, 0, source.channels[(size_t) c].data(), buffer.getNumSamples());
        resampleInPlace (buffer, source.sampleRate, format.sampleRate);
        converted = makeClip (buffer, source.numChannels(), format.sampleRate,
                              (int64_t) std::llround ((double) source.startSample * format.sampleRate / source.sampleRate));
    }
    const auto& clip = converted != nullptr ? *converted : source;

    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
    if (stream == nullptr)
    {
        errorMessage = tr ("Datei kann nicht geschrieben werden: ") + file.getFullPathName();
        return false;
    }

    const bool floatingPoint = format.bitsPerSample >= 32;
    const int bits = floatingPoint ? 32 : format.bitsPerSample <= 16 ? 16 : 24;
    juce::WavAudioFormat wav;
    auto options = juce::AudioFormatWriterOptions {}
                       .withSampleRate (clip.sampleRate)
                       .withNumChannels (clip.numChannels())
                       .withBitsPerSample (bits);
    if (floatingPoint)
        options = options.withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
    auto writer = wav.createWriterFor (stream, options);
    if (writer == nullptr)
    {
        errorMessage = tr ("WAV-Writer konnte nicht erstellt werden.");
        return false;
    }

    // Material before song position 0 (a stretched pick-up) is cut when padding from the start.
    const juce::int64 lead = fromProjectStart ? juce::jmax<juce::int64> (0, clip.startSample) : 0;
    const juce::int64 skip = fromProjectStart ? juce::jmax<juce::int64> (0, -clip.startSample) : 0;

    constexpr int chunk = 8192;
    juce::AudioBuffer<float> buffer (clip.numChannels(), chunk);

    for (juce::int64 written = 0; written < lead; written += chunk)
    {
        const int n = (int) juce::jmin<juce::int64> (chunk, lead - written);
        buffer.clear();
        writer->writeFromAudioSampleBuffer (buffer, 0, n);
    }

    // 16 bit: triangular (TPDF) dither of one LSB instead of plain rounding.
    juce::Random random (1234);
    const float lsb = 1.0f / 32768.0f;

    for (juce::int64 pos = skip; pos < clip.numSamples(); pos += chunk)
    {
        const int n = (int) juce::jmin<juce::int64> (chunk, clip.numSamples() - pos);
        for (int c = 0; c < clip.numChannels(); ++c)
        {
            buffer.copyFrom (c, 0, clip.channels[(size_t) c].data() + pos, n);
            if (bits == 16)
            {
                auto* data = buffer.getWritePointer (c);
                for (int i = 0; i < n; ++i)
                    data[i] += (random.nextFloat() - random.nextFloat()) * lsb;
            }
        }
        writer->writeFromAudioSampleBuffer (buffer, 0, n);
    }

    writer.reset();
    return true;
}

bool Exporter::writeTempoMap (const TempoMap& tempo, double endSeconds, const juce::File& file, juce::String& errorMessage)
{
    const int ticksPerQuarter = 960;
    auto tick = [ticksPerQuarter] (double quarters) { return std::round (juce::jmax (0.0, quarters) * ticksPerQuarter); };

    juce::MidiMessageSequence track;
    track.addEvent (juce::MidiMessage::textMetaEvent (3, "Align My Time"), 0.0);
    for (const auto& sig : tempo.signatures())
        track.addEvent (juce::MidiMessage::timeSignatureMetaEvent (sig.numerator, sig.denominator), tick (sig.quarters));

    // One tempo event per tempo point: the tempo up to the next point.
    const auto& points = tempo.points();
    for (size_t i = 0; i + 1 < points.size(); ++i)
    {
        const double quartersPerSecond = (points[i + 1].quarters - points[i].quarters) / (points[i + 1].seconds - points[i].seconds);
        track.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (1.0e6 / quartersPerSecond)), tick (points[i].quarters));
    }
    track.addEvent (juce::MidiMessage::endOfTrack(), tick (tempo.secondsToQuarters (juce::jmax (endSeconds, points.back().seconds))));

    juce::MidiFile midi;
    midi.setTicksPerQuarterNote (ticksPerQuarter);
    midi.addTrack (track);

    file.getParentDirectory().createDirectory();
    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk() || ! midi.writeTo (out, 1))
    {
        errorMessage = tr ("Datei kann nicht geschrieben werden: ") + file.getFullPathName();
        return false;
    }
    return true;
}

} // namespace amt::plugin
