#pragma once

#include "PluginProcessor.h"

namespace amt::plugin::project
{

/** Align My Time projects (standalone app): an XML file with the extension .amtp.
    It references the audio files (absolute and relative to the project) and holds the
    session: markers, settings and step. Schema: docs/PROJEKTFORMAT.md. */
constexpr int formatVersion = 1;
constexpr const char* fileExtension = ".amtp";
inline juce::String wildcard() { return "*.amtp"; }

/** The project as written to `projectFile` (relative paths are relative to its folder;
    pass juce::File() to leave them out). */
juce::ValueTree createProject (const AlignMyTimeProcessor& processor, const juce::File& projectFile);

/** Writes the project; returns an error message, or an empty string on success. */
juce::String save (const AlignMyTimeProcessor& processor, const juce::File& projectFile);

struct LoadResult
{
    juce::String error;              ///< not a readable project: nothing was changed
    juce::StringArray missingFiles;  ///< audio files that were found neither at their path nor next to the project
};

/** Replaces the current session with the project. Audio files are looked up next to the
    project first (so a project folder can be moved), then at their original path. */
LoadResult load (AlignMyTimeProcessor& processor, const juce::File& projectFile);

/** Compact description of everything a project saves, for "unsaved changes?" checks. */
juce::String fingerprint (const AlignMyTimeProcessor& processor);

} // namespace amt::plugin::project
