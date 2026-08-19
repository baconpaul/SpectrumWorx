////////////////////////////////////////////////////////////////////////////////
///
/// \file presetBrowser.hpp
/// -----------------------
///
/// SpectrumWorx preset browser implementation.
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef presetBrowser_hpp__228370D3_4C4C_46B8_8544_9273C3AAEB61A
#define presetBrowser_hpp__228370D3_4C4C_46B8_8544_9273C3AAEB61A
//------------------------------------------------------------------------------
#include "gui/gui.hpp"

#include "le/spectrumworx/presets.hpp"
#include "le/utility/platformSpecifics.hpp"

#include <cstdint>
#include <functional>
#include <memory>

namespace LE::SW
{

class SpectrumWorx;

namespace GUI
{

class SpectrumWorxEditor;

class PresetBrowser final : public PanelBackground,
                            private juce::ListBoxModel,
                            private juce::Button::Listener,
                            private juce::TextEditor::Listener
{
  public:
    PresetBrowser();
    ~PresetBrowser();

    SpectrumWorxEditor &editor();
    SpectrumWorxEditor const &editor() const;

  private: // JUCE Component overrides.
    void paint(juce::Graphics &) override;

    /// \note Reached from SpectrumWorxEditor::applyPaletteIfChanged(), by way
    /// of juce::Component::sendLookAndFeelChange(). \see takeColours().
    void lookAndFeelChanged() override { takeColours(); }

  private:
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \brief Tells the two text editors what to paint themselves in.
    ///
    /// \note Not a one-off in the constructor, because these are the one thing
    /// in the browser that *holds* colours rather than asking for them each
    /// paint: juce::TextEditor::setColour() overrides the LookAndFeel's answer
    /// for good, so a palette change leaves them in the palette they were built
    /// under until they are told again.
    ///
    ////////////////////////////////////////////////////////////////////////////
    void takeColours();

  private: // JUCE ButtonListener overrides.
    void buttonClicked(juce::Button *) override;

  private: // JUCE TextEditorListener overrides.
    void textEditorTextChanged(juce::TextEditor &) override;
    void textEditorReturnKeyPressed(juce::TextEditor &) override;
    void textEditorEscapeKeyPressed(juce::TextEditor &) override;
    void textEditorFocusLost(juce::TextEditor &) override;

  private: // JUCE ListBoxModel overrides.
    int getNumRows() noexcept override;
    void paintListBoxItem(int rowNumber, juce::Graphics &, int width, int height,
                          bool rowIsSelected) override;
    void listBoxItemDoubleClicked(int row, juce::MouseEvent const &) override;
    void deleteKeyPressed(int lastRowSelected) noexcept override;
    void returnKeyPressed(int lastRowSelected) noexcept override;
    void selectedRowsChanged(int lastRowSelected) override;

  private:
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \enum Location
    ///
    /// \brief Which of the two trees the list is showing.
    ///
    /// \note The browser was a directory browser: one `juce::File` and
    /// `std::filesystem` over it. The factory banks are in the binary now (8.2)
    /// and have no directory to point it at, so where it is looking became a
    /// small sum type rather than a path.
    ///                                       (31.07.2026.) (SW port)
    ///
    /// \note There was a third, `Root`, whose listing was the two words
    /// "Factory" and "User" -- so opening the browser showed a plugin with 303
    /// presets in it two rows and no preset names, and every session began with
    /// two clicks that told the user nothing. The trees are a *toggle* now, in
    /// the navigation row, and the browser opens inside one of them.
    ///                                       (19.08.2026.) issue #44
    ///
    ////////////////////////////////////////////////////////////////////////////

    enum struct Location : std::uint8_t
    {
        Factory, ///< an embedded bank, addressed by factoryBank_. Read only.
        User     ///< a real directory, addressed by currentDirectory_
    };

    struct Item
    {
        /// \note `Parent` -- the ".." row -- went with `Root`: the up button
        /// owns going up now, and a list whose first row is punctuation is a
        /// row of preset names lost to a control that has somewhere better to
        /// be. \see issue #44.
        enum struct Kind : std::uint8_t
        {
            Folder, ///< a bank or a sub-directory
            Preset
        };

        juce::String name;
        Kind kind{Kind::Preset};

        bool isDirectory() const { return kind != Kind::Preset; }

        bool operator==(Item const &other) const;
        bool operator<(Item const &other) const;
    };

  private:
  public:
    /// \note Public for `tools/show-ui`, which opens the browser on a bank to
    /// render it. See SpectrumWorxEditor::showFactoryBank().
    void setFactoryBank(juce::String const &bank);

  private:
    void setNewFolder(fs::path const &);

    /// \brief Up one level, wherever "up" is from here. Does nothing at the top
    /// of either tree, which is also where the up button is disabled.
    void goToParent();

    /// \brief Whether this is the top of the tree it is in -- the bank list, or
    /// the user's own preset folder.
    bool atTopOfTree() const;

    /// \brief Whether the selected row is a preset, which is what the jog needs
    /// to have somewhere to step *from*. A folder row, or no row at all, is not
    /// one. \see canStep().
    bool presetIsSelected() const;

    /// \brief Whether stepPreset( \p direction ) would move: there is a preset
    /// selected and another one that way.
    ///
    /// \note One predicate for the two questions the jog asks -- "may I be
    /// pressed" and "is there anything to do" -- so that a lit button that does
    /// nothing is not a state this can reach. \see updateNavigation().
    bool canStep(int direction) const;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \brief The previous or next preset in this listing, selected as though
    /// the user had clicked it.
    ///
    /// \param direction -1 or +1.
    ///
    /// \note Clamped rather than wrapped, and folders are not stepped onto:
    /// what a jog is for is hearing the presets in a bank one after another,
    /// and both of those would interrupt that. Neither is a case this has to
    /// handle gracefully, because canStep() has already disabled the button --
    /// at the first preset in a listing the back arrow is dead and at the last
    /// one the forward arrow is.
    ///
    ////////////////////////////////////////////////////////////////////////////

    void stepPreset(int direction);

    /// \brief Points the navigation row at where the browser now is. Called by
    /// refresh(), which every move comes through.
    void updateNavigation();

    /// \brief What the header strip says, for whichever of the two it is
    /// showing. Only `User` has a path to print.
    juce::String locationLabel() const;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \struct Place
    ///
    /// \brief Where the browser was when it last closed.
    ///
    /// \note Process-wide and main thread only, because the browser is built
    /// and destroyed every time the overlay opens and closes -- so where it was
    /// has to outlive it. It used to be half remembered: the destructor wrote
    /// `currentDirectory_` into `GUI::presetsFolder()` and nothing recorded
    /// `location_`, which is a member initialiser, so the browser reopened at
    /// the root whatever the user had been looking at. Writing the user's
    /// wanderings into `presetsFolder()` also moved the anchor `goToParent()`
    /// stops at and the folder `createUserPresetsFolder()` makes; that global
    /// means the preset root now and nothing writes to it.
    ///                                       (08.08.2026.) (SW port)
    ///
    ////////////////////////////////////////////////////////////////////////////

    struct Place
    {
        Location location{Location::Factory};
        juce::String factoryBank; ///< when location is Factory
        fs::path folder;          ///< when location is User
    };

    static Place &lastPlace();
    void restoreLastPlace();

    void refresh();
    void refreshFactory();
    void refreshUserDirectory();

    /// \brief The selected preset's bytes, from the binary or from a file.
    Preset::InMemoryPreset selectedPresetData() const;

    bool inFactory() const { return location_ == Location::Factory; }

    void refreshAndSelectPreset(juce::String const &presetName);

    void showFilenameEditBox(juce::String const &presetName, unsigned int atRow);
    void hideFilenameEditBox();

    void saveCurrentPreset(juce::String const &presetName, fs::path const &targetFile);

    /// \note Retries itself from the dialog's callback rather than from a loop;
    /// see the definition.
    void renameTo(fs::path const &sourceFile, fs::path const &targetFile,
                  juce::String const &newName);

    void saveDirtyComment();
    void presetSelectionChanged();

    void deselectAllRows();

    void addOneRow(bool const value) { addOneRow_ = value; }

    /// \note Was `bool askForOverwrite()`, answered where it was asked. JUCE 8
    /// builds with JUCE_MODAL_LOOPS_PERMITTED=0 -- and a plugin has no business
    /// spinning a modal loop inside a host's message thread -- so the answer
    /// arrives later, on the message thread.
    static void askForOverwrite(std::function<void(bool)> onAnswer);

    bool enablePresetSaving() const;

    unsigned int selectedIndex() const;

    Item const &item(unsigned int index) const;
    Item const &selectedItem() const;
    fs::path file(unsigned int index) const;
    fs::path selectedFile() const;

    Item const *findPreset(juce::String const &presetName) const;

    juce::TextEditor &comment() { return commentBox_; }
    PanelBackground &background() { return *this; }

  private: // friend class Detail::BackgroundWithCurrentFolder;
    juce::TextEditor presetNameEditBox_;
    juce::TextEditor commentBox_;
    juce::ListBox listBox_;
    PaintedButton save_;
    PaintedButton saveAs_;
    PaintedButton delete_;
    ArrowButton browseArrow_;

    /// \name The navigation row, between the Save buttons and the list
    ///
    /// \note Issue #44's. The row is where "Ignore external audio" was; that
    /// toggle is the editor's lock beside the sidechain source now.
    ///@{
    GlyphButton upFolder_;
    GlyphButton userPresets_;
    GlyphButton jogPrevious_;
    GlyphButton jogNext_;
    ///@}

    bool ignoreSelectionChange_;
    bool addOneRow_;
    bool newPresetPending_;

    int dirtyCommentPresetIndex_;

    Location location_{Location::Factory};

    /// The bank, relative to the preset root, when location_ is Factory. Empty
    /// means the top of the factory tree.
    juce::String factoryBank_;

    fs::path currentDirectory_;
    juce::Array<Item> files_;

    juce::String originalComment_;

    /// \note Held rather than stack-local: JUCE 8's FileChooser reports through
    /// launchAsync() and must outlive the call that starts it.
    std::unique_ptr<juce::FileChooser> folderChooser_;
}; // class PresetBrowser

} // namespace GUI

} // namespace LE::SW

#endif // presetBrowser_hpp
