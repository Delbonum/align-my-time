#pragma once

#include "Page.h"

namespace amt::plugin::ui
{

/** Step 3: before/after on the project grid, choose the destination, render. */
class RenderPage : public Page
{
public:
    explicit RenderPage (AlignMyTimeProcessor& p);
    ~RenderPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh() override;
    bool handleKey (const juce::KeyPress&) override;
    void sessionChanged() override;

    /** Renders if needed, then writes the file(s) (standalone: asks where). */
    void exportResult();

private:
    class DragTile;

    void render();
    void finishPendingAction();
    /** Writes one file per track: `firstFile` for a single track, the others named after it. */
    void exportTo (const juce::File& firstFile);
    /** The standalone app has no track to replace: the result is always a file. */
    Destination destination() const;
    std::vector<double> projectBarLines (double start, double end) const;

    WaveformView before, after;
    ChoiceCard newTrackCard { tr ("Als neue Spur"), tr ("Schreibt eine WAV-Datei. Zieh sie auf eine neue Spur – mit „ab Projektanfang“ einfach an Takt 1.") };
    ChoiceCard replaceCard { tr ("In dieser Spur ersetzen"), tr ("Die Spur spielt ab sofort die angepasste Version. Nicht-destruktiv, jederzeit zurückschaltbar.") };
    juce::TextEditor trackName;
    ToggleRow fromProjectStart { tr ("Datei ab Projektanfang (Takt 1)") };
    std::unique_ptr<DragTile> dragTile;
    juce::TextButton showInFolder;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::TextButton back, listen, renderButton;

    juce::Rectangle<int> beforeCaption, afterCaption, leftArea, rightArea, footer, nameLabel;
    bool pendingAction = false;
    juce::Array<juce::File> exportedFiles; ///< one per track
    juce::String errorText, multitrackNote;
};

} // namespace amt::plugin::ui
