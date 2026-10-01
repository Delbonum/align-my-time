#include "Exporter.h"

namespace amt::plugin
{

juce::File Exporter::defaultFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Align my Time");
}

juce::File Exporter::writeWav (const AudioClip& clip, const juce::String& name, bool fromProjectStart, juce::String& errorMessage)
{
    auto folder = defaultFolder();
    if (! folder.createDirectory())
    {
        errorMessage = "Ordner kann nicht angelegt werden: " + folder.getFullPathName();
        return {};
    }

    const auto safeName = juce::File::createLegalFileName (name.isNotEmpty() ? name : juce::String ("Align my Time"));
    auto file = folder.getNonexistentChildFile (safeName, ".wav", false);

    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
    if (stream == nullptr)
    {
        errorMessage = "Datei kann nicht geschrieben werden: " + file.getFullPathName();
        return {};
    }

    juce::WavAudioFormat wav;
    const auto options = juce::AudioFormatWriterOptions {}
                             .withSampleRate (clip.sampleRate)
                             .withNumChannels (clip.numChannels())
                             .withBitsPerSample (24);
    auto writer = wav.createWriterFor (stream, options);
    if (writer == nullptr)
    {
        errorMessage = "WAV-Writer konnte nicht erstellt werden.";
        return {};
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

    for (juce::int64 pos = skip; pos < clip.numSamples(); pos += chunk)
    {
        const int n = (int) juce::jmin<juce::int64> (chunk, clip.numSamples() - pos);
        for (int c = 0; c < clip.numChannels(); ++c)
            buffer.copyFrom (c, 0, clip.channels[(size_t) c].data() + pos, n);
        writer->writeFromAudioSampleBuffer (buffer, 0, n);
    }

    writer.reset();
    return file;
}

} // namespace amt::plugin
