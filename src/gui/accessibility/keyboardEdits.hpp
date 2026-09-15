////////////////////////////////////////////////////////////////////////////////
///
/// \file keyboardEdits.hpp
/// -----------------------
///
///   What a key means to a focused control, as sst-jucegui and Six Sines map it.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef keyboardEdits_hpp__5C2B7E41_0D9A_4F63_B8E2_3A71C94D6F10
#define keyboardEdits_hpp__5C2B7E41_0D9A_4F63_B8E2_3A71C94D6F10
//------------------------------------------------------------------------------
#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>

namespace LE::SW::GUI::Accessibility
{

struct KeyEdit
{
    enum Action : std::uint8_t
    {
        none,
        trigger,
        increase,
        decrease,
        toMaximum,
        toMinimum,
        toDefault,
        openMenu,
        openEditor,
        previous,
        next
    };

    enum Modifier : std::uint8_t
    {
        coarse,
        fine,
        quantized
    };

    Action action{none};
    Modifier modifier{coarse};

    bool isNone() const { return action == none; }
    bool opensMenu() const { return (action == openMenu) || (action == openEditor); }
};

/// the windows context menu key, which sst-jucegui reads by number
constexpr int contextMenuKeyCode{93};

/// sst-jucegui's mapping, plus left and right, which it leaves to the widget
inline KeyEdit keyEditFor(juce::KeyPress const &key)
{
    auto const code(key.getKeyCode());
    auto const mods(key.getModifiers());
    auto const modifier(mods.isShiftDown()     ? KeyEdit::fine
                        : mods.isCommandDown() ? KeyEdit::quantized
                                               : KeyEdit::coarse);

    if (code == juce::KeyPress::upKey)
        return {KeyEdit::increase, modifier};
    if (code == juce::KeyPress::downKey)
        return {KeyEdit::decrease, modifier};
    if ((code == juce::KeyPress::F10Key) && mods.isShiftDown())
        return {KeyEdit::openMenu};
    if (code == contextMenuKeyCode)
        return {KeyEdit::openMenu};
    if ((code == juce::KeyPress::returnKey) && mods.isShiftDown())
        return {KeyEdit::openEditor};
    if (code == juce::KeyPress::homeKey)
        return {KeyEdit::toMaximum};
    if (code == juce::KeyPress::endKey)
        return {KeyEdit::toMinimum};
    if (code == juce::KeyPress::deleteKey)
        return {KeyEdit::toDefault};
    if (code == juce::KeyPress::returnKey)
        return {KeyEdit::trigger};
    if (code == juce::KeyPress::leftKey)
        return {KeyEdit::previous, modifier};
    if (code == juce::KeyPress::rightKey)
        return {KeyEdit::next, modifier};
    return {};
}

} // namespace LE::SW::GUI::Accessibility

#endif // keyboardEdits_hpp
