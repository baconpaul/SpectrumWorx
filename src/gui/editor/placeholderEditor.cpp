////////////////////////////////////////////////////////////////////////////////
///
/// \file placeholderEditor.cpp
/// ---------------------------
///
///   **Temporary. Every function here aborts.**
///
///   sw-dsp's factory instantiates Module::Impl<Effect> for all 57 effects, and
/// the vtables that emits pull in the whole module widget set, which calls into
/// SpectrumWorxEditor -- so nothing that links the engine links at all until the
/// editor does. And spectrumWorxEditor.cpp does not compile yet: it is welded to
/// the deleted 2016 SpectrumWorx VST2/AU class (effect().sample_, loadPreset,
/// ...), which is a port, not a fix-up.
///
///   This file breaks that circle so that the tests and the plugin keep
/// building while that port happens. It is the same kind of placeholder as
/// stubEditor.cpp, which the plugin has been showing since stage 1 -- and the
/// plugin still shows that, so none of these are reachable today.
///
///   The size of it is the honest measure of the coupling: nineteen entry points
/// the module widgets call, plus every virtual of the four panels the editor
/// owns by value, because defining ~SpectrumWorxEditor emits their vtables too.
///
///   **Replace, do not extend.** The real spectrumWorxEditor.cpp takes over once
/// it is unbound from the deleted plugin class, and this file is deleted whole.
/// Anything that starts wanting a *working* function here is a sign the editor
/// port is what should happen next instead.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "gui/editor/auxiliaryComponents.hpp"
#include "gui/editor/spectrumWorxEditor.hpp"
#include "gui/gui.hpp"

#include "le/utility/assert.hpp"
//------------------------------------------------------------------------------
namespace LE::SW::GUI
{
//------------------------------------------------------------------------------

namespace
{
/// Every one of these is reachable only from a real editor, which nothing
/// currently creates. Reaching one means the port has moved on and this file
/// should have gone with it -- so say so loudly rather than return a plausible
/// value and misbehave quietly.
[[noreturn]] void editorNotPortedYet(char const *const what)
{
    LE_ASSERT_MSG(false, what);
    std::abort();
}
} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
// The vtable and typeinfo, which polymorphicDowncast<SpectrumWorxEditor> needs.
//
// \note The destructor is the key function -- the first non-inline virtual the
// class declares, virtual by way of juce::Component -- so it decides which
// object file carries the vtable. Defining the three overrides below is not
// enough on its own; this is.
////////////////////////////////////////////////////////////////////////////////

/// \note Defining it pulls in the destructors of everything it owns by value,
/// which is why the three below are here too. That cascade is the honest shape
/// of the coupling, not an accident of the stubbing.
SpectrumWorxEditor::~SpectrumWorxEditor() = default;

PresetBrowser::~PresetBrowser() = default;
SpectrumWorxEditor::LFODisplay::~LFODisplay() = default;
SpectrumWorxEditor::Settings::~Settings() = default;

void SpectrumWorxEditor::mouseDown(juce::MouseEvent const &)
{
    editorNotPortedYet("SpectrumWorxEditor::mouseDown");
}

void SpectrumWorxEditor::paint(juce::Graphics &)
{
    editorNotPortedYet("SpectrumWorxEditor::paint");
}

void SpectrumWorxEditor::buttonClicked(juce::Button *)
{
    editorNotPortedYet("SpectrumWorxEditor::buttonClicked");
}

////////////////////////////////////////////////////////////////////////////////
// What the module layer calls.
////////////////////////////////////////////////////////////////////////////////

SpectrumWorxEditor &SpectrumWorxEditor::fromChild(juce::Component const &)
{
    editorNotPortedYet("SpectrumWorxEditor::fromChild");
}

Engine::Setup const &SpectrumWorxEditor::engineSetup() const
{
    editorNotPortedYet("SpectrumWorxEditor::engineSetup");
}

Utility::CriticalSectionLock SpectrumWorxEditor::getProcessingLock() const
{
    editorNotPortedYet("SpectrumWorxEditor::getProcessingLock");
}

void SpectrumWorxEditor::mainKnobDragStarted(std::uint8_t) const
{
    editorNotPortedYet("SpectrumWorxEditor::mainKnobDragStarted");
}

void SpectrumWorxEditor::mainKnobDragStopped(std::uint8_t) const
{
    editorNotPortedYet("SpectrumWorxEditor::mainKnobDragStopped");
}

void SpectrumWorxEditor::updateActiveControlValue()
{
    editorNotPortedYet("SpectrumWorxEditor::updateActiveControlValue");
}

void SpectrumWorxEditor::updateLFO(ModuleUI const &, std::uint8_t, std::uint8_t, float)
{
    editorNotPortedYet("SpectrumWorxEditor::updateLFO");
}

void SpectrumWorxEditor::moduleActivated()
{
    editorNotPortedYet("SpectrumWorxEditor::moduleActivated");
}

void SpectrumWorxEditor::moduleDeactivated()
{
    editorNotPortedYet("SpectrumWorxEditor::moduleDeactivated");
}

void SpectrumWorxEditor::moduleControlActivated(ModuleControlBase &, double, double, double)
{
    editorNotPortedYet("SpectrumWorxEditor::moduleControlActivated");
}

void SpectrumWorxEditor::moduleControlDectivated(ModuleControlBase const &)
{
    editorNotPortedYet("SpectrumWorxEditor::moduleControlDectivated");
}

void SpectrumWorxEditor::moduleDrag(ModuleUI &, juce::MouseEvent const &)
{
    editorNotPortedYet("SpectrumWorxEditor::moduleDrag");
}

void SpectrumWorxEditor::moduleDragEnd(ModuleUI &, juce::MouseEvent const &)
{
    editorNotPortedYet("SpectrumWorxEditor::moduleDragEnd");
}

void SpectrumWorxEditor::removeModule(ModuleUI &)
{
    editorNotPortedYet("SpectrumWorxEditor::removeModule");
}

void SpectrumWorxEditor::updateModuleParameterAndNotifyHost(ModuleUI &, std::uint_fast8_t,
                                                            float) const
{
    editorNotPortedYet("SpectrumWorxEditor::updateModuleParameterAndNotifyHost");
}

void SpectrumWorxEditor::Settings::comboBoxValueChanged(ComboBox const &)
{
    editorNotPortedYet("SpectrumWorxEditor::Settings::comboBoxValueChanged");
}

ModuleControlBase &SharedModuleControls::controlForParameter(std::uint8_t)
{
    editorNotPortedYet("SharedModuleControls::controlForParameter");
}

/// \note focusLost is SharedModuleControls' key function, so these two are what
/// emit its vtable.
void SharedModuleControls::focusLost(FocusChangeType)
{
    editorNotPortedYet("SharedModuleControls::focusLost");
}

void SharedModuleControls::focusOfChildComponentChanged(FocusChangeType)
{
    editorNotPortedYet("SharedModuleControls::focusOfChildComponentChanged");
}

////////////////////////////////////////////////////////////////////////////////
// The editor's owned panels.
//
// \note Nothing the tests do calls any of these. They are here because
// ~SpectrumWorxEditor destroys these panels by value, which emits their vtables,
// which need every virtual they declare. The size of this section is the measure
// of how much of the editor a headless test has to drag in behind one factory
// instantiation -- and the reason to replace this file rather than extend it.
////////////////////////////////////////////////////////////////////////////////

SpectrumWorxEditor &PresetBrowser::editor() { editorNotPortedYet("PresetBrowser::editor"); }
void PresetBrowser::paint(juce::Graphics &) { editorNotPortedYet("PresetBrowser::paint"); }
void PresetBrowser::buttonClicked(juce::Button *)
{
    editorNotPortedYet("PresetBrowser::buttonClicked");
}
void PresetBrowser::textEditorTextChanged(juce::TextEditor &)
{
    editorNotPortedYet("PresetBrowser::textEditorTextChanged");
}
void PresetBrowser::textEditorReturnKeyPressed(juce::TextEditor &)
{
    editorNotPortedYet("PresetBrowser::textEditorReturnKeyPressed");
}
void PresetBrowser::textEditorEscapeKeyPressed(juce::TextEditor &)
{
    editorNotPortedYet("PresetBrowser::textEditorEscapeKeyPressed");
}
void PresetBrowser::textEditorFocusLost(juce::TextEditor &)
{
    editorNotPortedYet("PresetBrowser::textEditorFocusLost");
}
int PresetBrowser::getNumRows() noexcept { editorNotPortedYet("PresetBrowser::getNumRows"); }
void PresetBrowser::paintListBoxItem(int, juce::Graphics &, int, int, bool)
{
    editorNotPortedYet("PresetBrowser::paintListBoxItem");
}
void PresetBrowser::listBoxItemDoubleClicked(int, juce::MouseEvent const &)
{
    editorNotPortedYet("PresetBrowser::listBoxItemDoubleClicked");
}
void PresetBrowser::deleteKeyPressed(int) noexcept
{
    editorNotPortedYet("PresetBrowser::deleteKeyPressed");
}
void PresetBrowser::returnKeyPressed(int) noexcept
{
    editorNotPortedYet("PresetBrowser::returnKeyPressed");
}
void PresetBrowser::selectedRowsChanged(int)
{
    editorNotPortedYet("PresetBrowser::selectedRowsChanged");
}

void SpectrumWorxEditor::LFODisplay::paint(juce::Graphics &)
{
    editorNotPortedYet("LFODisplay::paint");
}
void SpectrumWorxEditor::LFODisplay::buttonClicked(juce::Button *)
{
    editorNotPortedYet("LFODisplay::buttonClicked");
}
void SpectrumWorxEditor::LFODisplay::sliderValueChanged(juce::Slider *) noexcept
{
    editorNotPortedYet("LFODisplay::sliderValueChanged");
}

SpectrumWorxEditor &SpectrumWorxEditor::Settings::editor()
{
    editorNotPortedYet("Settings::editor");
}
void SpectrumWorxEditor::Settings::buttonClicked(juce::Button *)
{
    editorNotPortedYet("Settings::buttonClicked");
}
void SpectrumWorxEditor::Settings::sliderValueChanged(juce::Slider *) noexcept
{
    editorNotPortedYet("Settings::sliderValueChanged");
}
juce::TabBarButton *SpectrumWorxEditor::Settings::createTabButton(juce::String const &, int)
{
    editorNotPortedYet("Settings::createTabButton");
}

void SpectrumWorxEditor::Settings::EnginePage::paint(juce::Graphics &)
{
    editorNotPortedYet("Settings::EnginePage::paint");
}
void SpectrumWorxEditor::Settings::InterfacePage::paint(juce::Graphics &)
{
    editorNotPortedYet("Settings::InterfacePage::paint");
}
void SpectrumWorxEditor::Settings::AboutPage::paint(juce::Graphics &)
{
    editorNotPortedYet("Settings::AboutPage::paint");
}

////////////////////////////////////////////////////////////////////////////////

/// \note Lives in spectrumWorxEditor.cpp for the same reason the rest of this
/// file exists: it instantiates globalParameterChanged<>, which needs the
/// complete SpectrumWorx.
void EditorKnob::valueChanged() noexcept { editorNotPortedYet("EditorKnob::valueChanged"); }

//------------------------------------------------------------------------------
} // namespace LE::SW::GUI
//------------------------------------------------------------------------------
