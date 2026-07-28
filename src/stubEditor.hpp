////////////////////////////////////////////////////////////////////////////////
///
/// \file stubEditor.hpp
/// -------------------
///
/// The stage 1 editor: a rectangle, a title, and the two buttons that let a
/// human drive the dynamic-parameter path from inside a DAW - swap the effect
/// in a slot, and fire a rescan on its own. Replaced wholesale in stage 6.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef stubEditor_hpp__7C2E5A90_4B31_4F8D_A0E6_D91F73C4B285
#define stubEditor_hpp__7C2E5A90_4B31_4F8D_A0E6_D91F73C4B285
//------------------------------------------------------------------------------
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>
//------------------------------------------------------------------------------
namespace LE::SW
{
//------------------------------------------------------------------------------

class SpectrumWorxCLAP;

class StubEditor final : public juce::Component
{
  public:
    explicit StubEditor(SpectrumWorxCLAP &);
    ~StubEditor() override;

    void paint(juce::Graphics &) override;
    void resized() override;

  private:
    void refreshLabels();

    SpectrumWorxCLAP &plugin_;

    std::array<std::unique_ptr<juce::TextButton>, 5> slotButtons_;
    std::unique_ptr<juce::TextButton> rescanButton_;
}; // class StubEditor

//------------------------------------------------------------------------------
} // namespace LE::SW
//------------------------------------------------------------------------------
#endif // stubEditor_hpp
