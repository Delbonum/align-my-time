#include "DocumentController.h"
#include "PlaybackRenderer.h"

namespace amt::plugin
{

DocumentController* DocumentController::of (ARA::PlugIn::DocumentController* controller)
{
    if (controller == nullptr)
        return nullptr;
    return juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<DocumentController> (controller);
}

SharedClip& DocumentController::getLinkedReplacement (const juce::ARARegionSequence* track)
{
    JUCE_ASSERT_MESSAGE_THREAD
    return *links[track].replacement;
}

void DocumentController::setLinkOwner (const juce::ARARegionSequence* track, const void* owner, const juce::String& ownerName)
{
    auto& link = links[track];
    link.owner = owner;
    link.ownerName = ownerName;
}

juce::String DocumentController::getLinkOwnerName (const juce::ARARegionSequence* track) const
{
    const auto it = links.find (track);
    return it != links.end() && it->second.owner != nullptr ? it->second.ownerName : juce::String();
}

void DocumentController::releaseLinks (const void* owner, const std::vector<const juce::ARARegionSequence*>& keep)
{
    for (auto& [track, link] : links)
    {
        if (link.owner != owner || std::find (keep.begin(), keep.end(), track) != keep.end())
            continue;
        link.owner = nullptr;
        link.ownerName.clear();
        link.replacement->set (nullptr);
    }
}

juce::ARAPlaybackRenderer* DocumentController::doCreatePlaybackRenderer() noexcept
{
    return new PlaybackRenderer (getDocumentController());
}

juce::ARARegionSequence* DocumentController::doCreateRegionSequence (juce::ARADocument* document, ARA::ARARegionSequenceHostRef hostRef)
{
    auto* track = juce::ARADocumentControllerSpecialisation::doCreateRegionSequence (document, hostRef);
    track->addListener (this);
    return track;
}

void DocumentController::willDestroyRegionSequence (juce::ARARegionSequence* track)
{
    // A new track might later get the same address: it must not inherit this one's link.
    const auto it = links.find (track);
    if (it == links.end())
        return;
    it->second.owner = nullptr;
    it->second.ownerName.clear();
    it->second.replacement->set (nullptr);
}

} // namespace amt::plugin

const ARA::ARAFactory* JUCE_CALLTYPE createARAFactory()
{
    return juce::ARADocumentControllerSpecialisation::createARAFactory<amt::plugin::DocumentController>();
}
