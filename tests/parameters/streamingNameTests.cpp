////////////////////////////////////////////////////////////////////////////////
///
/// \file streamingNameTests.cpp
/// ----------------------------
///
///   Every string this build will write into a preset or into session state,
/// committed, so that changing one is a decision rather than an accident.
///
///   Until 08.2026 a parameter had one name and it did both jobs: the editor
/// showed it and the preset writer keyed on it. Renaming a knob therefore
/// re-keyed every file that had ever named it, and the file did not complain --
/// it loaded a default instead, silently, for that one parameter. The same was
/// true of an effect's title, which is what a preset calls its modules.
///
///   `Parameters::StreamingName` and `Effects::EffectStreamingName` split the
/// two, and both default to the display string, because the display string is
/// exactly what the 303 factory presets contain. That default is what makes the
/// split free; this file is what makes it safe. A row here moving means a file
/// on someone's disk stops being understood.
///
///   Regenerate with SW_STREAMING_NAMES_UPDATE=1, and then do not commit it
/// until you can say which files you are willing to break. The legitimate
/// reasons are two: an effect or a parameter was added, or one was pinned with
/// STREAMING_NAME / LE_SW_EFFECT_STREAMING_NAME -- and a pin, done right, moves
/// nothing here at all. That is its point.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "goldens/engineHarness.hpp"

#include "core/modules/factory.hpp"

/// \note Not sorted with the block above, and it matters: finalImplementations
/// names GUI::ModuleUI, which moduleDSPAndGUI is what defines.
#include "core/modules/moduleDSPAndGUI.hpp"
#include "core/modules/finalImplementations.hpp"

#include "le/parameters/lfoImpl.hpp"
#include "le/parameters/parametersUtilities.hpp"
#include "le/parameters/runtimeInformation.hpp"
#include "le/parameters/uiElements.hpp"
#include "le/spectrumworx/effects/configuration/constants.hpp"
#include "le/spectrumworx/effects/configuration/effectNames.hpp"
#include "le/spectrumworx/engine/moduleParameters.hpp"
#include "le/spectrumworx/engine/parameters.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
//------------------------------------------------------------------------------
namespace
{
//------------------------------------------------------------------------------

using namespace LE;
using namespace LE::SW;

namespace Effects = LE::SW::Effects;

std::string tablePath() { return std::string(SW_PARAMETER_DATA_DIR) + "/streamingNames.txt"; }

/// \note '|' separates the columns and streaming names contain spaces, brackets
/// and an exclamation mark, so the key gets underscores rather than the values
/// getting quotes -- the same trick parameterTableTests.cpp uses, for the same
/// reason.
std::string sanitised(std::string_view const text)
{
    std::string result;
    for (auto const character : text)
        result += (character == ' ') ? '_' : character;
    return result;
}

using Table = std::map<std::string, std::string>;

/// \brief Collects `streamingName<Parameter>()` across a parameter tuple.
///
/// \note A functor rather than a loop because that is the only way to reach a
/// parameter's *type*, and the streaming name is a property of the type. The
/// globals and the LFO have no `RuntimeInformation` table to read instead --
/// only the per-effect parameters do.
struct StreamingNameCollector
{
    using result_type = void;

    Table &table;
    std::string_view prefix;
    unsigned index{0};

    template <class Parameter> result_type operator()(Parameter const &)
    {
        std::array<char, 32> indexText{};
        std::snprintf(indexText.data(), indexText.size(), "%02u", index++);
        table.emplace(std::string(prefix) + "/" + indexText.data(),
                      sanitised(Parameters::streamingName<Parameter>()));
    }
}; // struct StreamingNameCollector

/// \brief An initialised engine with a Program, the shape every engine-level
/// test here uses -- SpectrumWorxCore cannot be instantiated alone.
class Fixture
{
  public:
    Fixture()
    {
        engine_.setNumberOfChannels(2, 2);
        engine_.setSampleRate(48000);
        engine_.setBlockSize(512);
        REQUIRE(engine_.initialise());
    }

    void insert(std::uint8_t const slot, std::uint8_t const effectIndex)
    {
        REQUIRE(engine_.program()
                    .moduleChain()
                    .setParameter(slot, static_cast<std::int8_t>(effectIndex),
                                  engine_.moduleInitialiser())
                    .second == static_cast<std::int8_t>(effectIndex));
    }

    Engine::ModuleParameters const &moduleInSlot(std::uint8_t const slot)
    {
        Engine::ModuleParameters const *pFound{nullptr};
        std::uint8_t index{0};
        engine_.program().moduleChain().forEach<Engine::ModuleParameters>(
            [&](Engine::ModuleParameters const &module) {
                if (index++ == slot)
                    pFound = &module;
            });
        REQUIRE(pFound != nullptr);
        return *pFound;
    }

    GlobalParameters::Parameters const &globals() { return engine_.program().parameters(); }

  private:
    SWTest::Engine engine_;
}; // class Fixture

/// \brief One row per effect: what a preset calls it, and what a user sees.
///
/// \note Keyed by index, which effectsList.cmake holds stable across editions by
/// design and effectsListTests.cpp pins independently. Both names are columns so
/// that a retitling shows up as a moved *title* beside an unmoved key -- which is
/// the whole shape this mechanism exists to produce.
void collectEffects(Table &table)
{
    for (std::uint8_t effect(0); effect < Effects::Constants::numberOfEffects; ++effect)
    {
        std::array<char, 32> key{};
        std::snprintf(key.data(), key.size(), "effect/%02u", effect);
        table.emplace(key.data(), sanitised(Effects::effectStreamingName(effect)) + " | " +
                                      sanitised(Effects::effectName(effect)));
    }
}

/// \brief Every effect's parameters, keyed by the effect's *streaming* name, so
/// that a retitled effect does not drag its parameter rows with it.
///
/// \note Every effect through slot 0 in turn rather than one per slot: a
/// module's parameter table is a property of the effect, so a difference between
/// two rows cannot then be a difference between two slots.
void collectEffectParameters(Table &table)
{
    Fixture fixture;
    for (std::uint8_t effect(0); effect < Effects::Constants::numberOfEffects; ++effect)
    {
        fixture.insert(0, effect);

        auto const &module(fixture.moduleInSlot(0));
        std::string const effectKey(sanitised(Effects::effectStreamingName(effect)));

        for (std::uint8_t index(0); index < module.numberOfParameters(); ++index)
        {
            std::array<char, 32> indexText{};
            std::snprintf(indexText.data(), indexText.size(), "%02u", index);
            table.emplace("param/" + effectKey + "/" + indexText.data(),
                          sanitised(module.parameterInfo(index).streamingName));
        }
    }
}

/// \brief The globals and the LFO -- the two tuples with no per-effect table
/// behind them, and between them everything a `<Global>` node and an LFO
/// attribute set are made of.
void collectGlobalsAndLFO(Table &table)
{
    Fixture fixture;
    {
        StreamingNameCollector collector{table, "global"};
        LE::Parameters::forEach(fixture.globals(), collector);
    }
    {
        Parameters::LFOImpl const lfo;
        StreamingNameCollector collector{table, "lfo"};
        LE::Parameters::forEach(lfo.parameters(), collector);
    }
}

Table currentTable()
{
    Table table;
    collectEffects(table);
    collectEffectParameters(table);
    collectGlobalsAndLFO(table);
    return table;
}

Table readTable()
{
    Table table;
    std::ifstream stream(tablePath());
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
    std::ofstream file(tablePath(), std::ios::trunc);
    file << "# SpectrumWorx streaming names -- generated, do not hand edit.\n"
            "# Regenerate with SW_STREAMING_NAMES_UPDATE=1 ./sw-tests \"[streaming-names]\"\n"
            "#\n"
            "#   These are the strings that go *into a file*: the keys a preset and a\n"
            "#   session state are written under, and matched by on the way back in. They\n"
            "#   are not the strings a user sees -- see parameterTable.txt for those.\n"
            "#\n"
            "#   A row that moves is a file on someone's disk that stops being understood:\n"
            "#   the parameter it names loads its default instead, silently. Renaming a\n"
            "#   knob or retitling an effect must therefore be paired with a\n"
            "#   STREAMING_NAME / LE_SW_EFFECT_STREAMING_NAME pin, which holds the file key\n"
            "#   still and leaves this table alone.\n"
            "#\n"
            "# effect/<index> | <streaming name> | <title>\n"
            "#     what a preset calls the effect, and what a user sees. Equal unless the\n"
            "#     title has moved since presets were written naming it.\n"
            "#\n"
            "# param/<effect streaming name>/<index> | <streaming name>\n"
            "# global/<index> | <streaming name>\n"
            "# lfo/<index> | <streaming name>\n"
            "#     the module, global and LFO parameter keys. Spaces are written as\n"
            "#     underscores so that ' | ' stays unambiguous as a column separator.\n";
    for (auto const &[key, row] : table)
        file << key << " | " << row << '\n';
}

bool updateRequested()
{
    auto const *const requested(std::getenv("SW_STREAMING_NAMES_UPDATE"));
    return requested && (*requested != '\0') && (*requested != '0');
}

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

TEST_CASE("Every name that reaches a file is the one that has always reached it",
          "[streaming-names]")
{
    auto const table(currentTable());
    REQUIRE_FALSE(table.empty());

    if (updateRequested())
    {
        writeTable(table);
        WARN("SW_STREAMING_NAMES_UPDATE was set: "
             << table.size()
             << " rows rewritten. Every row that moved is a preset that will not "
                "load the same way. Read the diff.");
        return;
    }

    auto const expected(readTable());
    REQUIRE_FALSE(expected.empty()); // an absent or empty file is a failure, not a pass

    for (auto const &[key, row] : table)
    {
        auto const found(expected.find(key));
        INFO("streaming name " << key);
        REQUIRE(found != expected.end()); // a name this build writes and the snapshot does not
        CHECK(found->second == row);
    }

    for (auto const &[key, row] : expected)
    {
        INFO("streaming name " << key << " = " << row);
        CHECK(table.contains(key)); // a name the snapshot has and this build no longer writes
    }
}

/// \note Separate from the snapshot because it is a different claim. The
/// snapshot says the names have not changed; this says the mechanism behind them
/// is still wired up at all. `StreamingName`'s default is a null pointer meaning
/// "the display name serves", resolved in `streamingName()` -- so a fallback
/// that came undone would show up here as a null or an empty key, which is a
/// preset written with no name on half its parameters rather than a wrong one.
TEST_CASE("No name that reaches a file is empty", "[streaming-names]")
{
    Fixture fixture;

    for (std::uint8_t effect(0); effect < Effects::Constants::numberOfEffects; ++effect)
    {
        INFO("effect " << unsigned(effect));
        REQUIRE(Effects::effectStreamingName(effect) != nullptr);
        CHECK_FALSE(std::string_view(Effects::effectStreamingName(effect)).empty());

        /// \note And it round-trips: the writer mangles the streaming name into
        /// an element name and the reader mangles every candidate to match it,
        /// so a name that does not find its way back is a module that silently
        /// vanishes from a preset.
        CHECK(Effects::effectIndexFromStreamingName(Effects::effectStreamingName(effect)) ==
              static_cast<std::int8_t>(effect));

        fixture.insert(0, effect);
        auto const &module(fixture.moduleInSlot(0));
        for (std::uint8_t index(0); index < module.numberOfParameters(); ++index)
        {
            INFO("parameter " << unsigned(index));
            REQUIRE(module.parameterInfo(index).streamingName != nullptr);
            CHECK_FALSE(std::string_view(module.parameterInfo(index).streamingName).empty());
        }
    }
}
