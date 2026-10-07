#pragma once

#include "LookAndFeel.h"

namespace amt::plugin::ui
{

/** A chapter of the manual, parsed from a small Markdown subset: "## " starts a chapter,
    "### " a sub-heading; paragraphs, "- " bullets, "1. " steps and "| a | b |" tables;
    **bold**, `keys` and [links](…) inline. "<!-- id: x -->" gives the chapter an id, images are skipped. */
struct ManualChapter
{
    struct Block
    {
        enum class Kind { subheading, paragraph, bullet, step, tableRow };
        Kind kind = Kind::paragraph;
        juce::StringArray cells; ///< text (one cell), or the columns of a table row
        int number = 0;          ///< steps
        bool header = false;     ///< first row of a table
    };

    juce::String id, title;
    std::vector<Block> blocks;
};

std::vector<ManualChapter> parseManual (const juce::String& markdown);

/** The manual of the current language (embedded docs/HANDBUCH.md or docs/MANUAL.md). */
std::vector<ManualChapter> loadManual();

/** "Handbuch" overlay: chapters on the left, the chapter on the right (scrollable). */
class ManualView : public juce::Component
{
public:
    explicit ManualView (std::function<void()> onClose);
    ~ManualView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    /** Shows the chapter with this id (see the "<!-- id: … -->" lines); false if there is none. */
    bool showChapter (const juce::String& id);
    void showChapter (int index);
    int getChapterIndex() const { return current; }
    const std::vector<ManualChapter>& getChapters() const { return chapters; }

private:
    class Page;

    juce::Rectangle<int> card() const;

    std::function<void()> onClose;
    std::vector<ManualChapter> chapters;
    int current = 0;

    juce::OwnedArray<juce::TextButton> navItems;
    juce::Viewport navViewport, pageViewport;
    juce::Component navContent;
    std::unique_ptr<Page> page;
    juce::TextButton previous, next, close;
    juce::Rectangle<int> navArea;
};

} // namespace amt::plugin::ui
