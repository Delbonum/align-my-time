#pragma once

#include "../Localisation.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace amt::plugin::ui
{

/** Colours of the design (dark DAW look, amber for markers/taps, cyan for the project grid). */
namespace colours
{
    const juce::Colour background { 0xff121417 };
    const juce::Colour panel { 0xff1b1e22 };
    const juce::Colour panel2 { 0xff23272c };
    const juce::Colour border { 0xff30353c };
    const juce::Colour text { 0xffeceae4 };
    const juce::Colour muted { 0xffa0a7b0 };
    const juce::Colour accent { 0xfff5a524 };
    const juce::Colour accentDark { 0xff2a2213 };
    const juce::Colour accentCard { 0xff241f15 };
    const juce::Colour ink { 0xff1a1300 };
    const juce::Colour grid { 0xff4fc3d9 };
    const juce::Colour gridDark { 0xff15282c };
    const juce::Colour good { 0xff8fd19e };
    const juce::Colour wave { 0xffc9ced6 };
    const juce::Colour waveAligned { 0xffe9d6ae };
}


juce::Font uiFont (float height, bool bold = false);
juce::Font monoFont (float height, bool bold = false);

class AmtLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AmtLookAndFeel();

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float min, float max,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
};

/** Button roles from the design. Set as the "kind" property of a TextButton. */
enum class ButtonKind { ghost, solid, primary };
void setKind (juce::TextButton& button, ButtonKind kind);

/** Small uppercase section label. */
void drawSectionLabel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text);

/** A row of mutually exclusive choices ("Jede Eins | Jede Zählzeit"). */
class SegmentedControl : public juce::Component
{
public:
    explicit SegmentedControl (juce::StringArray options);

    void setSelected (int index, juce::NotificationType notification = juce::dontSendNotification);
    int getSelected() const { return selected; }
    std::function<void (int)> onChange;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    juce::OwnedArray<juce::TextButton> buttons;
    int selected = 0;
};

/** A large selectable card with radio dot, title and description. */
class ChoiceCard : public juce::Button
{
public:
    ChoiceCard (const juce::String& title, const juce::String& description);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

    /** Space at the bottom of the card for extra controls placed by the parent. */
    juce::Rectangle<int> getExtraArea() const;

    void setText (const juce::String& newTitle, const juce::String& newDescription)
    {
        title = newTitle;
        description = newDescription;
        repaint();
    }

private:
    juce::String title, description;
};

/** Switch with a label on the left, as in the design. */
class ToggleRow : public juce::ToggleButton
{
public:
    explicit ToggleRow (const juce::String& label) : juce::ToggleButton (label) {}
};

/** Icons from the design, drawn as strokes. */
juce::Path makeIcon (const juce::String& name);
void drawIcon (juce::Graphics& g, const juce::String& name, juce::Rectangle<float> area, juce::Colour colour, float thickness = 1.8f);

} // namespace amt::plugin::ui
