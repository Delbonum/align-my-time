#pragma once

#include <amt/AudioClip.h>
#include <juce_audio_formats/juce_audio_formats.h>

namespace amt::plugin
{

/** Writes rendered audio to disk so it can be dragged onto a new track. */
struct Exporter
{
    /** Default folder: <Music>/Align My Time. */
    static juce::File defaultFolder();

    /** A file in the default folder named after `name` that does not exist yet. */
    static juce::File newFileFor (const juce::String& name);

    /** Writes a 24-bit WAV to `file` (replacing it). With `fromProjectStart`, silence is prepended so
        the file starts at song position 0 and lines up when dropped at bar 1. */
    static bool writeWav (const AudioClip& clip, const juce::File& file, bool fromProjectStart, juce::String& errorMessage);
};

} // namespace amt::plugin
