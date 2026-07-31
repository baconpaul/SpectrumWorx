////////////////////////////////////////////////////////////////////////////////
///
/// presetBrowser.cpp
/// -----------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "presetBrowser.hpp"

#include "configuration/versionConfiguration.hpp"
#include "gui/editor/spectrumWorxEditor.hpp"

#include "le/parameters/uiElements.hpp"
#include "le/spectrumworx/presetFile.hpp"
#include "le/spectrumworx/presets.hpp"
#include "le/utility/countof.hpp"
#include "le/utility/tchar.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include <filesystem>
#include <system_error>

#include "le/utility/assert.hpp"
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------
namespace GUI
{
//------------------------------------------------------------------------------

namespace
{
typedef juce::String::CharPointerType::CharType char_t;

static char_t const presetExtension[] = _T( ".swp" );
unsigned int const presetExtensionLength = _countof(presetExtension) - 1;
} // namespace

#pragma warning(push)
#pragma warning(disable : 4355) // 'this' used in base member initializer list.

PresetBrowser::PresetBrowser()
    : BackgroundImage(resourceBitmap<PresetBackground>()),
      save_(*this, resourceBitmap<PresetSaveDown>(), resourceBitmap<PresetSaveUp>(),
            juce::Colours::transparentWhite, false),
      saveAs_(*this, resourceBitmap<PresetSaveAsDown>(), resourceBitmap<PresetSaveAsUp>(),
              juce::Colours::transparentWhite, false),
      delete_(*this, resourceBitmap<PresetDeleteDown>(), resourceBitmap<PresetDeleteUp>(),
              juce::Colours::transparentWhite, false),
      browseArrow_(*this, resourceBitmap<ChangeWaveform>(), resourceBitmap<ChangeWaveform>(),
                   juce::Colours::white.withAlpha(0.5f), false),
      ignoreExternalSamples_(*this, 15, 58, "Ignore external audio"), ignoreSelectionChange_(false),
      addOneRow_(false), newPresetPending_(false), dirtyCommentPresetIndex_(-1)
{
    listBox_.setModel(this);

    setSizeFromImage(*this, this->image());

    save_.setEnabled(false);
    saveAs_.setEnabled(enablePresetSaving());
    delete_.setEnabled(false);
    save_.addListener(this);
    saveAs_.addListener(this);
    delete_.addListener(this);

    browseArrow_.addListener(this);

    setNewFolder(GUI::presetsFolder());

    browseArrow_.setTopLeftPosition(174, 10);
    save_.setTopLeftPosition(17, 33);
    saveAs_.setTopLeftPosition(17 + 55, 33);
    delete_.setTopLeftPosition(17 + 55 + 55, 33);
    listBox_.setBounds(11, 79, getWidth() - 22, 234);
    comment().setBounds(8, 322, getWidth() - 13, 29);

    addChildComponent(&presetNameEditBox_);
    presetNameEditBox_.setAlwaysOnTop(true);
    presetNameEditBox_.setFont(Theme::singleton().whiteFont());
    presetNameEditBox_.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black);
    presetNameEditBox_.setColour(juce::TextEditor::focusedOutlineColourId,
                                 Theme::singleton().blueColour());
    presetNameEditBox_.addListener(this);

    listBox_.setOpaque(false);
    listBox_.setRowHeight(16);
    listBox_.getViewport()->setScrollBarThickness(12);

    comment().setMultiLine(true);
    comment().setReturnKeyStartsNewLine(true);
    comment().setPopupMenuEnabled(true);
    comment().setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    comment().setColour(juce::TextEditor::textColourId, Theme::singleton().blueColour());
    comment().setColour(juce::TextEditor::highlightColourId, juce::Colours::lightgrey);
    comment().addListener(this);
    {
        juce::Font font(Theme::singleton().Theme::getPopupMenuFont());
        font.setHeight(11);
        comment().setFont(font);
    }

    //  Required so that a preset can be deselected by clicking anywhere outside
    // the file list box.
    this->setWantsKeyboardFocus(true);

    // Implementation note:
    //   We enable the comment box only when a preset is selected (in other
    // words to create a new preset with a comment you first need to create a
    // preset and then add a comment to the new/existing preset).
    //                                        (27.05.2010.) (Domagoj Saric)
    comment().setEnabled(false);
    comment().setInputRestrictions(PresetHeader::maxCommentLength - 1);

    LE_ASSERT(!save_.getMouseClickGrabsKeyboardFocus());
    LE_ASSERT(!saveAs_.getMouseClickGrabsKeyboardFocus());
    LE_ASSERT(!delete_.getMouseClickGrabsKeyboardFocus());

    addToParentAndShow(*this, comment());
    addToParentAndShow(*this, listBox_);

    OwnedWindow<PresetBrowser>::attach();

#ifdef LE_SW_DISABLE_SIDE_CHANNEL
    ignoreExternalSamples_.setValue(true);
    ignoreExternalSamples_.setEnabled(false);
#endif // LE_SW_DISABLE_SIDE_CHANNEL
}

#pragma warning(pop)

PresetBrowser::~PresetBrowser()
{
    //...mrmlj...fade out does not work for 'on desktop components'
    //this->fadeOutComponent( 600, 0, 0, 0.2f );
    //juce::Point<int> const centre( this->getBounds().getCentre() );
    //juce::Desktop::getInstance().getAnimator().animateComponent( this, juce::Rectangle<int>( centre, centre ), 0, 600, true, 0, 0 );
    GUI::presetsFolder() = currentDirectory_;
}

bool PresetBrowser::enablePresetSaving() const { return true; }

void PresetBrowser::presetSelectionChanged()
{
    Item const &item(selectedItem());
    if (item.isDirectory)
    {
        save_.setEnabled(false);
        delete_.setEnabled(false);
        return;
    }

    bool const enablePresetSaving(this->enablePresetSaving());

    save_.setEnabled(enablePresetSaving);
    delete_.setEnabled(enablePresetSaving);

    bool const succeeded(editor().loadPreset(
        selectedFile(), ignoreExternalSamples_.getToggleState(), originalComment_, item.name));

    if (!succeeded)
    {
        Preset::reportPresetLoadingError();
        return;
    }

    comment().setText(originalComment_, false);
    comment().setEnabled(enablePresetSaving);
    LE_ASSERT(comment().getWantsKeyboardFocus() || !comment().isEnabled());
}

unsigned int PresetBrowser::selectedIndex() const
{
    int const index(listBox_.getLastRowSelected());
    LE_ASSERT_MSG(index >= 0, "Nothing selected");
    return index;
}

PresetBrowser::Item const &PresetBrowser::item(unsigned int const index) const
{
    Item const &item(files_.getReference(index));
    LE_ASSERT(
        currentDirectory_.getChildFile(item.name + (item.isDirectory ? _T( "" ) : presetExtension))
            .exists());
    return item;
}

PresetBrowser::Item const &PresetBrowser::selectedItem() const { return item(selectedIndex()); }

juce::File PresetBrowser::file(unsigned int const index) const
{
    Item const &item(this->item(index));
    LE_ASSERT(!item.isDirectory);
    return currentDirectory_.getChildFile(item.name + presetExtension);
}

juce::File PresetBrowser::selectedFile() const { return file(selectedIndex()); }

PresetBrowser::Item const *PresetBrowser::findPreset(juce::String const &presetName) const
{
    return std::find_if(files_.begin(), files_.end(),
                        [&](Item const &item) { return presetName == item.name; });
}

void PresetBrowser::listBoxItemDoubleClicked(int const row, juce::MouseEvent const &)
{
    Item const &item(this->item(row));
    if (item.isDirectory)
        setNewFolder(currentDirectory_.getChildFile(item.name));
    else
        showFilenameEditBox(item.name, listBox_.getLastRowSelected());
}

void PresetBrowser::paintListBoxItem(int const rowNumber, juce::Graphics &graphics, int const width,
                                     int const height, bool const rowIsSelected)
{
#ifdef __APPLE__
    // Implementation note:
    //   Under OS X this also gets called when the user double-clicks on the
    // last preset in the browser (on double-click we bring up the filename
    // edit box and add the bogus extra row for which this paint method gets
    // invoked).
    //                                        (25.11.2011.) (Domagoj Saric)
    if (rowNumber >= files_.size())
    {
        LE_ASSERT(addOneRow_ == true);
        LE_ASSERT(rowNumber == files_.size());
        LE_ASSERT(!rowIsSelected);
        return;
    }
#endif // __APPLE__

    LE_ASSERT(rowNumber < files_.size());

    if (rowIsSelected)
        graphics.fillAll(Theme::singleton().Theme::findColour(
            juce::DirectoryContentsDisplayComponent::highlightColourId));

    Item const &item(this->item(rowNumber));

    bool const isDirectory(item.isDirectory);

    unsigned int const x(isDirectory ? 16 : 4);

    graphics.setColour(juce::Colours::black);

    if (isDirectory)
    {
        //...mrmlj...
        //paintImage
        //(
        //    graphics,
        //    Theme::singleton().Theme::getDefaultFolderImage(),
        //    2, 2
        //);
        /// \note JUCE 8's getDefaultFolderImage() returns a Drawable rather than
        /// an Image, and may return nothing at all.
        if (auto const *const pFolderImage = Theme::singleton().Theme::getDefaultFolderImage())
            pFolderImage->drawWithin(
                graphics,
                juce::Rectangle<float>(2.0f, 2.0f, static_cast<float>(x) - 4.0f,
                                       static_cast<float>(height) - 4.0f),
                juce::RectanglePlacement::xLeft | juce::RectanglePlacement::xRight |
                    juce::RectanglePlacement::onlyReduceInSize,
                1.0f);
    }

    graphics.setColour(Theme::singleton().Theme::findColour(
        juce::DirectoryContentsDisplayComponent::textColourId));
    graphics.setFont(height * 0.7f);

    graphics.drawFittedText(item.name, x, 0, width - x, height, juce::Justification::centredLeft,
                            1);
}

void PresetBrowser::deleteKeyPressed(int /*lastRowSelected*/) noexcept {}

void PresetBrowser::returnKeyPressed(int /*lastRowSelected*/) noexcept {}

void PresetBrowser::textEditorTextChanged(juce::TextEditor &editor)
{
    LE_ASSERT((&editor == &this->presetNameEditBox_) || (&editor == &this->comment()));
    if (&editor == &comment())
        dirtyCommentPresetIndex_ = listBox_.getLastRowSelected();
}

////////////////////////////////////////////////////////////////////////////////
//
// PresetBrowser::textEditorReturnKeyPressed()
// -------------------------------------------
//
////////////////////////////////////////////////////////////////////////////////
///
/// \note This is the save path the plan calls out: it asked the user two
/// questions -- "overwrite?" and "retry?" -- and read the answers as return
/// values, one of them from inside a `while` condition. Neither dialog can
/// answer in place under JUCE 8, so both are inverted into continuations.
///
///   Each one captures a `SafePointer` rather than `this`. The browser is an
/// owned window and the user can shut it while a dialog is up, which under the
/// old synchronous code was impossible and is now the normal way to dismiss one.
///
////////////////////////////////////////////////////////////////////////////////

void PresetBrowser::textEditorReturnKeyPressed(juce::TextEditor &editor)
{
    LE_ASSERT(listBox_.getViewport()->isVerticalScrollBarShown() ||
              listBox_.getViewport()->isHorizontalScrollBarShown());
    LE_ASSERT(&editor == &this->presetNameEditBox_);

    // Implementation note:
    //   We simply do not accept empty input. The user either has to cancel the
    // operation or enter something meaningful.
    //                                        (14.12.2009.) (Domagoj Saric)
    juce::String const userEntry(editor.getText());
    if (userEntry.isEmpty())
        return;

    juce::File const targetFile(currentDirectory_.getChildFile(userEntry + presetExtension));

    juce::Component::SafePointer<PresetBrowser> const self(this);

    if (newPresetPending_)
    {
        auto const save([self, userEntry, targetFile] {
            if (!self)
                return;
            self->newPresetPending_ = false;
            self->hideFilenameEditBox();
            self->saveCurrentPreset(userEntry, targetFile);
        });

        if (!targetFile.exists())
            return save();

        askForOverwrite([save, targetFile](bool const overwrite) {
            if (!overwrite)
                return;
            targetFile.deleteFile();
            save();
        });
        return;
    }

    juce::File const sourceFile(selectedFile());
    if (sourceFile == targetFile)
        return;

    auto const rename([self, sourceFile, targetFile, userEntry] {
        if (self)
            self->renameTo(sourceFile, targetFile, userEntry);
    });

    if (!targetFile.exists())
        return rename();

    askForOverwrite([rename](bool const overwrite) {
        if (overwrite)
            rename();
    });
}

/// \note Was the body of a `while` loop whose condition asked the user whether
/// to retry. It calls itself from the dialog's callback instead -- one live
/// attempt at a time, and the stack unwinds between them.
void PresetBrowser::renameTo(juce::File const &sourceFile, juce::File const &targetFile,
                             juce::String const &newName)
{
    if (sourceFile.moveFileTo(targetFile))
    {
        hideFilenameEditBox();
        refreshAndSelectPreset(newName);
        return;
    }

    juce::Component::SafePointer<PresetBrowser> const self(this);
    GUI::warningOkCancelBox(_T( "Error writing." ), _T( "Retry?" ),
                            [self, sourceFile, targetFile, newName](bool const retry) {
                                if (retry && self)
                                    self->renameTo(sourceFile, targetFile, newName);
                            });
}

void PresetBrowser::textEditorEscapeKeyPressed(juce::TextEditor &editor)
{
    if (&editor == &this->presetNameEditBox_)
    {
        hideFilenameEditBox();
    }
    else
    {
        LE_ASSERT(&editor == &this->comment());
        comment().setText(originalComment_, false);
    }
}

void PresetBrowser::textEditorFocusLost(juce::TextEditor &editor)
{
    if (&editor == &this->presetNameEditBox_)
    {
        hideFilenameEditBox();
    }
    else
    {
        LE_ASSERT(&editor == &this->comment());
        // Implementation note:
        //   No need to do anything here: already handled in focusLost().
        //                                    (15.03.2010.) (Domagoj Saric)
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// PresetBrowser::saveDirtyComment()
// ---------------------------------
//
////////////////////////////////////////////////////////////////////////////////
// Implementation note:
//   Focus change monitoring and handling logic failed to work as desired
// when a juce::TextEditor::Listener was used to 'listen' to the comment
// 'TextEditor' because JUCE sends notifications through listeners
// asynchronously so a focus lost notification was received after the
// ListBoxModel::selectedRowsChanged() notification was received when all
// the required information on the current preset and a possibly non-saved
// comment are lost. For this reason a little utility class was used that simply
// derives from juce::TextEditor and override its focusLost() method to handle
// it directly (and synchronously). This approach however required RTTI on OSX
// (because JUCE converts between TextEditorListener and Component instance
// pointers with dynamic_cast) so the "dirty comment tracking" technique is
// currently used.
//                                            (16.12.2010.) (Domagoj Saric)
////////////////////////////////////////////////////////////////////////////////

void PresetBrowser::saveDirtyComment()
{
    if (dirtyCommentPresetIndex_ < 0)
        return;

    juce::File const dirtyPreset(this->file(dirtyCommentPresetIndex_));

    dirtyCommentPresetIndex_ = -1;

    if (dirtyPreset.existsAsFile())
    {
        juce::String const newComment(comment().getText());

        //...assert that the file/preset on disk is the same as the one in selectedPreset...
        Preset::InMemoryPresetBuffer newPresetData;
        unsigned int newPresetDataSize(0);
        {
            auto const pPresetData(readPresetFile(dirtyPreset));
            if (!pPresetData)
                return;
            PresetHeader const presetHeader(newComment);
            Preset preset(pPresetData.get());
            preset.setHeader(presetHeader);
            newPresetDataSize = preset.saveTo(&newPresetData[0]);
        }
        writePresetFile(dirtyPreset, newPresetData.data(), newPresetDataSize);
    }
}

void PresetBrowser::saveCurrentPreset(juce::String const &presetName, juce::File const &targetFile)
{
    bool const shouldRefresh(!targetFile.exists());

    originalComment_ = comment().getText();
    editor().savePreset(targetFile, ignoreExternalSamples_.getToggleState(), originalComment_);

    if (shouldRefresh)
        refreshAndSelectPreset(presetName);
}

void PresetBrowser::buttonClicked(juce::Button *const pButton)
{
    if (pButton == &save_)
    {
        saveCurrentPreset(selectedItem().name, selectedFile());
    }
    else if (pButton == &delete_)
    {
        selectedFile().deleteFile();
        refresh();
        delete_.setEnabled(false);
        deselectAllRows();
    }
    else if (pButton == &saveAs_)
    {
        juce::String newPreset(editor().currentProgramName());
        unsigned int const newPresetNameLength(newPreset.length());

        Item const *pPreset;
        Item const *const pPresetsEnd(files_.end());
        unsigned int counter(0);
        while ((pPreset = findPreset(newPreset)) != pPresetsEnd)
        {
            unsigned int const suffixLength(1 + 1 + 2 + counter / 100 + 1);
            newPreset.preallocateBytes((newPresetNameLength + suffixLength) * sizeof(char_t));
            char_t *const pSuffix(newPreset.getCharPointer().getAddress() + newPresetNameLength);
            LE_VERIFY(LE_INT_SPRINTF(pSuffix, _T( " (%02u)" ), ++counter) <= signed(suffixLength));
        }

        // Implementation note:
        //   Creating a new preset is done asynchronously (as it waits for
        // user input) so we return here immediately (delegating further
        // processing to the textEditorReturnKeyPressed() callback) and skip
        // the comment().moveKeyboardFocusToSibling() call as this would
        // cause the popup preset name edit box to disappear immediately.
        //                                    (17.12.2009.) (Domagoj Saric)

        newPresetPending_ = true;

        showFilenameEditBox(newPreset, PresetBrowser::getNumRows());
        return;
    }
    else
    {
        LE_ASSERT(pButton == &browseArrow_);
        /// \note Asynchronous, and the chooser is a member: JUCE 8 has no
        /// browseForDirectory(), and launchAsync() reports through a callback
        /// that the chooser has to still be alive to make.
        folderChooser_ = std::make_unique<juce::FileChooser>(
            "Please select a folder with SW presets...", currentDirectory_);
        folderChooser_->launchAsync(juce::FileBrowserComponent::openMode |
                                        juce::FileBrowserComponent::canSelectDirectories,
                                    [self = juce::Component::SafePointer<PresetBrowser>(this)](
                                        juce::FileChooser const &chooser) {
                                        auto const chosen(chooser.getResult());
                                        if (self && (chosen != juce::File()))
                                            self->setNewFolder(chosen);
                                    });
    }

    // Implementation note:
    //   Force the comment box to loose focus when a button is clicked (we do
    // not want the buttons to do so automatically because this messes up our
    // other related logic).
    //                                        (27.05.2010.) (Domagoj Saric)
    comment().moveKeyboardFocusToSibling(false);
}

void PresetBrowser::showFilenameEditBox(juce::String const &presetName, unsigned int atRow)
{
    LE_ASSERT(presetNameEditBox_.getParentComponent() == this);

    listBox_.scrollToEnsureRowIsOnscreen(atRow);

    juce::Rectangle<int> rowRect(
        listBox_.getRowPosition(atRow, true).translated(listBox_.getX(), listBox_.getY()));
    rowRect.setTop(rowRect.getY() - 3);
    rowRect.setBottom(rowRect.getBottom() + 2);
    //rowRect.setRight ( rowRect.getRight() - 1 );//...mrmlj...for testing...
    rowRect.setLeft(rowRect.getX() - 2);
    int const verticalOverflow(rowRect.getBottom() - listBox_.getBounds().getBottom());
    if (verticalOverflow > 0)
    {
        rowRect.setPosition(rowRect.getX(), rowRect.getY() - verticalOverflow);
        addOneRow(true);
        ++atRow;
    }

    presetNameEditBox_.setSelectAllWhenFocused(true);
    presetNameEditBox_.setBounds(rowRect);
    presetNameEditBox_.setText(presetName);
    presetNameEditBox_.setVisible(true);
    presetNameEditBox_.grabKeyboardFocus();
    presetNameEditBox_.setSelectAllWhenFocused(false);

    listBox_.updateContent();
    listBox_.scrollToEnsureRowIsOnscreen(atRow);
}

void PresetBrowser::hideFilenameEditBox()
{
    // Implementation note:
    //   The edit box should not be hidden if it lost focus due to a modal
    // component pop up (like a message box).
    //                                        (22.03.2010.) (Domagoj Saric)
    if (!presetNameEditBox_.isVisible() || presetNameEditBox_.getNumCurrentlyModalComponents() != 0)
        return;

    fadeOutComponent(presetNameEditBox_, 1, 200, false);

    listBox_.grabKeyboardFocus();
    this->addOneRow(false);
    listBox_.updateContent();
}

void PresetBrowser::askForOverwrite(std::function<void(bool)> onAnswer)
{
    GUI::warningOkCancelBox(_T( "File already exists." ), _T( "Overwrite?" ), std::move(onAnswer));
}

void PresetBrowser::setNewFolder(juce::File const &file)
{
    unsigned int const length(file.getFullPathName().length());
    bool const goToParent((file.getFullPathName()[length - 1] == '.') &&
                          (file.getFullPathName()[length - 2] == '.'));
    currentDirectory_ = goToParent ? file.getParentDirectory().getParentDirectory() : file;
    deselectAllRows();
    refresh();
    background().repaint();
}

SpectrumWorxEditor &PresetBrowser::editor()
{
    SpectrumWorxEditor *LE_RESTRICT const pEditor(&SpectrumWorxEditor::fromPresetBrowser(*this));
    return *pEditor;
}

SpectrumWorxEditor const &PresetBrowser::editor() const
{
    return const_cast<PresetBrowser &>(*this).editor();
}

void PresetBrowser::selectedRowsChanged(int const lastRowSelected)
{
    saveDirtyComment();

    if (ignoreSelectionChange_ || (lastRowSelected == -1))
        return;

    ignoreSelectionChange_ = true;
    presetSelectionChanged();
    ignoreSelectionChange_ = false;
}

int PresetBrowser::getNumRows() noexcept { return files_.size() + addOneRow_; }

void PresetBrowser::refresh()
{
    files_.clearQuick();

    std::error_code listingError;
    Item item;
    for (auto const &entry : std::filesystem::directory_iterator(
             currentDirectory_.getFullPathName().getCharPointer().getAddress(), listingError))
    {
        auto const fileName(entry.path().filename().string());
        if (fileName == "." || fileName == "..")
            continue;

        bool const isDirectory(entry.is_directory(listingError));
        auto const fullNameLength(static_cast<unsigned int>(fileName.size()));
        unsigned int const nameLength(
            isDirectory ? fullNameLength
                        : (fullNameLength - std::min(fullNameLength, presetExtensionLength)));

        if (isDirectory || std::_tcscmp(fileName.c_str() + nameLength, presetExtension) == 0)
        {
            item.isDirectory = isDirectory;
            item.name = juce::String(fileName.c_str(), nameLength);
            files_.add(item);
        }
    }

    std::sort(files_.begin(), files_.end());

    listBox_.updateContent();
}

void PresetBrowser::refreshAndSelectPreset(juce::String const &presetName)
{
    refresh();

    Item const *const pItem(findPreset(presetName));
    unsigned int const indexToSelect(static_cast<unsigned int>(pItem - files_.begin()));
    LE_ASSERT(indexToSelect < unsigned(files_.size()));

    comment().grabKeyboardFocus();

    listBox_.scrollToEnsureRowIsOnscreen(indexToSelect);
    ignoreSelectionChange_ = true;
    listBox_.selectRow(indexToSelect);
    ignoreSelectionChange_ = false;
    bool const enablePresetSaving(this->enablePresetSaving());
    delete_.setEnabled(enablePresetSaving);
    comment().setEnabled(enablePresetSaving);
}

void PresetBrowser::deselectAllRows()
{
    listBox_.deselectAllRows();
    // Implementation note:
    //   A single place to ensure that whenever the preset selection is cleared
    // the comment box is also cleared. In the special case when the selection
    // is only switched from one preset to another this will be taken care of
    // implicitly (the comment will be automatically set to the one of the newly
    // selected preset).
    //                                    (23.03.2010.) (Domagoj Saric)
    comment().clear();
    comment().setEnabled(false);

    save_.setEnabled(false);
}

void PresetBrowser::paint(juce::Graphics &graphics)
{
    BackgroundImage::paint(graphics);
    graphics.setColour(juce::Colours::white);
    graphics.drawFittedText(currentDirectory_.getFullPathName(), 9, 8, 162, 17,
                            juce::Justification::centredLeft, 1);
}

bool PresetBrowser::Item::operator==(Item const &other) const { return name == other.name; }

bool PresetBrowser::Item::operator<(Item const &other) const
{
    if (this->isDirectory != other.isDirectory)
        return this->isDirectory;
    return this->name < other.name;
}

//------------------------------------------------------------------------------
} // namespace GUI
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
