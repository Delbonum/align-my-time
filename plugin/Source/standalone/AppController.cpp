#include "AppController.h"
#include "../PluginEditor.h"
#include "../ProjectFile.h"

namespace amt::plugin
{

namespace
{
    constexpr auto cmd = juce::ModifierKeys::commandModifier;
    constexpr auto shift = juce::ModifierKeys::shiftModifier;

    juce::File defaultProjectFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Align My Time");
    }
}

AppController::AppController (AlignMyTimeProcessor& p) : processor (p), lastLanguage (getLanguage())
{
    recentFiles.setMaxNumberOfItems (10);
    recentFiles.restoreFromString (appSettings().getValue ("recentProjects"));

    commands.registerAllCommandsForTarget (this);
    commands.setFirstCommandTarget (this);
    startTimerHz (4);
}

AppController::~AppController()
{
    stopTimer();
    commands.setFirstCommandTarget (nullptr);
}

AlignMyTimeEditor* AppController::getEditor() const
{
    return dynamic_cast<AlignMyTimeEditor*> (processor.getActiveEditor());
}

//==============================================================================
juce::String AppController::getWindowTitle() const
{
    if (projectFile == juce::File() && ! hasUnsavedChanges())
        return "Align My Time";

    const auto name = projectFile != juce::File() ? projectFile.getFileNameWithoutExtension() : tr ("Unbenannt");
    return name + (hasUnsavedChanges() ? "*" : "") + utf8 (" – Align My Time");
}

bool AppController::hasUnsavedChanges() const
{
    // An untitled session without audio and markers has nothing worth saving.
    const auto& session = processor.getSession();
    if (projectFile == juce::File() && ! processor.isUsingAudioFile() && session.getExtraTracks().empty() && session.getMarkers().empty())
        return false;
    return project::fingerprint (processor) != savedFingerprint;
}

void AppController::markSaved()
{
    savedFingerprint = project::fingerprint (processor);
    timerCallback();
}

void AppController::setProjectFile (const juce::File& file)
{
    projectFile = file;
    appSettings().setValue ("lastProject", file.getFullPathName());
    if (file != juce::File())
    {
        recentFiles.addFile (file);
        appSettings().setValue ("recentProjects", recentFiles.toString());
    }
    appSettings().saveIfNeeded();
    menuItemsChanged();
}

void AppController::timerCallback()
{
    if (getLanguage() != lastLanguage)
    {
        lastLanguage = getLanguage();
        menuItemsChanged();
    }

    const auto title = getWindowTitle();
    if (title != lastTitle)
    {
        lastTitle = title;
        if (onTitleChanged)
            onTitleChanged();
    }
}

//==============================================================================
juce::String AppController::openProjectFile (const juce::File& file)
{
    const auto result = project::load (processor, file);
    if (result.error.isNotEmpty())
        return result.error;

    setProjectFile (file);
    markSaved();

    if (! result.missingFiles.isEmpty())
        showMessage (tr ("Audiodateien nicht gefunden"),
                     tr ("Diese Dateien wurden weder am gespeicherten Ort noch neben dem Projekt gefunden:") + "\n\n"
                         + result.missingFiles.joinIntoString ("\n") + "\n\n"
                         + tr ("Marker und Einstellungen sind geladen. Lade die Audiodatei neu (Datei › Audiodatei laden …) und speichere das Projekt."),
                     true);
    return {};
}

juce::String AppController::saveProjectFile (const juce::File& file)
{
    const auto error = project::save (processor, file);
    if (error.isEmpty())
    {
        setProjectFile (file);
        markSaved();
    }
    return error;
}

void AppController::restoreLastProject()
{
    const juce::File last (appSettings().getValue ("lastProject"));
    if (juce::File::isAbsolutePath (last.getFullPathName()) && last.existsAsFile() && openProjectFile (last).isEmpty())
        return;

    // Otherwise the app shows the session from last time (untitled).
    projectFile = juce::File();
    savedFingerprint.clear();
    timerCallback();
}

void AppController::requestQuit()
{
    whenChangesHandled ([this] {
        if (onQuitConfirmed)
            onQuitConfirmed();
    });
}

void AppController::whenChangesHandled (std::function<void()> action)
{
    if (! hasUnsavedChanges())
    {
        action();
        return;
    }

    const auto name = projectFile != juce::File() ? projectFile.getFileNameWithoutExtension() : tr ("Unbenannt");
    messageBox = juce::AlertWindow::showScopedAsync (juce::MessageBoxOptions()
                                                         .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                                         .withTitle (tr ("Änderungen speichern?"))
                                                         .withMessage (tr ("Das Projekt") + utf8 (" „") + name + utf8 ("“ ")
                                                                       + tr ("wurde geändert. Sollen die Änderungen gespeichert werden?"))
                                                         .withButton (tr ("Speichern"))
                                                         .withButton (tr ("Nicht speichern"))
                                                         .withButton (tr ("Abbrechen"))
                                                         .withAssociatedComponent (getEditor()),
                                                     [this, action] (int result) {
                                                         if (result == 1)
                                                             save ([action] (bool saved) {
                                                                 if (saved)
                                                                     action();
                                                             });
                                                         else if (result == 2)
                                                             action();
                                                     });
}

void AppController::save (std::function<void (bool)> then)
{
    if (projectFile == juce::File())
    {
        saveAs (std::move (then));
        return;
    }

    const auto error = saveProjectFile (projectFile);
    if (error.isNotEmpty())
        showMessage (tr ("Speichern fehlgeschlagen"), error, true);
    if (then)
        then (error.isEmpty());
}

void AppController::saveAs (std::function<void (bool)> then)
{
    auto initial = projectFile;
    if (initial == juce::File())
    {
        defaultProjectFolder().createDirectory();
        const auto name = processor.isUsingAudioFile() ? processor.getAudioFile().getFileNameWithoutExtension() : juce::String ("Align My Time");
        initial = defaultProjectFolder().getChildFile (juce::File::createLegalFileName (name) + project::fileExtension);
    }

    chooser = std::make_unique<juce::FileChooser> (tr ("Projekt speichern"), initial, project::wildcard());
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, then] (const juce::FileChooser& fc) {
                              const auto chosen = fc.getResult();
                              if (chosen == juce::File())
                              {
                                  if (then)
                                      then (false);
                                  return;
                              }
                              const auto error = saveProjectFile (chosen.withFileExtension (project::fileExtension));
                              if (error.isNotEmpty())
                                  showMessage (tr ("Speichern fehlgeschlagen"), error, true);
                              if (then)
                                  then (error.isEmpty());
                          });
}

void AppController::chooseProjectToOpen()
{
    const auto folder = projectFile != juce::File() ? projectFile.getParentDirectory() : defaultProjectFolder();
    chooser = std::make_unique<juce::FileChooser> (tr ("Projekt öffnen"), folder, project::wildcard());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc) {
        if (fc.getResult().existsAsFile())
            if (const auto error = openProjectFile (fc.getResult()); error.isNotEmpty())
                showMessage (tr ("Öffnen fehlgeschlagen"), error, true);
    });
}

void AppController::chooseAudioFiles (bool extraTracks)
{
    chooser = std::make_unique<juce::FileChooser> (extraTracks ? tr ("Weitere Spuren laden") : tr ("Audiodatei laden"), juce::File(),
                                                   AlignMyTimeProcessor::audioFileWildcard());
    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    if (extraTracks)
        flags |= juce::FileBrowserComponent::canSelectMultipleItems;

    chooser->launchAsync (flags, [this, extraTracks] (const juce::FileChooser& fc) {
        if (fc.getResults().isEmpty())
            return;
        processor.getSession().setStep (Step::tap);
        if (extraTracks)
            processor.addExtraFiles (fc.getResults());
        else
            processor.loadAudioFile (fc.getResult());
    });
}

void AppController::showMessage (const juce::String& title, const juce::String& message, bool warning)
{
    messageBox = juce::AlertWindow::showScopedAsync (juce::MessageBoxOptions()
                                                         .withIconType (warning ? juce::MessageBoxIconType::WarningIcon : juce::MessageBoxIconType::InfoIcon)
                                                         .withTitle (title)
                                                         .withMessage (message)
                                                         .withButton ("OK")
                                                         .withAssociatedComponent (getEditor()),
                                                     nullptr);
}

//==============================================================================
juce::StringArray AppController::getMenuBarNames()
{
    return { tr ("Datei"), tr ("Bearbeiten"), tr ("Ansicht"), tr ("Hilfe") };
}

juce::String AppController::menuText (juce::CommandID id) const
{
    switch (id)
    {
        case newProject:      return tr ("Neues Projekt");
        case openProject:     return tr ("Projekt öffnen …");
        case saveProject:     return tr ("Projekt speichern");
        case saveProjectAs:   return tr ("Projekt speichern unter …");
        case loadAudio:       return tr ("Audiodatei laden …");
        case addTracks:       return tr ("Weitere Spuren hinzufügen …");
        case exportResult:    return tr ("Ergebnis exportieren …");
        case quitApp:         return tr ("Beenden");
        case undo:            return tr ("Rückgängig");
        case redo:            return tr ("Wiederherstellen");
        case clearMarkers:    return tr ("Alle Marker löschen");
        case targetTempo:     return tr ("Ziel-Tempo und Taktart …");
        case settings:        return tr ("Einstellungen …");
        case audioSettings:   return tr ("Audio- und MIDI-Einstellungen …");
        case stepTap:         return tr ("Schritt 1: Tappen");
        case stepReview:      return tr ("Schritt 2: Prüfen");
        case stepRender:      return tr ("Schritt 3: Rendern");
        case zoomIn:          return tr ("Hineinzoomen");
        case zoomOut:         return tr ("Herauszoomen");
        case zoomFit:         return tr ("Ganze Aufnahme zeigen");
        case languageGerman:  return "Deutsch";
        case languageEnglish: return "English";
        case manual:          return tr ("Handbuch");
        case shortcuts:       return tr ("Tastenkürzel");
        case credits:         return tr ("Credits");
        default:              return {};
    }
}

juce::PopupMenu AppController::getMenuForIndex (int index, const juce::String&)
{
    juce::PopupMenu menu;
    auto add = [&] (juce::CommandID id) { menu.addCommandItem (&commands, id, menuText (id)); };

    if (index == 0)
    {
        add (newProject);
        add (openProject);

        juce::PopupMenu recent;
        recentFiles.createPopupMenuItems (recent, recentFileBaseId, false, true);
        if (recent.getNumItems() > 0)
        {
            recent.addSeparator();
            recent.addItem (clearRecentId, tr ("Liste leeren"));
        }
        menu.addSubMenu (tr ("Zuletzt geöffnet"), recent, recent.getNumItems() > 0);
        menu.addSeparator();
        add (saveProject);
        add (saveProjectAs);
        menu.addSeparator();
        add (loadAudio);
        add (addTracks);
        add (exportResult);
       #if ! JUCE_MAC
        menu.addSeparator();
        add (quitApp);
       #endif
    }
    else if (index == 1)
    {
        add (undo);
        add (redo);
        menu.addSeparator();
        add (clearMarkers);
        add (targetTempo);
        menu.addSeparator();
        add (audioSettings);
        add (settings);
    }
    else if (index == 2)
    {
        add (stepTap);
        add (stepReview);
        add (stepRender);
        menu.addSeparator();
        add (zoomIn);
        add (zoomOut);
        add (zoomFit);
        menu.addSeparator();
        juce::PopupMenu languages;
        languages.addCommandItem (&commands, languageGerman, menuText (languageGerman));
        languages.addCommandItem (&commands, languageEnglish, menuText (languageEnglish));
        menu.addSubMenu (tr ("Sprache"), languages);
    }
    else if (index == 3)
    {
        add (manual);
        add (shortcuts);
        menu.addSeparator();
        add (credits);
    }
    return menu;
}

void AppController::menuItemSelected (int itemID, int)
{
    if (itemID == clearRecentId)
    {
        recentFiles.clear();
        appSettings().setValue ("recentProjects", recentFiles.toString());
        menuItemsChanged();
        return;
    }

    const auto file = recentFiles.getFile (itemID - recentFileBaseId);
    if (itemID < recentFileBaseId || file == juce::File())
        return; // commands are invoked by the command manager

    whenChangesHandled ([this, file] {
        if (const auto error = openProjectFile (file); error.isNotEmpty())
        {
            if (! file.existsAsFile())
            {
                recentFiles.removeFile (file);
                appSettings().setValue ("recentProjects", recentFiles.toString());
                menuItemsChanged();
            }
            showMessage (tr ("Öffnen fehlgeschlagen"), file.existsAsFile() ? error : tr ("Die Datei gibt es nicht mehr:") + " " + file.getFullPathName(), true);
        }
    });
}

//==============================================================================
void AppController::getAllCommands (juce::Array<juce::CommandID>& ids)
{
    ids.addArray ({ newProject, openProject, saveProject, saveProjectAs, loadAudio, addTracks, exportResult, quitApp,
                    undo, redo, clearMarkers, targetTempo, settings, audioSettings,
                    stepTap, stepReview, stepRender, zoomIn, zoomOut, zoomFit, languageGerman, languageEnglish,
                    manual, shortcuts, credits });
}

void AppController::getCommandInfo (juce::CommandID id, juce::ApplicationCommandInfo& info)
{
    // The names shown in the menus come from menuText() (current language); this one is internal.
    info.setInfo (menuText (id).isNotEmpty() ? menuText (id) : juce::String (id), {}, "Align My Time", 0);

    const auto& session = processor.getSession();
    auto key = [&] (int code, int mods = cmd) { info.addDefaultKeypress (code, juce::ModifierKeys (mods)); };

    switch (id)
    {
        case newProject:     key ('n'); break;
        case openProject:    key ('o'); break;
        case saveProject:    key ('s'); break;
        case saveProjectAs:  key ('s', cmd | shift); break;
        case loadAudio:      key ('i'); break;
        case addTracks:      key ('i', cmd | shift); break;
        case exportResult:   key ('e'); info.setActive (session.canAlign()); break;
        case quitApp:        key ('q'); break;
        case undo:           key ('z'); info.setActive (session.canUndo()); break;
        case redo:
            key ('y');
            key ('z', cmd | shift);
            info.setActive (session.canRedo());
            break;
        case clearMarkers:   info.setActive (! session.getMarkers().empty()); break;
        case targetTempo:    key ('t'); break;
        case settings:       key (','); break;
        case audioSettings:  info.setActive (processor.getDeviceManager() != nullptr); break;
        case stepTap:        key ('1'); info.setTicked (session.getStep() == Step::tap); break;
        case stepReview:
            key ('2');
            info.setActive (session.getMarkers().size() >= 2);
            info.setTicked (session.getStep() == Step::review);
            break;
        case stepRender:
            key ('3');
            info.setActive (session.canAlign());
            info.setTicked (session.getStep() == Step::render);
            break;
        case zoomIn:
            key ('+');
            key ('=');
            info.setActive (session.getStep() == Step::review);
            break;
        case zoomOut:        key ('-'); info.setActive (session.getStep() == Step::review); break;
        case zoomFit:        key ('0'); info.setActive (session.getStep() == Step::review); break;
        case languageGerman: info.setTicked (getLanguage() == Language::german); break;
        case languageEnglish: info.setTicked (getLanguage() == Language::english); break;
        case manual:         key (juce::KeyPress::F1Key, 0); break;
        default: break;
    }
}

bool AppController::perform (const InvocationInfo& info)
{
    auto* editor = getEditor();
    auto& session = processor.getSession();
    auto goTo = [&] (Step step) {
        processor.stopPreview();
        session.setStep (step);
    };

    switch (info.commandID)
    {
        case newProject:
            whenChangesHandled ([this] {
                processor.newProject();
                setProjectFile ({});
                savedFingerprint.clear();
                timerCallback();
            });
            return true;
        case openProject:    whenChangesHandled ([this] { chooseProjectToOpen(); }); return true;
        case saveProject:    save (nullptr); return true;
        case saveProjectAs:  saveAs (nullptr); return true;
        case loadAudio:      chooseAudioFiles (false); return true;
        case addTracks:      chooseAudioFiles (true); return true;
        case exportResult:
            if (editor != nullptr && session.canAlign())
                editor->exportResult();
            return true;
        case quitApp:        requestQuit(); return true;
        case undo:           session.undo(); return true;
        case redo:           session.redo(); return true;
        case clearMarkers:   session.clearMarkers(); return true;
        case targetTempo:
            if (editor != nullptr)
                editor->openTempoEditor();
            return true;
        case settings:
        case audioSettings:
        case credits:
            if (editor != nullptr)
                editor->showSettings (info.commandID == audioSettings ? ui::SettingsPanel::Section::audio
                                      : info.commandID == credits     ? ui::SettingsPanel::Section::credits
                                                                      : ui::SettingsPanel::Section::general);
            return true;
        case stepTap:        goTo (Step::tap); return true;
        case stepReview:
            if (session.getMarkers().size() >= 2)
                goTo (Step::review);
            return true;
        case stepRender:
            if (session.canAlign())
                goTo (Step::render);
            return true;
        case zoomIn:
        case zoomOut:
        case zoomFit:
            if (editor != nullptr)
                editor->zoomReview (info.commandID == zoomIn ? 1 : info.commandID == zoomOut ? -1 : 0);
            return true;
        case languageGerman:
        case languageEnglish:
            setLanguage (info.commandID == languageEnglish ? Language::english : Language::german);
            if (editor != nullptr)
                editor->rebuildUi();
            timerCallback();
            return true;
        case manual:
        case shortcuts:
            if (editor != nullptr)
                editor->showManual (info.commandID == shortcuts ? "shortcuts" : juce::String());
            return true;
        default:
            return false;
    }
}

} // namespace amt::plugin
