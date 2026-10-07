#include "Exporter.h"
#include "Localisation.h"

namespace amt::plugin
{

juce::File Exporter::defaultFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Align My Time");
}

juce::File Exporter::newFileFor (const juce::String& name)
{
    const auto safeName = juce::File::createLegalFileName (name.isNotEmpty() ? name : juce::String ("Align My Time"));
    return defaultFolder().getNonexistentChildFile (safeName, ".wav", false);
}

bool Exporter::writeWav (const AudioClip& clip, const juce::File& file, bool fromProjectStart, juce::String& errorMessage)
{
    const auto folder = file.getParentDirectory();
    if (! folder.createDirectory())
    {
        errorMessage = tr ("Ordner kann nicht angelegt werden: ") + folder.getFullPathName();
        return false;
    }

    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
    if (stream == nullptr)
    {
        errorMessage = tr ("Datei kann nicht geschrieben werden: ") + file.getFullPathName();
        return false;
    }

    juce::WavAudioFormat wav;
    const auto options = juce::AudioFormatWriterOptions {}
                             .withSampleRate (clip.sampleRate)
                             .withNumChannels (clip.numChannels())
                             .withBitsPerSample (24);
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

    for (juce::int64 pos = skip; pos < clip.numSamples(); pos += chunk)
    {
        const int n = (int) juce::jmin<juce::int64> (chunk, clip.numSamples() - pos);
        for (int c = 0; c < clip.numChannels(); ++c)
            buffer.copyFrom (c, 0, clip.channels[(size_t) c].data() + pos, n);
        writer->writeFromAudioSampleBuffer (buffer, 0, n);
    }

    writer.reset();
    return true;
}

} // namespace amt::plugin
