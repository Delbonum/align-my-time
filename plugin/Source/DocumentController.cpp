#include "DocumentController.h"
#include "PlaybackRenderer.h"

namespace amt::plugin
{

juce::ARAPlaybackRenderer* DocumentController::doCreatePlaybackRenderer() noexcept
{
    return new PlaybackRenderer (getDocumentController());
}

} // namespace amt::plugin

const ARA::ARAFactory* JUCE_CALLTYPE createARAFactory()
{
    return juce::ARADocumentControllerSpecialisation::createARAFactory<amt::plugin::DocumentController>();
}
