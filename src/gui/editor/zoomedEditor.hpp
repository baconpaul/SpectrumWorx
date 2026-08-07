////////////////////////////////////////////////////////////////////////////////
///
/// \file zoomedEditor.hpp
/// ----------------------
///
///   The editor, drawn bigger than the skin it is laid out in.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef zoomedEditor_hpp__F1C7A0B4_3E52_4A16_9D0B_7C5E28A4F631
#define zoomedEditor_hpp__F1C7A0B4_3E52_4A16_9D0B_7C5E28A4F631
//------------------------------------------------------------------------------
#include "spectrumWorxEditor.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <utility>
//------------------------------------------------------------------------------
namespace LE::SW::GUI
{

////////////////////////////////////////////////////////////////////////////////
///
/// \class ZoomedEditor
///
////////////////////////////////////////////////////////////////////////////////

/// \brief Owns a SpectrumWorxEditor and shows it scaled.
///
///   Every offset in SpectrumWorxEditor is a pixel position in a 563 x 376
/// bitmap and none of them move: the editor lays itself out in skin pixels, as
/// it always has, and this draws it through one juce::AffineTransform.
///
/// \note Why a component in between, rather than a transform on the editor
/// itself. The scale has to be applied exactly once, and the size handed to the
/// host has to be the scaled one -- and those are two different components'
/// jobs. Putting both on the editor got the scale applied twice (a factor of
/// 1.5 needed sqrt(1.5) written down to look right), because an embedder that
/// unwinds a child's transform when it sizes it -- the shim's holder does
/// exactly this in its resized(), clap_juce_shim_impl.cpp -- then divides by a
/// transform that is also still applied at paint time. Split in two, the
/// question does not arise: this component reports size and carries none, the
/// editor carries the transform and reports the skin's size, and no embedder
/// can conflate them.
///
/// \note It also means the zoom is opt-in at the point of construction. The
/// test harness and anything else that wants the editor at 1:1 builds a
/// SpectrumWorxEditor directly and is unaffected by any of this.
class ZoomedEditor final : public juce::Component
{
  public:
    /// What the plugin shows. Not a constant of the editor, because the editor
    /// does not know it is being scaled.
    static constexpr float defaultZoom{1.5f};

    static constexpr int scaled(int const skinPixels, float const zoom = defaultZoom)
    {
        return static_cast<int>(static_cast<float>(skinPixels) * zoom + 0.5f);
    }

    ZoomedEditor(std::unique_ptr<SpectrumWorxEditor> editor, float const zoom = defaultZoom)
        : pEditor_(std::move(editor)), zoom_(zoom)
    {
        LE_ASSERT(pEditor_ != nullptr);
        setOpaque(true);
        addAndMakeVisible(*pEditor_);
        pEditor_->setTopLeftPosition(0, 0);
        pEditor_->setTransform(juce::AffineTransform::scale(zoom_));
        matchEditor();
    }

    SpectrumWorxEditor &editor() const { return *pEditor_; }
    float zoom() const { return zoom_; }

    /// \note Black, for the half pixel that rounding can leave down the right
    /// and bottom edges when the scaled size is not integral. The same reason
    /// the shim paints its own holder black.
    void paint(juce::Graphics &graphics) override { graphics.fillAll(juce::Colours::black); }

  private:
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note The editor resizes *itself* when a panel takes a column of its
    /// own, so this follows it rather than the other way round: the editor's
    /// size is the authority and this is its scaled shadow. Nothing here sets
    /// the editor's bounds, which is what keeps it from oscillating -- and
    /// resized() deliberately does not either, because an embedder handing this
    /// component a size is not an instruction to relay out the skin. The editor
    /// is not user-resizable (the shim is told setResizable(false)).
    ///
    ////////////////////////////////////////////////////////////////////////////
    void childBoundsChanged(juce::Component *const child) override
    {
        if (child == pEditor_.get())
            matchEditor();
    }

    void matchEditor()
    {
        setSize(scaled(pEditor_->getWidth(), zoom_), scaled(pEditor_->getHeight(), zoom_));
    }

    std::unique_ptr<SpectrumWorxEditor> pEditor_;
    float const zoom_;
}; // class ZoomedEditor

} // namespace LE::SW::GUI

//------------------------------------------------------------------------------
#endif // zoomedEditor_hpp
