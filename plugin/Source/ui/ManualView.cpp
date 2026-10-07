#include "ManualView.h"

#include <AlignMyTimeAssets.h>

namespace amt::plugin::ui
{

//==============================================================================
std::vector<ManualChapter> parseManual (const juce::String& markdown)
{
    using Block = ManualChapter::Block;
    std::vector<ManualChapter> chapters;
    juce::String paragraph;

    auto add = [&] (Block::Kind kind, const juce::StringArray& cells, int number = 0, bool header = false) {
        if (! chapters.empty())
            chapters.back().blocks.push_back ({ kind, cells, number, header });
    };
    auto flush = [&] {
        if (paragraph.isNotEmpty())
            add (Block::Kind::paragraph, juce::StringArray (paragraph));
        paragraph.clear();
    };

    bool inTable = false;
    for (auto line : juce::StringArray::fromLines (markdown))
    {
        const auto trimmed = line.trim();

        if (trimmed.startsWith ("<!--"))
        {
            // "<!-- id: shortcuts -->" names the chapter (invisible on GitHub); other comments are skipped.
            const auto inner = trimmed.fromFirstOccurrenceOf ("<!--", false, false).upToLastOccurrenceOf ("-->", false, false).trim();
            if (inner.startsWith ("id:") && ! chapters.empty())
                chapters.back().id = inner.fromFirstOccurrenceOf ("id:", false, false).trim();
            continue;
        }

        if (trimmed.isEmpty())
        {
            flush();
            inTable = false;
            continue;
        }

        if (trimmed.startsWith ("## "))
        {
            flush();
            ManualChapter chapter;
            chapter.title = trimmed.substring (3).trim();
            chapters.push_back (std::move (chapter));
            continue;
        }
        if (trimmed.startsWith ("# ") || trimmed.startsWith ("!["))
        {
            flush(); // the document title is the overlay's title; images are only for GitHub
            continue;
        }
        if (trimmed.startsWith ("### "))
        {
            flush();
            add (Block::Kind::subheading, juce::StringArray (trimmed.substring (4).trim()));
            continue;
        }
        if (trimmed.startsWith ("|"))
        {
            flush();
            if (trimmed.containsOnly ("|-: "))
                continue; // separator row
            juce::StringArray cells;
            cells.addTokens (trimmed.substring (1, trimmed.endsWith ("|") ? trimmed.length() - 1 : trimmed.length()), "|", "`");
            cells.trim();
            add (Block::Kind::tableRow, cells, 0, ! inTable);
            inTable = true;
            continue;
        }
        if (trimmed.startsWith ("- ") || trimmed.startsWith ("* "))
        {
            flush();
            add (Block::Kind::bullet, juce::StringArray (trimmed.substring (2).trim()));
            continue;
        }
        if (const auto dot = trimmed.indexOf (". "); dot > 0 && dot <= 3 && trimmed.substring (0, dot).containsOnly ("0123456789"))
        {
            flush();
            add (Block::Kind::step, juce::StringArray (trimmed.substring (dot + 2).trim()), trimmed.substring (0, dot).getIntValue());
            continue;
        }

        // Continuation of a list item (indented) or of a paragraph.
        if (line.startsWith ("  ") && paragraph.isEmpty() && ! chapters.empty() && ! chapters.back().blocks.empty())
        {
            auto& last = chapters.back().blocks.back();
            if (last.kind == Block::Kind::bullet || last.kind == Block::Kind::step)
            {
                last.cells.set (0, last.cells[0] + " " + trimmed);
                continue;
            }
        }
        paragraph << (paragraph.isEmpty() ? "" : " ") << trimmed;
    }
    flush();
    return chapters;
}

std::vector<ManualChapter> loadManual()
{
    const bool german = getLanguage() == Language::german;
    const auto* data = german ? AlignMyTimeAssets::HANDBUCH_md : AlignMyTimeAssets::MANUAL_md;
    const auto size = german ? AlignMyTimeAssets::HANDBUCH_mdSize : AlignMyTimeAssets::MANUAL_mdSize;
    return parseManual (juce::String::fromUTF8 (data, size));
}

namespace
{
    /** **bold**, `key` and [link](target) inside a line of text. */
    juce::AttributedString styled (const juce::String& text, float size, juce::Colour colour, bool bold = false)
    {
        juce::AttributedString result;
        result.setWordWrap (juce::AttributedString::byWord);
        result.setLineSpacing (size * 0.3f);

        bool strong = bold, code = false;
        juce::String run;
        auto flush = [&] {
            if (run.isEmpty())
                return;
            result.append (run, code ? monoFont (size - 1.5f, strong) : uiFont (size, strong), code ? colours::accent : colour);
            run.clear();
        };

        for (int i = 0; i < text.length(); ++i)
        {
            const auto c = text[i];
            if (c == '*' && text[i + 1] == '*')
            {
                flush();
                strong = ! strong || bold;
                ++i;
            }
            else if (c == '`')
            {
                flush();
                code = ! code;
            }
            else if (c == '[' && ! code)
            {
                const int close = text.indexOf (i, "](");
                const int end = close > 0 ? text.indexOf (close, ")") : -1;
                if (end < 0)
                {
                    run << c;
                    continue;
                }
                flush();
                result.append (text.substring (i + 1, close), uiFont (size, strong), colours::grid);
                i = end;
            }
            else
            {
                run << juce::String::charToString (c);
            }
        }
        flush();
        return result;
    }
}

//==============================================================================
class ManualView::Page : public juce::Component
{
public:
    void setChapter (const ManualChapter* newChapter)
    {
        chapter = newChapter;
        layout();
    }

    void layout()
    {
        items.clear();
        if (chapter == nullptr || getWidth() <= 0)
            return;

        using Kind = ManualChapter::Block::Kind;
        const float width = (float) getWidth() - 8.0f;
        float y = 4.0f;

        auto place = [&] (const juce::AttributedString& text, float x, float w, Item::Decoration deco, int number = 0) -> Item& {
            Item item;
            item.text.createLayout (text, w);
            item.area = { x, y, w, item.text.getHeight() };
            item.decoration = deco;
            item.number = number;
            items.push_back (std::move (item));
            return items.back();
        };

        y += place (styled (chapter->title, 22.0f, colours::text, true), 0.0f, width, Item::Decoration::none).area.getHeight() + 14.0f;

        Kind previousKind = Kind::paragraph;
        for (const auto& block : chapter->blocks)
        {
            const bool listEnded = (previousKind == Kind::bullet || previousKind == Kind::step || previousKind == Kind::tableRow) && block.kind != previousKind;
            if (listEnded)
                y += 6.0f;

            switch (block.kind)
            {
                case Kind::subheading:
                    y += 10.0f;
                    y += place (styled (block.cells[0], 15.0f, colours::accent, true), 0.0f, width, Item::Decoration::none).area.getHeight() + 6.0f;
                    break;

                case Kind::paragraph:
                    y += place (styled (block.cells[0], 13.5f, colours::text), 0.0f, width, Item::Decoration::none).area.getHeight() + 10.0f;
                    break;

                case Kind::bullet:
                    y += place (styled (block.cells[0], 13.5f, colours::text), 20.0f, width - 20.0f, Item::Decoration::bullet).area.getHeight() + 6.0f;
                    break;

                case Kind::step:
                    y += place (styled (block.cells[0], 13.5f, colours::text), 32.0f, width - 32.0f, Item::Decoration::step, block.number)
                             .area.getHeight() + 8.0f;
                    break;

                case Kind::tableRow:
                {
                    // Two columns: narrow label column (keys, terms), wide description.
                    const int columns = juce::jmax (1, block.cells.size());
                    std::vector<float> widths ((size_t) columns, width / (float) columns);
                    if (columns == 2)
                        widths = { width * 0.36f, width * 0.64f };

                    float x = 0.0f, rowHeight = 0.0f;
                    const auto firstCell = items.size();
                    for (int c = 0; c < columns; ++c)
                    {
                        auto& cell = place (styled (block.cells[c], 13.0f, block.header ? colours::muted : colours::text, block.header),
                                            x + 10.0f, widths[(size_t) c] - 20.0f,
                                            block.header ? Item::Decoration::headerCell : Item::Decoration::cell);
                        cell.area.setY (y + 7.0f);
                        rowHeight = juce::jmax (rowHeight, cell.text.getHeight() + 14.0f);
                        x += widths[(size_t) c];
                    }
                    // Every cell remembers its row's full box for the background and border.
                    x = 0.0f;
                    for (auto i = firstCell; i < items.size(); ++i)
                    {
                        items[i].cell = { x, y, widths[i - firstCell], rowHeight };
                        x += widths[i - firstCell];
                    }
                    y += rowHeight;
                    break;
                }
            }
            previousKind = block.kind;
        }

        setSize (getWidth(), (int) y + 24);
    }

    void paint (juce::Graphics& g) override
    {
        for (const auto& item : items)
        {
            switch (item.decoration)
            {
                case Item::Decoration::bullet:
                    g.setColour (colours::accent);
                    g.fillEllipse (6.0f, item.area.getY() + 7.0f, 5.0f, 5.0f);
                    break;

                case Item::Decoration::step:
                {
                    const auto badge = juce::Rectangle<float> (0.0f, item.area.getY() - 1.0f, 22.0f, 22.0f);
                    g.setColour (colours::accentDark);
                    g.fillEllipse (badge);
                    g.setColour (colours::accent);
                    g.setFont (monoFont (11.5f, true));
                    g.drawText (juce::String (item.number), badge, juce::Justification::centred);
                    break;
                }

                case Item::Decoration::headerCell:
                case Item::Decoration::cell:
                    g.setColour (item.decoration == Item::Decoration::headerCell ? colours::panel2 : colours::background);
                    g.fillRect (item.cell);
                    g.setColour (colours::border);
                    g.drawRect (item.cell, 1.0f);
                    break;

                case Item::Decoration::none:
                default:
                    break;
            }
            item.text.draw (g, item.area);
        }
    }

    void resized() override
    {
        if (getWidth() != lastWidth)
        {
            lastWidth = getWidth();
            layout();
        }
    }

private:
    struct Item
    {
        enum class Decoration { none, bullet, step, cell, headerCell };

        juce::TextLayout text;
        juce::Rectangle<float> area, cell;
        Decoration decoration = Decoration::none;
        int number = 0;
    };

    const ManualChapter* chapter = nullptr;
    std::vector<Item> items;
    int lastWidth = -1;
};

//==============================================================================
ManualView::ManualView (std::function<void()> closed) : onClose (std::move (closed)), chapters (loadManual()), page (std::make_unique<Page>())
{
    setWantsKeyboardFocus (true);

    for (int i = 0; i < (int) chapters.size(); ++i)
    {
        auto* item = navItems.add (new juce::TextButton (chapters[(size_t) i].title));
        item->getProperties().set ("nav", true);
        item->setWantsKeyboardFocus (false);
        item->onClick = [this, i] { showChapter (i); };
        navContent.addAndMakeVisible (item);
    }
    navViewport.setViewedComponent (&navContent, false);
    navViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (navViewport);

    pageViewport.setViewedComponent (page.get(), false);
    pageViewport.setScrollBarsShown (true, false);
    pageViewport.setSingleStepSizes (24, 24);
    addAndMakeVisible (pageViewport);

    for (auto* b : { &previous, &next, &close })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }
    previous.getProperties().set ("icon", "arrow-l");
    setKind (previous, ButtonKind::ghost);
    previous.onClick = [this] { showChapter (current - 1); };
    next.getProperties().set ("icon", "arrow-r");
    next.getProperties().set ("iconAfter", true);
    setKind (next, ButtonKind::ghost);
    next.onClick = [this] { showChapter (current + 1); };

    close.setButtonText (tr ("Schließen"));
    close.getProperties().set ("icon", "check");
    setKind (close, ButtonKind::primary);
    close.onClick = [this] {
        if (onClose)
            onClose();
    };

    showChapter (0);
}

ManualView::~ManualView()
{
    navViewport.setViewedComponent (nullptr, false);
    pageViewport.setViewedComponent (nullptr, false);
}

juce::Rectangle<int> ManualView::card() const
{
    return getLocalBounds().reduced (juce::jmin (32, getWidth() / 20), juce::jmin (24, getHeight() / 20));
}

bool ManualView::showChapter (const juce::String& id)
{
    for (int i = 0; i < (int) chapters.size(); ++i)
    {
        if (chapters[(size_t) i].id == id)
        {
            showChapter (i);
            return true;
        }
    }
    return false;
}

void ManualView::showChapter (int index)
{
    if (chapters.empty())
        return;

    current = juce::jlimit (0, (int) chapters.size() - 1, index);
    for (int i = 0; i < navItems.size(); ++i)
        navItems[i]->setToggleState (i == current, juce::dontSendNotification);

    page->setChapter (&chapters[(size_t) current]);
    pageViewport.setViewPosition (0, 0);

    previous.setEnabled (current > 0);
    next.setEnabled (current + 1 < (int) chapters.size());
    previous.setButtonText (current > 0 ? chapters[(size_t) current - 1].title : juce::String());
    next.setButtonText (current + 1 < (int) chapters.size() ? chapters[(size_t) current + 1].title : juce::String());
    previous.setVisible (current > 0);
    next.setVisible (current + 1 < (int) chapters.size());
    repaint();
}

void ManualView::resized()
{
    auto area = card().reduced (24);
    area.removeFromTop (44); // title

    auto bottom = area.removeFromBottom (40);
    close.setBounds (bottom.removeFromRight (150));
    bottom.removeFromRight (16);
    const int pagerWidth = juce::jmin (280, bottom.getWidth() / 2);
    previous.setBounds (bottom.removeFromLeft (pagerWidth));
    next.setBounds (bottom.removeFromRight (pagerWidth));
    area.removeFromBottom (12);

    navArea = area.removeFromLeft (230);
    navViewport.setBounds (navArea);
    const int navWidth = navArea.getWidth() - (navItems.size() * 40 > navArea.getHeight() ? navViewport.getScrollBarThickness() + 4 : 0);
    navContent.setSize (navWidth, navItems.size() * 40);
    for (int i = 0; i < navItems.size(); ++i)
        navItems[i]->setBounds (0, i * 40, navWidth, 36);

    area.removeFromLeft (28);
    pageViewport.setBounds (area);
    page->setSize (area.getWidth() - pageViewport.getScrollBarThickness() - 6, page->getHeight());
}

void ManualView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.6f));

    const auto box = card().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (box, 14.0f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (box.reduced (0.5f), 14.0f, 1.0f);

    auto area = card().reduced (24);
    g.setColour (colours::text);
    g.setFont (uiFont (20.0f, true));
    g.drawText (tr ("Handbuch"), area.removeFromTop (32), juce::Justification::centredLeft);

    g.setColour (colours::border);
    g.fillRect (juce::Rectangle<int> (navArea.getRight() + 13, navArea.getY(), 1, navArea.getHeight()));
}

void ManualView::mouseDown (const juce::MouseEvent& e)
{
    if (! card().contains (e.getPosition()) && onClose)
        onClose();
}

bool ManualView::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && onClose)
    {
        onClose();
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::pageDownKey || key.getKeyCode() == juce::KeyPress::pageUpKey)
    {
        const int step = pageViewport.getHeight() - 40;
        pageViewport.setViewPosition (0, pageViewport.getViewPositionY() + (key.getKeyCode() == juce::KeyPress::pageDownKey ? step : -step));
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::downKey || key.getKeyCode() == juce::KeyPress::upKey)
    {
        pageViewport.setViewPosition (0, pageViewport.getViewPositionY() + (key.getKeyCode() == juce::KeyPress::downKey ? 40 : -40));
        return true;
    }
    if (key == juce::KeyPress::rightKey || key == juce::KeyPress::leftKey)
    {
        showChapter (current + (key == juce::KeyPress::rightKey ? 1 : -1));
        return true;
    }
    return false;
}

} // namespace amt::plugin::ui
