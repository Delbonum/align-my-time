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

    /** Writes a 24-bit WAV. With `fromProjectStart`, silence is prepended so the file starts at
        song position 0 and lines up when dropped at bar 1. Returns the file, or an empty File. */
    static juce::File writeWav (const AudioClip& clip, const juce::String& name, bool fromProjectStart,
                                juce::String& errorMessage);
};

} // namespace amt::plugin
