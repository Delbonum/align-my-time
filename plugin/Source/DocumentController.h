#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace amt::plugin
{

/** ARA document controller: creates the playback renderer. The alignment data itself is stored
    with each plug-in instance (getStateInformation), so the archive needs no extra objects. */
class DocumentController final : public juce::ARADocumentControllerSpecialisation
{
public:
    using juce::ARADocumentControllerSpecialisation::ARADocumentControllerSpecialisation;

protected:
    juce::ARAPlaybackRenderer* doCreatePlaybackRenderer() noexcept override;

    bool doRestoreObjectsFromStream (juce::ARAInputStream&, const juce::ARARestoreObjectsFilter*) noexcept override { return true; }
    bool doStoreObjectsToStream (juce::ARAOutputStream&, const juce::ARAStoreObjectsFilter*) noexcept override { return true; }
};

} // namespace amt::plugin
