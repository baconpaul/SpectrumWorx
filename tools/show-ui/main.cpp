////////////////////////////////////////////////////////////////////////////////
///
/// \file main.cpp
/// --------------
///
///   sw-show-ui: a window that hosts SpectrumWorx's GUI with no plugin, no host
/// and no audio underneath it.
///
///   Stage 6 ports 10 k lines of JUCE 2.1.2 widget code to JUCE 8, and the
/// plugin those widgets belong to does not exist yet -- the CLAP host layer is
/// stage 5. Waiting for it would put the largest stage on the critical path for
/// no reason, so the GUI is built against a mock instead, in a process a
/// developer can launch, resize and quit in a second.
///
///   Pages are registered by the code they exercise, so a widget becomes
/// viewable the moment it compiles rather than when the whole editor does:
///
///     sw-show-ui                          the default page, in a window
///     sw-show-ui --list                   what there is
///     sw-show-ui gallery                  a named page
///     sw-show-ui --render gallery out.png offscreen, no window server
///
///   --render exists because a window is not always available: CI has no
/// display, and neither does a sandboxed agent. It paints the page into an
/// image and writes a PNG, which is the only way some environments can see
/// whether a widget draws at all. It is also what makes a rendering regression
/// test possible later.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "page.hpp"

#include "gui/theme.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>
#include <cstring>
//------------------------------------------------------------------------------
namespace
{
//------------------------------------------------------------------------------

void listPages()
{
    std::fputs("sw-show-ui pages:\n", stderr);
    for (auto const &page : SWShowUI::pages())
        std::fprintf(stderr, "  %-16s %s\n", page.name, page.description);
}

/// Resolves a page name to a page, complaining usefully when it cannot.
SWShowUI::Page const *resolve(juce::String const &name)
{
    if (auto const *const page = SWShowUI::findPage(name))
        return page;
    std::fprintf(stderr, "sw-show-ui: no page named '%s'.\n", name.toRawUTF8());
    listPages();
    return nullptr;
}

//------------------------------------------------------------------------------
// Windowed mode
//------------------------------------------------------------------------------

/// A plain resizable desktop window. The editor is not an AudioProcessorEditor
/// and never will be -- this is the clap-first route -- so there is nothing to
/// attach here beyond a Component.
class HarnessWindow final : public juce::DocumentWindow
{
  public:
    HarnessWindow(juce::String const &name, std::unique_ptr<juce::Component> content)
        : juce::DocumentWindow(name, juce::Colours::darkgrey, juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        auto const width(content->getWidth() ? content->getWidth() : 900);
        auto const height(content->getHeight() ? content->getHeight() : 600);
        setContentOwned(content.release(), true);
        setResizable(true, false);
        centreWithSize(width, height);
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
}; // class HarnessWindow

class ShowUIApplication final : public juce::JUCEApplication
{
  public:
    juce::String const getApplicationName() override { return "sw-show-ui"; }
    juce::String const getApplicationVersion() override { return "0.1"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(juce::String const &commandLine) override
    {
        auto const requested(firstToken(commandLine));
        if (requested == "--list")
        {
            listPages();
            quit();
            return;
        }

        auto const *const page(resolve(requested));
        if (!page)
        {
            setApplicationReturnValue(2);
            quit();
            return;
        }

        window_ = std::make_unique<HarnessWindow>("SpectrumWorx -- " + juce::String(page->name),
                                                  page->construct());
    }

    void shutdown() override
    {
        window_.reset();
        // Before JUCE's leak detector runs: the skin cache holds juce::Images
        // and Typefaces, and a namespace-scope cache would otherwise be
        // destroyed after the JUCE that allocated them.
        LE::SW::GUI::Theme::destroySingleton();
    }
    void systemRequestedQuit() override { quit(); }

  private:
    /// The command line arrives as one string; take the first non-empty token.
    static juce::String firstToken(juce::String const &commandLine)
    {
        for (auto const &token : juce::StringArray::fromTokens(commandLine, true))
            if (token.isNotEmpty())
                return token;
        return SWShowUI::defaultPageName();
    }

    std::unique_ptr<HarnessWindow> window_;
}; // class ShowUIApplication

//------------------------------------------------------------------------------
// Offscreen mode
//------------------------------------------------------------------------------

int renderPage(juce::String const &pageName, juce::File const &output)
{
    auto const *const page(resolve(pageName));
    if (!page)
        return 2;

    auto const component(page->construct());
    if (component->getWidth() <= 0 || component->getHeight() <= 0)
        component->setSize(900, 600);

    juce::Image image(juce::Image::ARGB, component->getWidth(), component->getHeight(), true);
    {
        juce::Graphics graphics(image);
        component->paintEntireComponent(graphics, true);
    }

    output.deleteFile();
    std::unique_ptr<juce::FileOutputStream> stream(output.createOutputStream());
    if (!stream || stream->failedToOpen())
    {
        std::fprintf(stderr, "sw-show-ui: cannot write '%s'.\n",
                     output.getFullPathName().toRawUTF8());
        return 1;
    }
    if (!juce::PNGImageFormat().writeImageToStream(image, *stream))
    {
        std::fputs("sw-show-ui: PNG encoding failed.\n", stderr);
        return 1;
    }

    std::fprintf(stderr, "sw-show-ui: wrote %d x %d to %s\n", image.getWidth(), image.getHeight(),
                 output.getFullPathName().toRawUTF8());
    return 0;
}

int render(juce::String const &pageName, juce::File const &output)
{
    // No JUCEApplication and no message loop: paintEntireComponent() needs a
    // graphics context and a font engine, which ScopedJuceInitialiser_GUI
    // provides, and nothing else.
    juce::ScopedJuceInitialiser_GUI const juceInitialiser;
    auto const result(renderPage(pageName, output));
    // Inside the initialiser's lifetime, so the images and typefaces the page
    // cached are gone before JUCE's leak detector looks. Getting this wrong is
    // not cosmetic: JUCE is a static-destruction order hazard, and the
    // resources genuinely outlive it otherwise.
    LE::SW::GUI::Theme::destroySingleton();
    return result;
}

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

// What START_JUCE_APPLICATION expands to, opened up so that --render can run
// before any of it: JUCEApplicationBase::main() creates an NSApplication on
// macOS, which offscreen rendering neither needs nor should require.
juce::JUCEApplicationBase *juce_CreateApplication();
juce::JUCEApplicationBase *juce_CreateApplication() { return new ShowUIApplication(); }

int main(int argc, char *argv[])
{
    if ((argc >= 2) && (std::strcmp(argv[1], "--render") == 0))
    {
        if (argc != 4)
        {
            std::fputs("usage: sw-show-ui --render <page> <output.png>\n", stderr);
            return 2;
        }
        juce::String const path(argv[3]);
        return render(juce::String(argv[2]),
                      juce::File::isAbsolutePath(path)
                          ? juce::File(path)
                          : juce::File::getCurrentWorkingDirectory().getChildFile(path));
    }

    juce::JUCEApplicationBase::createInstance = &juce_CreateApplication;
    return juce::JUCEApplicationBase::main(argc, const_cast<char const **>(argv));
}
