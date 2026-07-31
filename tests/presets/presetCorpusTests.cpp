////////////////////////////////////////////////////////////////////////////////
///
/// \file presetCorpusTests.cpp
/// ---------------------------
///
///   Every factory preset, loaded into a real engine, snapshotted.
///
///   Stage 8's "done when" says an unmodified 2016-era preset file still loads.
/// There are 303 of them committed under assets/presets, written between 2009
/// and 2016 by a plugin that no longer exists, and they are the only sample of
/// the format nobody can rewrite. This walks all of them.
///
///   It exists to be the backstop for 8.1, which replaces the XML parser. A
/// parser swap that loses an attribute, mangles an entity or reads a float one
/// ulp differently changes what a preset sounds like and nothing else in the
/// suite would notice: the goldens render effects at their *defaults*, and the
/// parameter table snapshot never opens a file.
///
///   One row per preset rather than one per parameter -- the full dump is
/// ~8000 lines, and a diff nobody can read is a diff nobody reads. The row
/// carries the module count and the effect names in the clear, so the common
/// failures (a preset losing a module, or loading the wrong effect) name
/// themselves; everything finer is behind a hash of the canonical dump.
/// SW_PRESET_DUMP=<substring> prints those dumps in full, which is the first
/// thing to reach for when a hash moves.
///
///   Values are formatted to six significant figures before hashing. They come
/// from decimal text in the file by way of strtof, so they are the same number
/// everywhere; six figures is enough to catch a parameter that moved and not so
/// many that a legitimate difference in the last bit of a conversion turns the
/// file red on another platform.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "presets/presetHarness.hpp"

#include "core/modules/factory.hpp"

/// \note Not sorted with the block above: finalImplementations names
/// GUI::ModuleUI, which moduleDSPAndGUI is what defines. loadPreset() is a
/// template over the consumer and downcasts a chain node to SW::Module, so this
/// translation unit needs the complete type.
#include "core/modules/moduleDSPAndGUI.hpp"
#include "core/modules/finalImplementations.hpp"

#include "le/spectrumworx/presetFile.hpp"
#include "le/spectrumworx/presets.hpp"

#include <juce_core/juce_core.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
//------------------------------------------------------------------------------
namespace
{
//------------------------------------------------------------------------------

using namespace LE;
using namespace LE::SW;

using SWTest::digest;
using SWTest::dump;
using SWTest::Loaded;
using SWTest::PresetConsumer;
using SWTest::ScopedProblemCounter;

std::string snapshotPath() { return std::string(SW_PRESET_SNAPSHOT_DIR) + "/presetCorpus.txt"; }

//------------------------------------------------------------------------------
// The corpus
//------------------------------------------------------------------------------

/// `<bank>/<preset>`, so the key survives a change of checkout path and sorts
/// the way the browser shows them.
std::vector<std::pair<std::string, std::filesystem::path>> corpus()
{
    std::filesystem::path const root(SW_PRESET_DATA_DIR);
    std::vector<std::pair<std::string, std::filesystem::path>> found;

    std::error_code error;
    for (auto const &entry : std::filesystem::recursive_directory_iterator(root, error))
    {
        if (!entry.is_regular_file() || (entry.path().extension() != ".swp"))
            continue;
        found.emplace_back(std::filesystem::relative(entry.path(), root, error).generic_string(),
                           entry.path());
    }

    std::ranges::sort(found, {}, &std::pair<std::string, std::filesystem::path>::first);
    return found;
}

/// \note One engine per preset, not one reused across all 303. Loading a preset
/// *merges* into the current chain -- loadModuleChain() looks for a module
/// already holding the same effect and moves it across rather than building a
/// new one -- so a reused engine would make every row depend on the row before
/// it, and a snapshot in which row 200 changes when row 199 does is not a
/// snapshot of row 200.
Loaded load(std::filesystem::path const &file, bool &succeeded)
{
    SWTest::Engine engine;
    engine.setNumberOfChannels(2, 2);
    engine.setSampleRate(48000);
    engine.setBlockSize(512);
    REQUIRE(engine.initialise());

    succeeded = false;

    auto const presetData(readPresetFile(juce::File(file.string())));
    if (!presetData)
        return {};

    SWTest::clearPresetProblems();
    {
        ScopedProblemCounter const counting;
        if (!LE::SW::loadPreset(presetData.get(), true /*ignore external samples*/, nullptr,
                                PresetConsumer{engine}))
            return {};
    }

    /// \note No effect a preset names may be unknown -- the 57 shipped are the
    /// 57 these banks were written against, and one going missing is a build
    /// that silently dropped an effect rather than a preset that is wrong.
    CHECK(SWTest::presetProblems().unknownEffect == 0);

    succeeded = true;
    auto loaded(dump(engine));
    loaded.missing = SWTest::presetProblems().missingParameter;
    return loaded;
}

using Table = std::map<std::string, std::string>;

Table readTable()
{
    Table table;
    std::ifstream stream(snapshotPath());
    std::string line;
    while (std::getline(stream, line))
    {
        if (line.empty() || (line.front() == '#'))
            continue;
        auto const separator(line.find(" | "));
        REQUIRE(separator != std::string::npos);
        table.emplace(line.substr(0, separator), line.substr(separator + 3));
    }
    return table;
}

void writeTable(Table const &table)
{
    std::ofstream file(snapshotPath(), std::ios::trunc);
    file << "# SpectrumWorx factory preset corpus -- generated, do not hand edit.\n"
            "# Regenerate with SW_PRESET_CORPUS_UPDATE=1 ./sw-tests \"[preset-corpus]\"\n"
            "#\n"
            "# <bank>/<preset> | <modules> | <effects> | <parameters> | <missing> | <digest>\n"
            "#     what loading that file into a fresh engine produces. <missing> counts\n"
            "#     parameters the effect has and the preset never mentions -- normal for\n"
            "#     a 2009-2011 file against a 2016 effect, and a number that must not\n"
            "#     grow. The digest is FNV-1a over the full parameter dump, values at six\n"
            "#     significant figures; SW_PRESET_DUMP=<substring> prints those dumps.\n";
    for (auto const &[key, row] : table)
        file << key << " | " << row << '\n';
}

bool environmentFlag(char const *const name)
{
    auto const *const value(std::getenv(name));
    return value && (*value != '\0') && (*value != '0');
}

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

TEST_CASE("Every factory preset loads and produces the committed state", "[preset-corpus]")
{
    auto const files(corpus());
    INFO("preset directory " << SW_PRESET_DATA_DIR);
    REQUIRE(files.size() >= 300); // 303 as committed; an empty sweep is a failure, not a pass

    auto const *const dumpFilter(std::getenv("SW_PRESET_DUMP"));

    Table table;
    for (auto const &[key, path] : files)
    {
        INFO("preset " << key);

        bool succeeded{false};
        auto const loaded(load(path, succeeded));
        REQUIRE(succeeded); // stage 8: an unmodified 2016-era preset file still loads

        if (dumpFilter && (key.find(dumpFilter) != std::string::npos))
            WARN(key << ":\n" << loaded.text);

        std::array<char, 32> digestText{};
        std::snprintf(digestText.data(), digestText.size(), "%016llx",
                      static_cast<unsigned long long>(digest(loaded.text)));

        table.emplace(key, std::to_string(loaded.modules) + " | " + loaded.effects + " | " +
                               std::to_string(loaded.parameters) + " | " +
                               std::to_string(loaded.missing) + " | " + digestText.data());
    }

    if (environmentFlag("SW_PRESET_CORPUS_UPDATE"))
    {
        writeTable(table);
        WARN("SW_PRESET_CORPUS_UPDATE was set: " << table.size()
                                                 << " rows rewritten. Read the diff before "
                                                    "committing it.");
        return;
    }

    auto const expected(readTable());
    REQUIRE_FALSE(expected.empty()); // an absent or empty file is a failure, not a pass

    for (auto const &[key, row] : expected)
    {
        auto const found(table.find(key));
        INFO("preset " << key);
        REQUIRE(found != table.end()); // a preset that disappeared
        CHECK(found->second == row);
    }

    for (auto const &[key, row] : table)
    {
        INFO("preset " << key << " = " << row);
        CHECK(expected.find(key) != expected.end()); // a preset that appeared
    }

    CHECK(table.size() == expected.size());
}
