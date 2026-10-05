#include "LookAndFeel.h"

#include <map>

namespace amt::plugin::ui
{

juce::Font uiFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font monoFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, bold ? juce::Font::bold : juce::Font::plain));
}

//==============================================================================
AmtLookAndFeel::AmtLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::panel);
    setColour (juce::TextButton::buttonColourId, colours::panel2);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, colours::text);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextEditor::backgroundColourId, colours::background);
    setColour (juce::TextEditor::outlineColourId, colours::border);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::accent);
    setColour (juce::CaretComponent::caretColourId, colours::accent);
    setColour (juce::Slider::thumbColourId, colours::accent);
    setColour (juce::Slider::trackColourId, colours::accent);
    setColour (juce::Slider::backgroundColourId, colours::panel2);
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, colours::border);
    setColour (juce::Slider::textBoxBackgroundColourId, colours::background);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colours::white);
    setColour (juce::TooltipWindow::textColourId, colours::ink);
    setColour (juce::ScrollBar::thumbColourId, colours::muted.withAlpha (0.6f));
    setColour (juce::ScrollBar::trackColourId, colours::background);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel2);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::ink);
}

void setKind (juce::TextButton& button, ButtonKind kind)
{
    button.getProperties().set ("kind", (int) kind);
    button.repaint();
}

static ButtonKind kindOf (const juce::Button& button)
{
    return (ButtonKind) (int) button.getProperties().getWithDefault ("kind", (int) ButtonKind::ghost);
}

void AmtLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&, bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const auto kind = kindOf (button);
    const bool segment = button.getProperties().contains ("segment");

    if (button.getProperties().contains ("nav"))
    {
        if (button.getToggleState() || highlighted)
        {
            g.setColour (button.getToggleState() ? colours::panel2 : colours::panel2.withAlpha (0.5f));
            g.fillRoundedRectangle (bounds, 8.0f);
        }
        if (button.getToggleState())
        {
            g.setColour (colours::accent);
            g.fillRoundedRectangle (bounds.withWidth (4.0f).reduced (0.0f, 8.0f), 2.0f);
        }
        return;
    }

    if (segment)
    {
        if (button.getToggleState())
        {
            g.setColour (colours::panel2);
            g.fillRoundedRectangle (bounds, 6.0f);
            g.setColour (colours::border);
            g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
        }
        else if (highlighted)
        {
            g.setColour (colours::panel2.withAlpha (0.5f));
            g.fillRoundedRectangle (bounds, 6.0f);
        }
        return;
    }

    auto fill = kind == ButtonKind::primary ? colours::accent : kind == ButtonKind::solid ? colours::panel2 : juce::Colours::transparentBlack;
    if (! button.isEnabled())
        fill = fill.withMultipliedAlpha (0.4f);
    else if (down)
        fill = kind == ButtonKind::primary ? fill.darker (0.2f) : colours::border;
    else if (highlighted)
        fill = kind == ButtonKind::primary ? fill.brighter (0.15f) : colours::panel2.brighter (0.05f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 8.0f);

    if (kind != ButtonKind::primary)
    {
        g.setColour (colours::border);
        g.drawRoundedRectangle (bounds, 8.0f, 1.0f);
    }

    if (button.hasKeyboardFocus (false))
    {
        g.setColour (colours::accent.withAlpha (0.6f));
        g.drawRoundedRectangle (bounds.expanded (1.0f), 9.0f, 1.5f);
    }
}

juce::Font AmtLookAndFeel::getTextButtonFont (juce::TextButton& button, int)
{
    return uiFont (button.getProperties().contains ("segment") ? 12.0f : 13.5f, true);
}

void AmtLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    const auto kind = kindOf (button);
    const bool segment = button.getProperties().contains ("segment");
    if (button.getProperties().contains ("nav"))
    {
        g.setColour (button.getToggleState() ? colours::text : colours::muted);
        g.setFont (uiFont (14.0f, true));
        g.drawText (button.getButtonText(), button.getLocalBounds().withTrimmedLeft (18), juce::Justification::centredLeft);
        return;
    }

    auto colour = kind == ButtonKind::primary ? colours::ink : colours::text;
    if (segment && ! button.getToggleState())
        colour = colours::muted;
    if (! button.isEnabled())
        colour = colour.withMultipliedAlpha (0.45f);

    g.setColour (colour);
    g.setFont (getTextButtonFont (button, button.getHeight()));

    const auto icon = button.getProperties()["icon"].toString();
    const bool iconAfter = button.getProperties().contains ("iconAfter");
    auto area = button.getLocalBounds().reduced (segment ? 2 : 12, 0);
    const auto text = button.getButtonText();

    if (icon.isEmpty())
    {
        g.drawText (text, area, juce::Justification::centred);
        return;
    }

    const float iconSize = 16.0f;
    const int gap = text.isEmpty() ? 0 : 8;
    const int textWidth = text.isEmpty() ? 0 : juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text);
    const int total = (int) iconSize + gap + textWidth;
    auto content = area.withSizeKeepingCentre (juce::jmin (total, area.getWidth()), area.getHeight());

    auto iconArea = (iconAfter ? content.removeFromRight ((int) iconSize) : content.removeFromLeft ((int) iconSize)).toFloat();
    drawIcon (g, icon, iconArea.withSizeKeepingCentre (iconSize, iconSize), colour);

    if (text.isNotEmpty())
    {
        iconAfter ? content.removeFromRight (gap) : content.removeFromLeft (gap);
        g.drawText (text, content, juce::Justification::centred);
    }
}

void AmtLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    auto bounds = button.getLocalBounds();
    const auto switchArea = bounds.removeFromRight (36).withSizeKeepingCentre (36, 20).toFloat();
    const bool on = button.getToggleState();

    g.setColour (on ? colours::accent : colours::panel2);
    g.fillRoundedRectangle (switchArea, 10.0f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (switchArea, 10.0f, 1.0f);

    const auto knob = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ on ? switchArea.getRight() - 10.0f : switchArea.getX() + 10.0f, switchArea.getCentreY() });
    g.setColour (juce::Colours::white);
    g.fillEllipse (knob);

    g.setColour (button.isEnabled() ? (highlighted ? colours::text.brighter() : colours::text) : colours::muted);
    g.setFont (uiFont (13.5f));
    g.drawFittedText (button.getButtonText(), bounds.withTrimmedRight (10), juce::Justification::centredLeft, 2);

    if (button.hasKeyboardFocus (false))
    {
        g.setColour (colours::accent.withAlpha (0.6f));
        g.drawRoundedRectangle (switchArea.expanded (2.0f), 12.0f, 1.5f);
    }
}

void AmtLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                       juce::Slider::SliderStyle, juce::Slider&)
{
    const auto track = juce::Rectangle<float> ((float) x, (float) y + h * 0.5f - 2.0f, (float) w, 4.0f);
    g.setColour (colours::panel2);
    g.fillRoundedRectangle (track, 2.0f);
    g.setColour (colours::accent);
    g.fillRoundedRectangle (track.withRight (pos), 2.0f);
    g.setColour (juce::Colours::white);
    g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ pos, track.getCentreY() }));
}

void AmtLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (colours::background);
    g.fillRoundedRectangle (juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h), 8.0f);
}

void AmtLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& editor)
{
    g.setColour (editor.hasKeyboardFocus (true) ? colours::accent : colours::border);
    g.drawRoundedRectangle (juce::Rectangle<float> (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f), 8.0f, 1.0f);
}

//==============================================================================
void drawSectionLabel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text)
{
    g.setColour (colours::muted);
    g.setFont (uiFont (11.0f, true).withExtraKerningFactor (0.08f));
    // JUCE's toUpperCase leaves German umlauts alone.
    const auto upper = text.toUpperCase().replace (utf8 ("ä"), utf8 ("Ä")).replace (utf8 ("ö"), utf8 ("Ö")).replace (utf8 ("ü"), utf8 ("Ü"));
    g.drawText (upper, area, juce::Justification::centredLeft);
}

//==============================================================================
SegmentedControl::SegmentedControl (juce::StringArray options)
{
    for (int i = 0; i < options.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (options[i]));
        b->getProperties().set ("segment", true);
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { setSelected (i, juce::sendNotification); };
        addAndMakeVisible (b);
    }
    setSelected (0);
}

void SegmentedControl::setSelected (int index, juce::NotificationType notification)
{
    selected = juce::jlimit (0, buttons.size() - 1, index);
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == selected, juce::dontSendNotification);

    if (notification != juce::dontSendNotification && onChange)
        onChange (selected);
}

void SegmentedControl::resized()
{
    auto area = getLocalBounds().reduced (3);
    const int columns = numColumns > 0 ? numColumns : buttons.size();
    const int rows = getNumRows();
    const int w = area.getWidth() / juce::jmax (1, columns);
    const int h = area.getHeight() / juce::jmax (1, rows);
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setBounds (juce::Rectangle<int> (area.getX() + (i % juce::jmax (1, columns)) * w,
                                                     area.getY() + (i / juce::jmax (1, columns)) * h, w, h)
                                   .reduced (1, 1));
}

void SegmentedControl::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::background);
    g.fillRoundedRectangle (bounds, 9.0f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (bounds, 9.0f, 1.0f);
}

//==============================================================================
ChoiceCard::ChoiceCard (const juce::String& t, const juce::String& d) : juce::Button (t), title (t), description (d)
{
    setClickingTogglesState (false);
}

juce::Rectangle<int> ChoiceCard::getExtraArea() const
{
    return getLocalBounds().reduced (14).removeFromBottom (34);
}

void ChoiceCard::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const bool on = getToggleState();
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (on ? colours::accentCard : colours::background);
    g.fillRoundedRectangle (bounds, 12.0f);
    g.setColour (on ? colours::accent : (highlighted ? colours::muted : colours::border));
    g.drawRoundedRectangle (bounds, 12.0f, on ? 2.0f : 1.0f);

    auto area = getLocalBounds().reduced (14);
    auto titleRow = area.removeFromTop (20);

    const auto dot = titleRow.removeFromLeft (16).withSizeKeepingCentre (16, 16).toFloat();
    g.setColour (on ? colours::accent : colours::muted);
    g.drawEllipse (dot.reduced (1.0f), 2.0f);
    if (on)
        g.fillEllipse (dot.reduced (4.0f));

    titleRow.removeFromLeft (10);
    const auto icon = getProperties()["icon"].toString();
    if (icon.isNotEmpty())
    {
        drawIcon (g, icon, titleRow.removeFromLeft (18).toFloat().withSizeKeepingCentre (18.0f, 18.0f), colours::text);
        titleRow.removeFromLeft (8);
    }

    g.setColour (colours::text);
    g.setFont (uiFont (14.5f, true));
    g.drawText (title, titleRow, juce::Justification::centredLeft);

    area.removeFromTop (8);
    g.setColour (colours::muted);
    g.setFont (uiFont (12.5f));
    g.drawFittedText (description, area.removeFromTop (36), juce::Justification::topLeft, 2, 1.0f);

    if (hasKeyboardFocus (false))
    {
        g.setColour (colours::accent.withAlpha (0.6f));
        g.drawRoundedRectangle (bounds.expanded (1.0f), 13.0f, 1.5f);
    }
}

//==============================================================================
juce::Path makeIcon (const juce::String& name)
{
    static const std::map<juce::String, const char*> paths {
        { "play", "M7 5L19 12L7 19Z" },
        { "stop", "M7.5 6H16.5Q18 6 18 7.5V16.5Q18 18 16.5 18H7.5Q6 18 6 16.5V7.5Q6 6 7.5 6Z" },
        { "rewind", "M6 5V19M19 5L9 12L19 19Z" },
        { "arrow-r", "M5 12H19M13 6L19 12L13 18" },
        { "arrow-l", "M19 12H5M11 6L5 12L11 18" },
        { "undo", "M9 14L4 9L9 4M4 9H14A6 6 0 0 1 14 21H11" },
        { "trash", "M4 7H20M10 11V17M14 11V17M6 7L7 20H17L18 7M9 7V4H15V7" },
        { "magnet", "M6 4V12A6 6 0 0 0 18 12V4M6 8H10M14 8H18" },
        { "plus", "M12 5V19M5 12H19" },
        { "minus", "M5 12H19" },
        { "link", "M10 14A4 4 0 0 0 16 14L19 11A4 4 0 0 0 13 5L12 6M14 10A4 4 0 0 0 8 10L5 13A4 4 0 0 0 11 19L12 18" },
        { "refresh", "M20 11A8 8 0 1 0 17.7 16.7M20 4V11H13" },
        { "headphones", "M4 15V12A8 8 0 0 1 20 12V15M3 14H8V21H3ZM16 14H21V21H16Z" },
        { "check", "M5 12.5L9.5 17L19 7.5" },
        { "stretch", "M3 12H21M7 8L3 12L7 16M17 8L21 12L17 16" },
        { "scissors", "M9 7A3 3 0 1 1 3 7A3 3 0 1 1 9 7M9 17A3 3 0 1 1 3 17A3 3 0 1 1 9 17M8.5 8.5L20 18M8.5 15.5L20 6" },
        { "nudge-l", "M15 6L9 12L15 18" },
        { "nudge-r", "M9 6L15 12L9 18" },
        { "track", "M5 5H19Q21 5 21 7V17Q21 19 19 19H5Q3 19 3 17V7Q3 5 5 5ZM7 12H8M10 9V15M13 10V14M16 8V16" },
        { "info", "M21 12A9 9 0 1 1 3 12A9 9 0 1 1 21 12M12 11V17M12 7.5V8" },
        { "folder", "M3 6Q3 5 4 5H9L11 7H20Q21 7 21 8V18Q21 19 20 19H4Q3 19 3 18Z" },
        { "drag", "M12 3V15M7 10L12 15L17 10M5 19H19" },
        { "record", "M18 12A6 6 0 1 1 6 12A6 6 0 1 1 18 12" },

        { "file", "M6 3H14L19 8V21H6ZM14 3V8H19" },
    };

    if (name == "gear")
    {
        // A cog: 8 teeth around a ring with a hole, as a single outline.
        juce::Path cog;
        constexpr int teeth = 8;
        const float outer = 10.5f, inner = 8.0f;
        for (int i = 0; i < teeth * 4; ++i)
        {
            const float angle = juce::MathConstants<float>::twoPi * (float) i / (float) (teeth * 4) - juce::MathConstants<float>::halfPi;
            const float r = (i % 4 == 0 || i % 4 == 1) ? outer : inner;
            const juce::Point<float> p (12.0f + r * std::cos (angle), 12.0f + r * std::sin (angle));
            if (i == 0)
                cog.startNewSubPath (p);
            else
                cog.lineTo (p);
        }
        cog.closeSubPath();
        cog.addEllipse (8.5f, 8.5f, 7.0f, 7.0f);
        return cog;
    }

    auto it = paths.find (name);
    return it != paths.end() ? juce::Drawable::parseSVGPath (it->second) : juce::Path();
}

void drawIcon (juce::Graphics& g, const juce::String& name, juce::Rectangle<float> area, juce::Colour colour, float thickness)
{
    auto path = makeIcon (name);
    if (path.isEmpty())
        return;

    path.applyTransform (juce::AffineTransform::scale (area.getWidth() / 24.0f, area.getHeight() / 24.0f).translated (area.getX(), area.getY()));
    g.setColour (colour);

    if (name == "play" || name == "record")
        g.fillPath (path);
    g.strokePath (path, juce::PathStrokeType (thickness * area.getWidth() / 18.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

} // namespace amt::plugin::ui
