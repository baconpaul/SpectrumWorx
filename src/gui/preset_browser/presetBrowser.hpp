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

#include "le/utility/platformSpecifics.hpp"

#include <functional>
#include <memory>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------

class SpectrumWorx;

//------------------------------------------------------------------------------
namespace GUI
{
//------------------------------------------------------------------------------

class SpectrumWorxEditor;

class PresetBrowser final : public BackgroundImage,
                            private juce::ListBoxModel,
                            private juce::Button::Listener,
                            private juce::TextEditor::Listener,
                            public OwnedWindow<PresetBrowser>
{
  public:
    PresetBrowser();
    ~PresetBrowser();

    juce::Component &window() { return *this; }

    SpectrumWorxEditor &editor();
    SpectrumWorxEditor const &editor() const;

  private: // JUCE Component overrides.
    void paint(juce::Graphics &) override;

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
    struct Item
    {
        juce::String name;
        bool isDirectory;

        bool operator==(Item const &other) const;
        bool operator<(Item const &other) const;
    };

  private:
    void setNewFolder(juce::File const &);

    void refresh();

    void refreshAndSelectPreset(juce::String const &presetName);

    void showFilenameEditBox(juce::String const &presetName, unsigned int atRow);
    void hideFilenameEditBox();

    void saveCurrentPreset(juce::String const &presetName, juce::File const &targetFile);

    /// \note Retries itself from the dialog's callback rather than from a loop;
    /// see the definition.
    void renameTo(juce::File const &sourceFile, juce::File const &targetFile,
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
    juce::File file(unsigned int index) const;
    juce::File selectedFile() const;

    Item const *findPreset(juce::String const &presetName) const;

    juce::TextEditor &comment() { return commentBox_; }
    BackgroundImage &background() { return *this; }

  private: // friend class Detail::BackgroundWithCurrentFolder;
    juce::TextEditor presetNameEditBox_;
    juce::TextEditor commentBox_;
    juce::ListBox listBox_;
    BitmapButton save_;
    BitmapButton saveAs_;
    BitmapButton delete_;
    BitmapButton browseArrow_;
    LEDTextButton ignoreExternalSamples_;

    bool ignoreSelectionChange_;
    bool addOneRow_;
    bool newPresetPending_;

    int dirtyCommentPresetIndex_;

    juce::File currentDirectory_;
    juce::Array<Item> files_;

    juce::String originalComment_;

    /// \note Held rather than stack-local: JUCE 8's FileChooser reports through
    /// launchAsync() and must outlive the call that starts it.
    std::unique_ptr<juce::FileChooser> folderChooser_;
}; // class PresetBrowser

//------------------------------------------------------------------------------
} // namespace GUI
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // presetBrowser_hpp
