#pragma once

#include <amt/AudioClip.h>
#include <amt/TempoMap.h>
#include <juce_audio_formats/juce_audio_formats.h>

namespace amt::plugin
{

/** File format of exported audio. */
struct ExportFormat
{
    int bitsPerSample = 24;  ///< 16 (with dither), 24, or 32 (float)
    double sampleRate = 0.0; ///< 0 = as rendered (the session's rate)
};

/** Writes rendered audio (and tempo maps) to disk so they can be dragged into the DAW. */
struct Exporter
{
    /** Default folder: <Music>/Align My Time. */
    static juce::File defaultFolder();

    /** A file in the default folder named after `name` that does not exist yet. */
    static juce::File newFileFor (const juce::String& name, const juce::String& extension = ".wav");

    /** Writes a WAV to `file` (replacing it). With `fromProjectStart`, silence is prepended so the
        file starts at song position 0 and lines up when dropped at bar 1. */
    static bool writeWav (const AudioClip& clip, const juce::File& file, bool fromProjectStart, const ExportFormat& format,
                          juce::String& errorMessage);

    /** Writes `tempo` as a standard MIDI file (type 1, one track with tempo and time signature events),
        from song position 0 up to `endSeconds`. */
    static bool writeTempoMap (const TempoMap& tempo, double endSeconds, const juce::File& file, juce::String& errorMessage);
};

} // namespace amt::plugin
