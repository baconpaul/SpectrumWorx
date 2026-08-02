////////////////////////////////////////////////////////////////////////////////
///
/// \file editorModuleInitialiser.hpp
/// --------------------------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef editorModuleInitialiser_hpp__1C7B4A02_58D6_4E93_8F35_0B6A2D7419EC
#define editorModuleInitialiser_hpp__1C7B4A02_58D6_4E93_8F35_0B6A2D7419EC
//------------------------------------------------------------------------------
#include "core/modules/moduleDSPAndGUI.hpp"
#include "core/spectrumWorxCore.hpp"
#include "gui/editor/spectrumWorxEditor.hpp"

#include "le/utility/cstdint.hpp"
#include "le/utility/intrusivePtr.hpp"
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------
namespace GUI
{
class SpectrumWorxEditor;
} // namespace GUI

////////////////////////////////////////////////////////////////////////////////
///
/// \struct EditorModuleInitialiser
///
/// \brief The DSP initialiser, plus the module's UI region.
///
///   Filling a slot has two halves: give the module its buffers, and give it a
/// place on screen. `SpectrumWorxCore::ModuleInitialiser` is only the first --
/// it lives in `sw-dsp`, and `Module::createGUI` is in `sw-gui`, which links
/// `sw-dsp` and not the other way round. So the second half cannot live down
/// there, and this is where it goes instead.
///
/// \note The 2016 code had both halves in one place because the initialiser was
/// a member of the VST2/AU host class, which sat above the GUI. The port's
/// equivalent sits below it, which is the whole reason this type exists rather
/// than an editor pointer on `SpectrumWorxCore`. See gui.cmake's note on
/// moduleDSPAndGUI.cpp, which was moved out of `sw-dsp` for the same reason.
///
/// \note `pEditor` may be null -- no editor open, which is the normal case for a
/// plugin being driven by automation with its window shut. The DSP half still
/// runs; there is simply nothing to draw into.
///
////////////////////////////////////////////////////////////////////////////////

struct EditorModuleInitialiser
{
    /// The chain reaches for this typedef; see AutomatedModuleChain::setParameter.
    using Module = SpectrumWorxCore::ModuleInitialiser::Module;

    bool operator()(Module &module, std::uint8_t const moduleIndex) const
    {
        if (!dsp(module, moduleIndex))
            return false;

        if (!pEditor)
            return true;

        /// \note The editor decides whether that means building a strip or moving
        /// one it already has; see SpectrumWorxEditor::createModuleRegion(). This
        /// used to reach into the module for the strip it owned.
        pEditor->createModuleRegion(LE::Utility::IntrusivePtr<Module>(&module), moduleIndex);
        return true;
    }

    SpectrumWorxCore::ModuleInitialiser dsp;
    GUI::SpectrumWorxEditor *pEditor;
}; // struct EditorModuleInitialiser

//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // editorModuleInitialiser_hpp
