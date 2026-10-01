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

private:
    class DragTile;

    void render();
    void finishPendingAction();
    std::vector<double> projectBarLines (double start, double end) const;

    WaveformView before, after;
    ChoiceCard newTrackCard { "Als neue Spur", de ("Schreibt eine WAV-Datei. Zieh sie auf eine neue Spur – mit „ab Projektanfang“ einfach an Takt 1.") };
    ChoiceCard replaceCard { "In dieser Spur ersetzen", de ("Die Spur spielt ab sofort die angepasste Version. Nicht-destruktiv, jederzeit zurückschaltbar.") };
    juce::TextEditor trackName;
    ToggleRow fromProjectStart { "Datei ab Projektanfang (Takt 1)" };
    std::unique_ptr<DragTile> dragTile;
    juce::TextButton showInFolder;

    juce::TextButton back, listen, renderButton;

    juce::Rectangle<int> beforeCaption, afterCaption, leftArea, rightArea, footer, nameLabel;
    bool pendingAction = false;
    juce::File exportedFile;
    juce::String errorText;
};

} // namespace amt::plugin::ui
