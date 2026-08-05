////////////////////////////////////////////////////////////////////////////////
///
/// presetFileTests.cpp
/// -------------------
///
///   Two holes, both on the *file* side of loading a preset, and both recorded
/// in doc/tech/todo.md.
///
///   **Loading one preset on top of another.** `presetCorpusTests.cpp` uses a
/// fresh engine per preset, deliberately and correctly -- a snapshot in which
/// row 200 changes when row 199 does is not a snapshot of row 200. The
/// consequence is that nothing in the suite ever did what a user does, which is
/// click the next preset in the browser. That is not the same code path:
/// `loadModuleChain()` *merges*, moving a module already holding the right
/// effect across rather than building a new one, so the second load is the
/// interesting one and it had never run headlessly against the corpus.
///
///   **A file that is not a preset.** Four `[clap][state]` cases drive bad
/// *streams* -- empty, truncated, wrong root, a format from the future -- and
/// the file side had none of it. `readPresetFile` has four ways to answer "no"
/// and nothing had asked it any of them. The corpus proves 303 happy paths and
/// asserts `unknownEffect == 0` and `missing` against a committed number, so
/// both counters were only ever checked in the direction that says nothing:
/// neither had ever been driven above zero, and a reporter wired to a counter
/// that is never incremented reads exactly like one that works.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "presets/presetHarness.hpp"

#include "core/modules/factory.hpp"

/// \note Not sorted with the block above, for the reason presetCorpusTests.cpp
/// gives: loadPreset() downcasts a chain node to SW::Module and this is the
/// header with the complete type.
#include "core/modules/moduleDSPAndGUI.hpp"
#include "core/modules/finalImplementations.hpp"

#include "le/spectrumworx/presetStorage.hpp"
#include "le/spectrumworx/presets.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
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

using SWTest::Loaded;
using SWTest::PresetConsumer;
using SWTest::ScopedProblemCounter;

//------------------------------------------------------------------------------
// The corpus, and reading it
//------------------------------------------------------------------------------

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

/// The bytes of a preset file, in the writable NUL-terminated buffer the
/// destructive parse wants. Empty if the file cannot be read.
std::vector<char> presetBytes(std::filesystem::path const &file)
{
    auto const data(readPresetFile(file));
    if (!data)
        return {};
    std::string_view const text(data.get());
    std::vector<char> buffer(text.begin(), text.end());
    buffer.push_back('\0');
    return buffer;
}

/// A configured engine with nothing loaded into it.
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

    SWTest::Engine &engine() { return engine_; }

    /// \brief Loads \p data into this engine, whatever it already holds.
    /// \return whether the load succeeded.
    bool load(std::vector<char> data)
    {
        if (data.empty())
            return false;
        SWTest::clearPresetProblems();
        ScopedProblemCounter const counting;
        return LE::SW::loadPreset(data.data(), true /*ignore external samples*/, nullptr,
                                  PresetConsumer{engine_});
    }

    Loaded dump() { return SWTest::dump(engine_); }

  private:
    SWTest::Engine engine_;
}; // class Fixture

/// What \p file loads to in an engine that has never held anything else, which
/// is the answer presetCorpus.txt committed.
Loaded intoAFreshEngine(std::filesystem::path const &file)
{
    Fixture fixture;
    REQUIRE(fixture.load(presetBytes(file)));
    return fixture.dump();
}

//------------------------------------------------------------------------------
// Writing files that are not presets
//------------------------------------------------------------------------------

std::filesystem::path outputDirectory()
{
    std::filesystem::path const directory(std::filesystem::path(SW_TEST_OUTPUT_DIR) / "presets");
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    return directory;
}

/// \brief Puts \p bytes on disk under \p name and hands back the path.
std::filesystem::path fileHolding(std::string const &name, std::string_view const bytes)
{
    auto const path(outputDirectory() / name);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    REQUIRE(stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size())));
    stream.close();
    REQUIRE(std::filesystem::exists(path));
    return path;
}

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
// One preset on top of another
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A preset loaded on top of another is the preset, not a blend", "[preset-file]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note The whole corpus through **one** engine, in order, which is a
    /// browser being clicked down a bank. After each load the engine is compared
    /// against the same preset loaded into a fresh one, so every preset is
    /// checked against a *different* predecessor -- 302 distinct "what was there
    /// before" states rather than one chosen by hand.
    ///
    ///   What it is written against is the merge: `loadModuleChain()` moves a
    /// module already holding the right effect across rather than building a new
    /// one, so anything the previous preset left in that module and the new one
    /// does not mention is carried forward. A parameter the writer omits because
    /// it is at its default is exactly such a thing, and 104 of these 303 files
    /// omit at least one.
    ///
    ////////////////////////////////////////////////////////////////////////////
    auto const files(corpus());
    REQUIRE(files.size() >= 300);

    Fixture reused;
    unsigned int loaded{0};
    std::string previous("(nothing)");

    for (auto const &[key, path] : files)
    {
        INFO("preset " << key << " loaded on top of " << previous);

        REQUIRE(reused.load(presetBytes(path)));
        auto const onTop(reused.dump());
        auto const fresh(intoAFreshEngine(path));

        // The effect chain first, in the clear: the common failure is a module
        // the previous preset had and this one did not ask for.
        CHECK(onTop.modules == fresh.modules);
        CHECK(onTop.effects == fresh.effects);
        // ...and then every parameter of every one of them.
        CHECK(onTop.text == fresh.text);

        previous = key;
        ++loaded;
    }

    INFO("presets loaded in sequence: " << loaded);
    CHECK(loaded == files.size());
}

TEST_CASE("Loading the same preset twice changes nothing the second time", "[preset-file]")
{
    /// \note The degenerate case of the merge, and the one where it does the
    /// most work: every module in the chain already holds the effect the preset
    /// asks for, so every one of them is moved across rather than built. A
    /// merge that mutated what it moved would show up here on the second load
    /// and nowhere else.
    auto const files(corpus());
    REQUIRE(files.size() >= 300);

    /// The first preset holding three or more modules, so the merge has a real
    /// chain to reuse rather than one slot.
    for (auto const &[key, path] : files)
    {
        Fixture fixture;
        REQUIRE(fixture.load(presetBytes(path)));
        auto const once(fixture.dump());
        if (once.modules < 3)
            continue;

        INFO("preset " << key << ", " << once.modules << " modules");
        REQUIRE(fixture.load(presetBytes(path)));
        CHECK(fixture.dump().text == once.text);
        return;
    }
    FAIL("no preset in the corpus holds three modules");
}

TEST_CASE("An empty preset empties the chain a full one filled", "[preset-file]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note The direction the corpus sweep above cannot reach, because every
    /// file in it has at least one module: loading a preset with **no** modules
    /// on top of one with five has to leave five empty slots, not five modules
    /// nobody asked for.
    ///
    ///   Written by hand rather than found in the banks for that reason. It is
    /// legal state -- `stateTests.cpp` builds the same shape -- and it is what
    /// "New" in the browser produces.
    ///
    ////////////////////////////////////////////////////////////////////////////
    auto const files(corpus());
    REQUIRE_FALSE(files.empty());

    Fixture fixture;
    REQUIRE(fixture.load(presetBytes(files.front().second)));
    REQUIRE(fixture.dump().modules > 0);

    std::string const empty(
        "<SpectrumWorxPreset Format=\"3\" Version=\"3.0\" LastModified=\"\" Comment=\"\">"
        "<Global><p n=\"In\" v=\"0.5\" /></Global>"
        "<Modules /></SpectrumWorxPreset>");
    REQUIRE(fixture.load(std::vector<char>(empty.begin(), empty.end() + 1)));

    auto const cleared(fixture.dump());
    CHECK(cleared.modules == 0);
    CHECK(cleared.effects == "-");
}

////////////////////////////////////////////////////////////////////////////////
// A file that is not a preset
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A preset file that cannot be read answers with nothing", "[preset-file]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note `readPresetFile` has four ways to fail and nothing had driven any
    /// of them. Each is a real thing a user can arrange -- a preset deleted
    /// while the browser still lists it, a directory named `*.swp`, a file
    /// truncated by a full disk -- and the contract for all of them is the same
    /// empty pointer rather than a partly filled buffer.
    ///
    ////////////////////////////////////////////////////////////////////////////
    auto const directory(outputDirectory());

    CHECK_FALSE(readPresetFile(directory / "no such preset.swp"));

    /// \note A directory, which `file_size` answers about without erroring on
    /// some platforms -- so this is the one that could plausibly get through and
    /// hand back a buffer of whatever the directory entry's size happens to be.
    auto const asDirectory(directory / "a directory.swp");
    std::error_code error;
    std::filesystem::create_directories(asDirectory, error);
    CHECK_FALSE(readPresetFile(asDirectory));

    /// An empty file reads successfully and yields a buffer holding just the
    /// terminator -- which is a *parse* failure rather than a read one, and the
    /// case below is where that is asserted.
    auto const emptyFile(fileHolding("empty.swp", ""));
    auto const empty(readPresetFile(emptyFile));
    REQUIRE(empty);
    CHECK(empty[0] == '\0');
}

TEST_CASE("A file that is not a preset is refused rather than half applied", "[preset-file]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note The file-side twin of stateTests.cpp's "A state that cannot be read
    /// is refused rather than half applied". Same shapes, driven through
    /// `readPresetFile` and `loadPreset` instead of through a `clap_istream`,
    /// because those are different code and a user reaches them by a different
    /// route -- the browser, not the host.
    ///
    ///   As there, the `false` is the smaller half of it. What matters is that
    /// the engine still holds the preset it held before, rather than a chain
    /// half rebuilt from a document that stopped making sense in the middle.
    ///
    ////////////////////////////////////////////////////////////////////////////
    auto const files(corpus());
    REQUIRE_FALSE(files.empty());
    auto const good(presetBytes(files.front().second));
    REQUIRE_FALSE(good.empty());

    auto const refuses([&](std::string const &name, std::string_view const bytes) {
        INFO(name);
        Fixture fixture;
        REQUIRE(fixture.load(good));
        auto const before(fixture.dump());

        auto const path(fileHolding(name, bytes));
        CHECK_FALSE(fixture.load(presetBytes(path)));
        CHECK(fixture.dump().text == before.text);
    });

    refuses("empty.swp", "");
    refuses("prose.swp", "this is not a preset, it is a sentence");
    refuses("half a document.swp", "<SpectrumWorxPreset Format=\"3\"><Global>");
    refuses("wrong root.swp", "<SomethingElse Format=\"3\"></SomethingElse>");
    refuses("from the future.swp",
            "<SpectrumWorxPreset Format=\"99\"><Global /><Modules /></SpectrumWorxPreset>");

    /// \note A real preset cut in half, which is the truncation a full disk
    /// actually produces: valid bytes as far as they go, and a tag that never
    /// closes. Cut at 60 % so the cut lands inside the module list rather than
    /// in the header, where the parse would fail on the first element.
    std::string_view const whole(good.data(), good.size() - 1);
    refuses("truncated.swp", whole.substr(0, whole.size() * 3 / 5));
}

TEST_CASE("A preset naming an effect this build does not have loads the rest", "[preset-file]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note `unknownEffect` driven above zero for the first time. The corpus
    /// asserts it is *zero* over 303 files, which is the right assertion there
    /// -- an effect going missing is a build that dropped one -- but on its own
    /// it is equally consistent with a counter nothing ever increments and a
    /// reporter nothing ever calls.
    ///
    ///   The behaviour it pins is degrading rather than refusing, which is the
    /// preset format's long-standing contract: a project written by a build with
    /// an effect this one does not have still opens, with everything it can
    /// understand.
    ///
    ////////////////////////////////////////////////////////////////////////////
    std::string const preset(
        "<SpectrumWorxPreset Format=\"3\" Version=\"3.0\" LastModified=\"\" Comment=\"\">"
        "<Global><p n=\"In\" v=\"0.25\" /></Global>"
        "<Modules>"
        "<Module effect=\"No Such Effect\"><p n=\"Bypass\" v=\"0\" /></Module>"
        "<Module effect=\"Ah-ah\"><p n=\"Bypass\" v=\"0\" /></Module>"
        "</Modules></SpectrumWorxPreset>");

    Fixture fixture;
    REQUIRE(fixture.load(presetBytes(fileHolding("unknown effect.swp", preset))));

    auto const loaded(fixture.dump());
    CHECK(loaded.modules == 1);
    CHECK(loaded.effects == "Ah-ah");

    // ...and it said so, once, rather than dropping the module silently.
    CHECK(SWTest::presetProblems().unknownEffect == 1);
}

TEST_CASE("A preset that omits a parameter reports it and uses the default", "[preset-file]")
{
    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \note `missingParameter` driven deliberately, which is the other counter
    /// the corpus only ever reads. It reports a committed *number* per preset --
    /// normal for a 2009-2011 file against a 2016 effect, and a number that must
    /// not grow -- so the count is pinned but the mechanism behind it is not:
    /// a reporter that stopped firing would move 104 rows at once and read as
    /// one large diff rather than as a broken reporter.
    ///
    ///   Here the omission is deliberate and the expected count is exact.
    ///
    ////////////////////////////////////////////////////////////////////////////
    auto const withOnly([](std::string_view const parameters) {
        return std::string("<SpectrumWorxPreset Format=\"3\" Version=\"3.0\" LastModified=\"\" "
                           "Comment=\"\"><Global><p n=\"In\" v=\"0.5\" /></Global><Modules>"
                           "<Module effect=\"Gain\">") +
               std::string(parameters) + "</Module></Modules></SpectrumWorxPreset>";
    });

    /// Everything Gain has: five base parameters and its own one, so a complete
    /// preset raises nothing and the difference below is the omission itself.
    auto const complete(withOnly("<p n=\"Bypass\" v=\"0\" /><p n=\"Gain\" v=\"0\" />"
                                 "<p n=\"Wet\" v=\"100\" /><p n=\"StartFreq\" v=\"0\" />"
                                 "<p n=\"StopFreq\" v=\"22050\" /><p n=\"Gain\" v=\"3\" />"));

    Fixture fixture;
    REQUIRE(fixture.load(presetBytes(fileHolding("complete.swp", complete))));
    auto const whenComplete(SWTest::presetProblems().missingParameter);

    /// The same module with only its Bypass, which is what a file written by a
    /// build whose Gain had fewer knobs looks like.
    REQUIRE(fixture.load(
        presetBytes(fileHolding("sparse.swp", withOnly("<p n=\"Bypass\" v=\"0\" />")))));
    auto const whenSparse(SWTest::presetProblems().missingParameter);

    UNSCOPED_INFO("complete: " << whenComplete << " missing, sparse: " << whenSparse);
    CHECK(whenSparse > whenComplete);

    // It loaded anyway, and what it did not mention is at its default.
    auto const loaded(fixture.dump());
    CHECK(loaded.modules == 1);
    CHECK(loaded.effects == "Gain");
}
