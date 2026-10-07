#pragma once

#include "SharedClip.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <map>

namespace amt::plugin
{

/** ARA document controller: creates the playback renderer. The alignment data itself is stored
    with each plug-in instance (getStateInformation), so the archive needs no extra objects.

    It also links the instances of one project: an instance that aligns other tracks along with its
    own (multitrack drums) publishes their aligned audio here, and the playback renderers on those
    tracks play it for "replace in track". */
class DocumentController final : public juce::ARADocumentControllerSpecialisation
{
public:
    using juce::ARADocumentControllerSpecialisation::ARADocumentControllerSpecialisation;

    /** Our specialisation of an ARA document controller, or nullptr. */
    static DocumentController* of (ARA::PlugIn::DocumentController* controller);

    /** What the renderer of `track` plays instead of its events. The slot lives as long as the
        document, so renderers may keep the reference. Message thread. */
    SharedClip& getLinkedReplacement (const juce::ARARegionSequence* track);

    /** Marks `track` as aligned along by the instance `owner` (shown as `ownerName`, its track). */
    void setLinkOwner (const juce::ARARegionSequence* track, const void* owner, const juce::String& ownerName);
    /** The name of the instance aligning `track` along with its own; empty if none. */
    juce::String getLinkOwnerName (const juce::ARARegionSequence* track) const;

    /** Removes the links of `owner` (all of them, or all but `keep`) and clears their replacements. */
    void releaseLinks (const void* owner, const std::vector<const juce::ARARegionSequence*>& keep = {});

protected:
    juce::ARAPlaybackRenderer* doCreatePlaybackRenderer() noexcept override;
    juce::ARARegionSequence* doCreateRegionSequence (juce::ARADocument*, ARA::ARARegionSequenceHostRef) override;
    void willDestroyRegionSequence (juce::ARARegionSequence*) override;

    bool doRestoreObjectsFromStream (juce::ARAInputStream&, const juce::ARARestoreObjectsFilter*) noexcept override { return true; }
    bool doStoreObjectsToStream (juce::ARAOutputStream&, const juce::ARAStoreObjectsFilter*) noexcept override { return true; }

private:
    struct Link
    {
        std::unique_ptr<SharedClip> replacement = std::make_unique<SharedClip>();
        const void* owner = nullptr;
        juce::String ownerName;
    };

    // Entries are never erased, so the slots stay valid for the renderers holding them.
    std::map<const juce::ARARegionSequence*, Link> links;

    JUCE_DECLARE_WEAK_REFERENCEABLE (DocumentController)
};

} // namespace amt::plugin
