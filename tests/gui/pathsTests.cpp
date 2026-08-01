////////////////////////////////////////////////////////////////////////////////
///
/// \file pathsTests.cpp
/// --------------------
///
///   Where the user's presets live, answered without being initialised first.
///
///   This exists because of a bug it would have caught outright. `rootPath()`
/// and `presetsFolder()` were half of a two-phase initialisation whose other
/// half, `initializePaths()`, nothing called -- its only caller had been the
/// 2016 VST2/AU plugin class that the CLAP replaced. Both getters asserted
/// "Not initialized." and neither was reachable while `presetBrowser.cpp` was in
/// no target, so nothing noticed. Stage 8 put the browser in a target and the
/// presets button asserted on the first press.
///
/// \note Deliberately no side effect. Asking where the presets go must not
/// create a directory in someone's Documents folder, which is why creating it is
/// `createUserPresetsFolder()` and why this test does not call it.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "gui/gui.hpp"

#include <catch2/catch_test_macros.hpp>
//------------------------------------------------------------------------------
namespace
{
//------------------------------------------------------------------------------

using namespace LE::SW;

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

TEST_CASE("The user preset paths answer without an initialisation step", "[paths]")
{
    /// \note The first call in the process, and it has to work. Anything that
    /// has to run before it is the bug this file is about.
    auto const &root(GUI::rootPath());

    INFO("root " << root.getFullPathName());
    CHECK(root != juce::File());
    CHECK(root.isAbsolutePath(root.getFullPathName()));
    CHECK(root.getFileName() == "SpectrumWorx");

    auto const &presets(GUI::presetsFolder());

    INFO("presets " << presets.getFullPathName());
    CHECK(presets != juce::File());
    CHECK(presets.isAChildOf(root));

    /// \note Idempotent, and the same object each time -- the browser holds the
    /// reference across its own lifetime and writes the most-recently-used
    /// folder back into it.
    CHECK(&GUI::rootPath() == &root);
    CHECK(&GUI::presetsFolder() == &presets);

    /// \note Named rather than probed on disk, and that is the point: this test
    /// says where the presets go without asking the filesystem anything, so it
    /// neither creates a directory in the Documents folder of whoever runs it
    /// nor passes or fails depending on whether one is already there.
    CHECK(presets.getFileName() == "Presets");
    CHECK(presets.getParentDirectory() == root);
}
