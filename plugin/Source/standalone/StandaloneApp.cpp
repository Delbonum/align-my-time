// The standalone app (replaces JUCE's default standalone window): a normal application window
// with a menu bar, project files and the audio/MIDI settings inside the app's own settings.
// Built only into the Standalone target (JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1).

#include "AppController.h"
#include "../PluginEditor.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(); // PluginProcessor.cpp

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace amt::plugin
{

class MainWindow : public juce::DocumentWindow
{
public:
    explicit MainWindow (std::unique_ptr<juce::StandalonePluginHolder> pluginHolder)
        : juce::DocumentWindow ("Align My Time", ui::colours::panel, juce::DocumentWindow::allButtons),
          holder (std::move (pluginHolder))
    {
        auto& processor = dynamic_cast<AlignMyTimeProcessor&> (*holder->processor);
        processor.setDeviceManager (&holder->deviceManager);
        controller = std::make_unique<AppController> (processor);

        editor.reset (processor.createEditorIfNeeded());
        if (auto* ed = dynamic_cast<AlignMyTimeEditor*> (editor.get()))
            ed->onProjectFileDropped = [this] (const juce::File& file) { openFile (file); };
        const int startWidth = editor->getWidth(), startHeight = editor->getHeight();

        setUsingNativeTitleBar (true);
       #if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu (controller.get());
       #else
        setMenuBar (controller.get());
       #endif
        setContentNonOwned (editor.get(), true);
        addKeyListener (controller->getCommandManager().getKeyMappings());

        // Resizable; the editor scales its content. The last size is kept, but never larger than the screen.
        constexpr int designWidth = AlignMyTimeEditor::designWidth, designHeight = AlignMyTimeEditor::designHeight;
        const int chromeHeight = getHeight() - startHeight;
        setResizable (true, false);
        setResizeLimits (designWidth / 2, designHeight / 2 + chromeHeight, designWidth * 2, designHeight * 2 + chromeHeight);
        if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            const auto area = display->userArea.reduced (20);
            const float scale = juce::jmin ((float) startWidth / (float) designWidth, (float) area.getWidth() / (float) designWidth,
                                            (float) (area.getHeight() - chromeHeight) / (float) designHeight);
            centreWithSize (juce::roundToInt ((float) designWidth * scale), juce::roundToInt ((float) designHeight * scale) + chromeHeight);
        }

        controller->onTitleChanged = [this] { setName (controller->getWindowTitle()); };
        controller->onQuitConfirmed = [this] {
            holder->savePluginState();
            juce::JUCEApplication::getInstance()->quit();
        };
    }

    ~MainWindow() override
    {
       #if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu (nullptr);
       #else
        setMenuBar (nullptr);
       #endif
        removeKeyListener (controller->getCommandManager().getKeyMappings());
        clearContentComponent();
        editor.reset();
        controller.reset();
        holder.reset();
    }

    AppController& getController() { return *controller; }

    void focusEditor()
    {
        if (editor != nullptr && editor->isShowing())
            editor->grabKeyboardFocus();
    }

    void openFile (const juce::File& file)
    {
        if (file.hasFileExtension (".amtp"))
        {
            if (const auto error = controller->openProjectFile (file); error.isNotEmpty())
                juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                                  .withIconType (juce::MessageBoxIconType::WarningIcon)
                                                  .withTitle (tr ("Öffnen fehlgeschlagen"))
                                                  .withMessage (error)
                                                  .withButton ("OK")
                                                  .withAssociatedComponent (editor.get()),
                                              nullptr);
        }
        else if (file.existsAsFile())
        {
            dynamic_cast<AlignMyTimeProcessor&> (*holder->processor).loadAudioFile (file);
        }
    }

    void closeButtonPressed() override { controller->requestQuit(); }

private:
    std::unique_ptr<juce::StandalonePluginHolder> holder;
    std::unique_ptr<AppController> controller;
    std::unique_ptr<juce::AudioProcessorEditor> editor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};

//==============================================================================
class StandaloneApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return JucePlugin_Name; }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String& commandLine) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        applyJuceTranslations();

        // Output only: the app works with audio files, so no input (and no feedback warning).
        juce::Array<juce::StandalonePluginHolder::PluginInOuts> channels;
        channels.add ({ 0, 2 });
        // Audio setup and window position go into appSettings(): a second PropertiesFile on the same
        // file would write back its stale copy and undo changed settings (e.g. the tap key).
        window = std::make_unique<MainWindow> (std::make_unique<juce::StandalonePluginHolder> (&appSettings(), false,
                                                                                                juce::String(), nullptr, channels, false));
        window->getController().restoreLastProject();
        window->setVisible (true);
        window->focusEditor();

        const auto file = commandLineFile (commandLine);
        if (file.existsAsFile())
            window->openFile (file);
    }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        if (window != nullptr)
            if (const auto file = commandLineFile (commandLine); file.existsAsFile())
                window->openFile (file);
    }

    void shutdown() override
    {
        window.reset();
        appSettings().saveIfNeeded();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override
    {
        if (window != nullptr)
            window->getController().requestQuit();
        else
            quit();
    }

private:
    static juce::File commandLineFile (const juce::String& commandLine)
    {
        const auto path = commandLine.trim().unquoted();
        return juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();
    }

    ui::AmtLookAndFeel lookAndFeel;
    std::unique_ptr<MainWindow> window;
};

} // namespace amt::plugin

juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication()
{
    return new amt::plugin::StandaloneApp();
}
