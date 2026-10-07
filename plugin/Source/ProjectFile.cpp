#include "ProjectFile.h"

namespace amt::plugin::project
{

namespace ids
{
    static const juce::Identifier root ("AlignMyTimeProject"), formatVersion ("formatVersion"), appVersion ("appVersion"),
        audio ("Audio"), file ("File"), role ("role"), name ("name"), path ("path"), relativePath ("relativePath"),
        session ("AlignMyTime"), step ("step"), audioFile ("audioFile"),
        extraTracks ("ExtraTracks"), extraTrack ("ExtraTrack"), kind ("kind"), id ("id");
    static const juce::String mainRole ("main"), extraRole ("extra");
}

namespace
{
    juce::ValueTree fileNode (const juce::String& role, const juce::String& name, const juce::File& file, const juce::File& projectFile)
    {
        juce::ValueTree node (ids::file);
        node.setProperty (ids::role, role, nullptr);
        node.setProperty (ids::name, name, nullptr);
        node.setProperty (ids::path, file.getFullPathName(), nullptr);
        if (projectFile != juce::File())
            node.setProperty (ids::relativePath, file.getRelativePathFrom (projectFile.getParentDirectory()).replaceCharacter ('\\', '/'), nullptr);
        return node;
    }

    /** Next to the project first (the folder may have been moved or copied), then the original path. */
    juce::File resolve (const juce::ValueTree& node, const juce::File& projectFile)
    {
        const auto relative = node.getProperty (ids::relativePath).toString();
        if (relative.isNotEmpty())
        {
            const auto candidate = projectFile.getParentDirectory().getChildFile (relative);
            if (candidate.existsAsFile())
                return candidate;
        }

        const auto absolute = node.getProperty (ids::path).toString();
        if (juce::File::isAbsolutePath (absolute) && juce::File (absolute).existsAsFile())
            return juce::File (absolute);
        return {};
    }
}

juce::ValueTree createProject (const AlignMyTimeProcessor& processor, const juce::File& projectFile)
{
    juce::ValueTree project (ids::root);
    project.setProperty (ids::formatVersion, formatVersion, nullptr);
    project.setProperty (ids::appVersion, versionString(), nullptr);

    const auto& session = processor.getSession();
    juce::ValueTree audio (ids::audio);
    if (processor.isUsingAudioFile())
        audio.appendChild (fileNode (ids::mainRole, processor.getAudioFile().getFileNameWithoutExtension(), processor.getAudioFile(), projectFile), nullptr);
    for (const auto& track : session.getExtraTracks())
        if (track.kind == ExtraTrack::Kind::file)
            audio.appendChild (fileNode (ids::extraRole, track.name, juce::File (track.id), projectFile), nullptr);
    project.appendChild (audio, nullptr);

    // The session as the plug-in stores it; the extra tracks are listed under <Audio> instead.
    auto state = session.toValueTree();
    state.removeChild (state.getChildWithName (ids::extraTracks), nullptr);
    project.appendChild (state, nullptr);
    return project;
}

juce::String save (const AlignMyTimeProcessor& processor, const juce::File& projectFile)
{
    const auto error = tr ("Das Projekt kann nicht gespeichert werden:") + " " + projectFile.getFullPathName();
    auto xml = createProject (processor, projectFile).createXml();
    if (xml == nullptr)
        return error;

    // Written next to the target first, so a failed save never destroys the previous version.
    juce::TemporaryFile temp (projectFile);
    if (! xml->writeTo (temp.getFile()) || ! temp.overwriteTargetFileWithTemporary())
        return error;
    return {};
}

LoadResult load (AlignMyTimeProcessor& processor, const juce::File& projectFile)
{
    LoadResult result;
    const auto xml = juce::parseXML (projectFile);
    if (xml == nullptr || ! xml->hasTagName (ids::root.toString()))
    {
        result.error = tr ("Das ist keine Projektdatei von Align My Time:") + " " + projectFile.getFileName();
        return result;
    }

    const auto project = juce::ValueTree::fromXml (*xml);
    if ((int) project.getProperty (ids::formatVersion, 1) > formatVersion)
    {
        result.error = tr ("Diese Projektdatei stammt aus einer neueren Version von Align My Time. Bitte aktualisiere die App.");
        return result;
    }

    auto state = project.getChildWithName (ids::session).createCopy();
    if (! state.isValid())
    {
        result.error = tr ("Das ist keine Projektdatei von Align My Time:") + " " + projectFile.getFileName();
        return result;
    }

    juce::ValueTree extras (ids::extraTracks);
    for (const auto& node : project.getChildWithName (ids::audio))
    {
        const auto file = resolve (node, projectFile);
        if (file == juce::File())
        {
            const auto original = node.getProperty (ids::path).toString();
            result.missingFiles.add (original.isNotEmpty() ? original : node.getProperty (ids::name).toString());
            continue;
        }

        if (node.getProperty (ids::role).toString() == ids::mainRole)
        {
            state.setProperty (ids::audioFile, file.getFullPathName(), nullptr);
        }
        else
        {
            juce::ValueTree track (ids::extraTrack);
            track.setProperty (ids::kind, (int) ExtraTrack::Kind::file, nullptr);
            track.setProperty (ids::id, file.getFullPathName(), nullptr);
            track.setProperty (ids::name, node.getProperty (ids::name, file.getFileNameWithoutExtension()), nullptr);
            extras.appendChild (track, nullptr);
        }
    }
    state.removeChild (state.getChildWithName (ids::extraTracks), nullptr);
    state.appendChild (extras, nullptr);

    processor.newProject();
    processor.restoreState (state);
    return result;
}

juce::String fingerprint (const AlignMyTimeProcessor& processor)
{
    // Which step is shown is no reason to ask "save changes?".
    auto project = createProject (processor, {});
    project.removeProperty (ids::appVersion, nullptr);
    project.getChildWithName (ids::session).removeProperty (ids::step, nullptr);
    return project.toXmlString();
}

} // namespace amt::plugin::project
