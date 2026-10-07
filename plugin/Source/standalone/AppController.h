#pragma once

#include "../PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace amt::plugin
{

class AlignMyTimeEditor;

/** The standalone app's menu bar (Datei | Bearbeiten | Ansicht | Hilfe), its keyboard shortcuts
    and the project files (.amtp). The window and the audio devices live in StandaloneApp.cpp;
    this part is in the shared code so the smoke test can drive it. */
class AppController : public juce::MenuBarModel,
                      public juce::ApplicationCommandTarget,
                      private juce::Timer
{
public:
    enum CommandIDs
    {
        newProject = 1,
        openProject,
        saveProject,
        saveProjectAs,
        loadAudio,
        addTracks,
        exportResult,
        quitApp,
        undo,
        redo,
        clearMarkers,
        targetTempo,
        settings,
        audioSettings,
        stepTap,
        stepReview,
        stepRender,
        zoomIn,
        zoomOut,
        zoomFit,
        languageGerman,
        languageEnglish,
        manual,
        shortcuts,
        credits
    };

    explicit AppController (AlignMyTimeProcessor& processor);
    ~AppController() override;

    juce::ApplicationCommandManager& getCommandManager() { return commands; }

    /** "Song – Align My Time", with a "*" while there are unsaved changes. */
    juce::String getWindowTitle() const;
    std::function<void()> onTitleChanged;

    /** Called once quitting is fine (changes saved or discarded). */
    std::function<void()> onQuitConfirmed;

    /** Asks about unsaved changes, then calls onQuitConfirmed. */
    void requestQuit();

    /** Open / save without dialogs; return an error message or an empty string. */
    juce::String openProjectFile (const juce::File& file);
    juce::String saveProjectFile (const juce::File& file);

    bool hasUnsavedChanges() const;
    const juce::File& getProjectFile() const { return projectFile; }

    /** At startup: reopens the project that was open last time, if it still exists. */
    void restoreLastProject();

    //==============================================================================
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int index, const juce::String& name) override;
    void menuItemSelected (int itemID, int topLevelMenuIndex) override;

    juce::ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }
    void getAllCommands (juce::Array<juce::CommandID>& ids) override;
    void getCommandInfo (juce::CommandID id, juce::ApplicationCommandInfo& info) override;
    bool perform (const InvocationInfo& info) override;

private:
    AlignMyTimeEditor* getEditor() const;
    juce::String menuText (juce::CommandID id) const;

    /** Runs `action` once unsaved changes are saved or discarded; nothing if the user cancels. */
    void whenChangesHandled (std::function<void()> action);
    /** Saves (asking for a file if the project has none); `then` gets true if it was saved. */
    void save (std::function<void (bool)> then);
    void saveAs (std::function<void (bool)> then);
    void chooseProjectToOpen();
    void chooseAudioFiles (bool extraTracks);
    void showMessage (const juce::String& title, const juce::String& message, bool warning);

    void setProjectFile (const juce::File& file);
    void markSaved();
    void timerCallback() override;

    AlignMyTimeProcessor& processor;
    juce::ApplicationCommandManager commands;
    juce::RecentlyOpenedFilesList recentFiles;
    juce::File projectFile;
    juce::String savedFingerprint, lastTitle;
    Language lastLanguage;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::ScopedMessageBox messageBox;

    static constexpr int recentFileBaseId = 1000, clearRecentId = 999;

    JUCE_DECLARE_NON_COPYABLE (AppController)
};

} // namespace amt::plugin
