////////////////////////////////////////////////////////////////////////////////
///
/// \file stateTests.cpp
/// --------------------
///
///   `clap_plugin_state`, which until 02.08.2026 had no test of any kind --
/// `grep -r CLAP_EXT_STATE tests/` returned nothing, on the one surface every
/// user of every format meets every time they reopen a project.
///
///   A `clap_ostream` and a `clap_istream` over a `std::vector<char>` are about
///   thirty lines, and everything here is a mutation of them: what the plugin
/// writes, what it does with what it is given, and what it does with what no
/// sane host would give it.
///
///   Two things are reached through `clap_plugin::plugin_data` rather than the C
/// API -- the loaded sample and the editor pointer -- because neither has a C
/// entry point and both are exactly what this change is about. The precedent is
/// processLockTests.cpp, which does the same to hold a lock a host cannot name.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "spectrumWorxCLAP.hpp"
#include "swClapEntryImpl.hpp"

#include "core/automatedModuleChain.hpp"
#include "le/spectrumworx/effects/configuration/effectNames.hpp"
#include "le/spectrumworx/presetFile.hpp"

/// \note For ScopedProblemCounter, which swaps the default preset-problem
/// reporter -- a `juce::AlertWindow` per problem -- for a counter. Without it a
/// 2011 preset loaded here raises one message box per parameter its effect grew
/// later, in a process with no message thread, and leaks an AsyncUpdater for
/// each. See the note on the class.
#include "presets/presetHarness.hpp"

#include <clap/clap.h>

#include <juce_core/juce_core.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
//------------------------------------------------------------------------------
namespace
{
//------------------------------------------------------------------------------

constexpr std::uint32_t blockSize{256};
constexpr double sampleRate{48000};

////////////////////////////////////////////////////////////////////////////////
// The streams
////////////////////////////////////////////////////////////////////////////////

/// \brief A `clap_ostream` over a growable buffer.
///
/// \param chunk the most it will accept per call, so that a plugin which assumes
/// its whole write lands in one go is caught. `writeFully()` loops; something
/// that did not, would not.
class OutStream
{
  public:
    explicit OutStream(std::int64_t const chunk = 0) : chunk_(chunk)
    {
        stream_.ctx = this;
        stream_.write = [](clap_ostream const *const stream, void const *const buffer,
                           std::uint64_t const size) -> std::int64_t {
            auto &self(*static_cast<OutStream *>(stream->ctx));
            if (self.failAfter_ >= 0 && self.written_ >= self.failAfter_)
                return -1;
            auto const accepted(
                self.chunk_ > 0 ? std::min<std::uint64_t>(size, std::uint64_t(self.chunk_)) : size);
            auto const *const bytes(static_cast<char const *>(buffer));
            self.data_.insert(self.data_.end(), bytes, bytes + accepted);
            self.written_ += std::int64_t(accepted);
            return std::int64_t(accepted);
        };
    }

    OutStream(OutStream const &) = delete; // holds a pointer to itself
    OutStream &operator=(OutStream const &) = delete;

    clap_ostream const *operator&() const { return &stream_; }

    /// After this many bytes, every write reports an error.
    void failAfter(std::int64_t const bytes) { failAfter_ = bytes; }

    std::vector<char> const &data() const { return data_; }

    /// What was written, as a string -- the trailing NUL the plugin writes is
    /// not part of it.
    std::string text() const
    {
        if (data_.empty())
            return {};
        return std::string(data_.data(), data_.size() - (data_.back() == '\0' ? 1 : 0));
    }

  private:
    clap_ostream stream_{};
    std::vector<char> data_;
    std::int64_t chunk_;
    std::int64_t written_{0};
    std::int64_t failAfter_{-1};
}; // class OutStream

/// \brief A `clap_istream` over a fixed buffer.
///
/// \param chunk the most it will hand back per call. A host is free to answer
/// one byte at a time and CLAP says so, so this is not a hypothetical.
class InStream
{
  public:
    explicit InStream(std::vector<char> data, std::int64_t const chunk = 0)
        : data_(std::move(data)), chunk_(chunk)
    {
        stream_.ctx = this;
        stream_.read = [](clap_istream const *const stream, void *const buffer,
                          std::uint64_t const size) -> std::int64_t {
            auto &self(*static_cast<InStream *>(stream->ctx));
            if (self.fail_)
                return -1;
            auto available(self.data_.size() - self.read_);
            if (available == 0)
                return 0;
            auto const wanted(
                self.chunk_ > 0 ? std::min<std::uint64_t>(size, std::uint64_t(self.chunk_)) : size);
            auto const handed(std::min<std::uint64_t>(wanted, available));
            std::memcpy(buffer, self.data_.data() + self.read_, handed);
            self.read_ += handed;
            return std::int64_t(handed);
        };
    }

    explicit InStream(std::string const &text, std::int64_t const chunk = 0)
        : InStream(std::vector<char>(text.begin(), text.end()), chunk)
    {
    }

    InStream(InStream const &) = delete;
    InStream &operator=(InStream const &) = delete;

    clap_istream const *operator&() const { return &stream_; }

    /// Reports an error on the first read, which is not the same as being empty.
    void fail() { fail_ = true; }

  private:
    clap_istream stream_{};
    std::vector<char> data_;
    std::size_t read_{0};
    std::int64_t chunk_;
    bool fail_{false};
}; // class InStream

////////////////////////////////////////////////////////////////////////////////
// The plugin
////////////////////////////////////////////////////////////////////////////////

clap_host const &nullHost()
{
    static clap_host host{CLAP_VERSION,
                          nullptr,
                          "sw-tests",
                          "SpectrumWorx",
                          "",
                          "0",
                          [](clap_host const *, char const *) -> void const * { return nullptr; },
                          [](clap_host const *) {},
                          [](clap_host const *) {},
                          [](clap_host const *) {}};
    return host;
}

clap_plugin_factory const &factory()
{
    auto const *const pFactory(static_cast<clap_plugin_factory const *>(
        LE::SW::ClapFirst::getFactory(CLAP_PLUGIN_FACTORY_ID)));
    REQUIRE(pFactory != nullptr);
    return *pFactory;
}

/// \brief RAII around the refcounted entry point. \see pluginTests.cpp
class Entry
{
  public:
    Entry() { REQUIRE(LE::SW::ClapFirst::clapInit("sw-tests")); }
    ~Entry() { LE::SW::ClapFirst::clapDeinit(); }

    Entry(Entry const &) = delete; // makes non-copyable
    Entry &operator=(Entry const &) = delete;
}; // class Entry

/// \brief An initialised plugin, activated only if asked.
///
/// \note Inactive by default, because that is when a host restores state: it
/// creates the plugin, hands back the project's bytes and only then activates.
/// A state format that needs a sample rate to load would fail exactly there and
/// nowhere else.
class Plugin
{
  public:
    explicit Plugin(clap_host const &host = nullHost(), bool const active = false)
    {
        auto const *const pDescriptor(factory().get_plugin_descriptor(&factory(), 0));
        REQUIRE(pDescriptor != nullptr);
        pPlugin_ = factory().create_plugin(&factory(), &host, pDescriptor->id);
        REQUIRE(pPlugin_ != nullptr);
        REQUIRE(pPlugin_->init(pPlugin_));
        if (active)
        {
            REQUIRE(pPlugin_->activate(pPlugin_, sampleRate, 1, blockSize));
            active_ = true;
        }
    }

    ~Plugin()
    {
        if (active_)
            pPlugin_->deactivate(pPlugin_);
        pPlugin_->destroy(pPlugin_);
    }

    Plugin(Plugin const &) = delete; // makes non-copyable
    Plugin &operator=(Plugin const &) = delete;

    clap_plugin const &operator*() const { return *pPlugin_; }
    clap_plugin const *operator->() const { return pPlugin_; }

    /// \note clap-helpers keeps the C++ object in `plugin_data`. Needed for the
    /// two things state now carries that the C API cannot ask about.
    LE::SW::SpectrumWorxCLAP &implementation() const
    {
        auto *const pHelper(static_cast<LE::SW::PluginHelper *>(pPlugin_->plugin_data));
        REQUIRE(pHelper != nullptr);
        return *static_cast<LE::SW::SpectrumWorxCLAP *>(pHelper);
    }

    /// \note The sample virtuals are `EditorHost`'s and protected on the plugin,
    /// because the editor is what is supposed to call them. A test is not an
    /// editor; going in through the interface the editor uses is closer to the
    /// truth than widening the plugin's own would be.
    LE::SW::GUI::EditorHost &editorHost() const { return implementation(); }

    clap_plugin_state const &state() const
    {
        auto const *const pState(static_cast<clap_plugin_state const *>(
            pPlugin_->get_extension(pPlugin_, CLAP_EXT_STATE)));
        REQUIRE(pState != nullptr);
        return *pState;
    }

    clap_plugin_params const &params() const
    {
        auto const *const pParams(static_cast<clap_plugin_params const *>(
            pPlugin_->get_extension(pPlugin_, CLAP_EXT_PARAMS)));
        REQUIRE(pParams != nullptr);
        return *pParams;
    }

    /// Every parameter the plugin exports, by id, in the order it exports them.
    std::vector<std::pair<clap_id, double>> allParameters() const
    {
        auto const &parameters(params());
        std::vector<std::pair<clap_id, double>> values;
        auto const count(parameters.count(pPlugin_));
        for (std::uint32_t index(0); index < count; ++index)
        {
            clap_param_info info{};
            REQUIRE(parameters.get_info(pPlugin_, index, &info));
            double value{0};
            REQUIRE(parameters.get_value(pPlugin_, info.id, &value));
            values.emplace_back(info.id, value);
        }
        return values;
    }

    void setParameter(clap_id const id, double const value) const
    {
        implementation().setParameter(LE::SW::ParameterID{LE::Plugins::ParameterID{id}},
                                      static_cast<LE::Plugins::AutomatedParameterValue>(value));
    }

  private:
    /// \note First, so it outlives every load this plugin does. A state load
    /// reports its problems from wherever it reaches them and the default
    /// reporter is a message box; one per parameter a 2011 preset never
    /// mentioned, in a process with no message thread.
    SWTest::ScopedProblemCounter quiet_;

    clap_plugin const *pPlugin_;
    bool active_{false};
}; // class Plugin

/// \brief Fills three slots and moves a handful of parameters off their
/// defaults, so that a round trip has something to lose.
void driveIntoAState(Plugin const &plugin)
{
    auto &implementation(plugin.implementation());
    auto &chain(implementation.program().moduleChain());

    for (std::uint8_t slot(0); slot < 3; ++slot)
        REQUIRE(chain
                    .setParameter(slot, static_cast<std::int8_t>(slot + 2),
                                  implementation.moduleInitialiser())
                    .second == static_cast<std::int8_t>(slot + 2));

    /// Every module's Gain and Wet, and the global input gain: enough that a
    /// dropped attribute anywhere in the document shows up.
    std::uint8_t index(0);
    chain.forEach<LE::SW::Engine::ModuleParameters>(
        [&](LE::SW::Engine::ModuleParameters const &constModule) {
            auto &module(const_cast<LE::SW::Engine::ModuleParameters &>(constModule));
            module.setBaseParameter(1 /*Gain*/, -3.0f - float(index));
            module.setBaseParameter(2 /*Wet*/, 90.0f - float(index));
            ++index;
        });

    implementation.program().parameters().get<LE::SW::GlobalParameters::InputGain>().setValue(
        0.75f);
}

std::vector<char> asBuffer(std::string const &text)
{
    std::vector<char> buffer(text.begin(), text.end());
    buffer.push_back('\0');
    return buffer;
}

//------------------------------------------------------------------------------
} // anonymous namespace
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
// The round trip
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A session's parameters come back through a second plugin instance", "[clap][state]")
{
    Entry const entry;

    OutStream saved;
    std::vector<std::pair<clap_id, double>> original;
    {
        Plugin const plugin;
        driveIntoAState(plugin);
        original = plugin.allParameters();

        REQUIRE(plugin.state().save(&*plugin, &saved));
    }

    REQUIRE_FALSE(saved.data().empty());

    /// \note A *second instance*, not the same one reloaded. Restoring into the
    /// object that saved would pass with a state format that wrote nothing at
    /// all, because everything is already where it belongs.
    Plugin const restored;
    InStream stream(saved.data());
    REQUIRE(restored.state().load(&*restored, &stream));

    auto const reloaded(restored.allParameters());
    REQUIRE(reloaded.size() == original.size());
    for (std::size_t index(0); index < original.size(); ++index)
    {
        INFO("parameter " << index << " of " << original.size());
        CHECK(reloaded[index].first == original[index].first);
        CHECK(reloaded[index].second == Catch::Approx(original[index].second));
    }
}

TEST_CASE("A session restores before the plugin has a sample rate", "[clap][state]")
{
    Entry const entry;

    OutStream saved;
    {
        Plugin const plugin(nullHost(), true /*active*/);
        driveIntoAState(plugin);
        REQUIRE(plugin.state().save(&*plugin, &saved));
    }

    /// \note Inactive, which is where a host restores: create, set state,
    /// activate. Nothing in the load may need a sample rate.
    Plugin const restored;
    InStream stream(saved.data());
    REQUIRE(restored.state().load(&*restored, &stream));

    std::uint8_t modules(0);
    restored.implementation().program().moduleChain().forEach<LE::SW::Engine::ModuleParameters>(
        [&](LE::SW::Engine::ModuleParameters const &) { ++modules; });
    CHECK(modules == 3);
}

////////////////////////////////////////////////////////////////////////////////
// What the bytes are
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Session state is the preset format plus a dawExtraState block", "[clap][state]")
{
    Entry const entry;
    Plugin const plugin;
    driveIntoAState(plugin);

    OutStream saved;
    REQUIRE(plugin.state().save(&*plugin, &saved));
    auto const text(saved.text());
    INFO("state:\n" << text);

    /// It is a preset: same root, same version stamp, same parameter shape.
    CHECK(text.find("<SpectrumWorxPreset") != std::string::npos);
    CHECK(text.find("Format=\"3\"") != std::string::npos);
    CHECK(text.find("<p n=") != std::string::npos);

    /// \note And it is *more* than a preset, which is the whole reason the two
    /// can share a serialisation. Empty today; present regardless, so that the
    /// day it is not empty is not also the day this starts being written.
    CHECK(text.find("<dawExtraState") != std::string::npos);

    /// The terminator goes into the stream: loadFrom() parses a C string and a
    /// host may hand back exactly these bytes and nothing after them.
    CHECK(saved.data().back() == '\0');

    /// \note A preset written to a file must *not* carry the block. If it did,
    /// opening somebody's preset would silently overwrite where your browser was
    /// pointing and which settings you had -- the exact confusion between "what
    /// this sounds like" and "where I was" that the block exists to avoid.
    auto const asPreset(
        LE::SW::savePreset(juce::File(), juce::String(), plugin.implementation().program()));
    CHECK(asPreset.find("<SpectrumWorxPreset") != std::string::npos);
    CHECK(asPreset.find("<dawExtraState") == std::string::npos);
}

TEST_CASE("A 2011 preset is legal session state", "[clap][state]")
{
    Entry const entry;
    Plugin const plugin;

    /// \note The formats are one now, so the bytes of a factory preset are bytes
    /// `stateLoad` must accept. Worth pinning because it is the cheapest
    /// possible statement of "state is the preset serialisation" -- and because
    /// a host that migrates an old project by handing over a preset file is not
    /// a strange host.
    juce::File const preset(juce::String(SW_PRESET_DATA_DIR) + "/Voices/LE Autotalk.swp");
    REQUIRE(preset.existsAsFile());

    juce::MemoryBlock contents;
    REQUIRE(preset.loadFileAsData(contents));

    InStream stream(
        std::vector<char>(static_cast<char const *>(contents.getData()),
                          static_cast<char const *>(contents.getData()) + contents.getSize()));
    REQUIRE(plugin.state().load(&*plugin, &stream));

    std::vector<std::string> effects;
    plugin.implementation().program().moduleChain().forEach<LE::SW::Engine::ModuleParameters>(
        [&](LE::SW::Engine::ModuleParameters const &module) {
            effects.emplace_back(LE::SW::Effects::effectStreamingName(module.effectTypeIndex()));
        });
    REQUIRE(effects.size() == 2);
    CHECK(effects[0] == "Talking Wind");
    CHECK(effects[1] == "TuneWorx");
}

////////////////////////////////////////////////////////////////////////////////
// The sample
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("The loaded sample survives a session", "[clap][state]")
{
    Entry const entry;

    /// \note A factory sample, named without a directory. That is the one
    /// spelling that survives a move between machines -- Sample::load() resolves
    /// a bare name against the embedded set when there is nothing on disk -- and
    /// therefore the one worth pinning here.
    juce::File const sample(juce::File::createFileWithoutCheckingPath("Carrier.mp3"));

    OutStream saved;
    {
        Plugin const plugin(nullHost(), true /*active*/);
        plugin.editorHost().setNewSample(sample);
        REQUIRE(plugin.editorHost().currentSampleFile() != juce::File());

        REQUIRE(plugin.state().save(&*plugin, &saved));
        INFO("state:\n" << saved.text());
        CHECK(saved.text().find("Carrier.mp3") != std::string::npos);
    }

    Plugin const restored(nullHost(), true /*active*/);
    REQUIRE(restored.editorHost().currentSampleFile() == juce::File());

    InStream stream(saved.data());
    REQUIRE(restored.state().load(&*restored, &stream));

    /// \note This is the whole of week_two.md item 4's inherited bug: a session
    /// that restored everything except which audio file was loaded.
    CHECK(restored.editorHost().currentSampleFile().getFileName() == "Carrier.mp3");
}

TEST_CASE("Loading a sample marks the session dirty", "[clap][state]")
{
    Entry const entry;

    /// \brief A host that offers `clap.state` and counts what it is told.
    ///
    /// \note And deliberately no `clap.thread-check`, matching pluginTests.cpp's
    /// StatefulHost: the mark is then always deferred to on_main_thread(), which
    /// is correct from either thread and is the arm that has to work.
    static struct Counters
    {
        unsigned dirtyMarks{0};
        unsigned callbacks{0};
    } counters;
    counters = {};

    static clap_host_state stateExtension{[](clap_host const *) { ++counters.dirtyMarks; }};
    clap_host host{CLAP_VERSION,
                   nullptr,
                   "sw-tests",
                   "SpectrumWorx",
                   "",
                   "0",
                   [](clap_host const *, char const *const id) -> void const * {
                       return (std::strcmp(id, CLAP_EXT_STATE) == 0) ? &stateExtension : nullptr;
                   },
                   [](clap_host const *) {},
                   [](clap_host const *) {},
                   [](clap_host const *) { ++counters.callbacks; }};

    Plugin const plugin(host, true /*active*/);

    plugin.editorHost().setNewSample(juce::File::createFileWithoutCheckingPath("Carrier.mp3"));

    CHECK(counters.callbacks > 0);
    CHECK(counters.dirtyMarks == 0); // deferred, not skipped

    plugin->on_main_thread(&*plugin);
    CHECK(counters.dirtyMarks == 1);
}

////////////////////////////////////////////////////////////////////////////////
// What a host may do to the stream
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A state stream is read whatever size the pieces arrive in", "[clap][state]")
{
    Entry const entry;

    OutStream saved(7 /*bytes per write*/);
    {
        Plugin const plugin;
        driveIntoAState(plugin);
        REQUIRE(plugin.state().save(&*plugin, &saved));
    }

    /// \note One byte per read, which CLAP permits and which a state format that
    /// reads a fixed-size header would survive by luck rather than by design.
    Plugin const restored;
    InStream stream(saved.data(), 1);
    REQUIRE(restored.state().load(&*restored, &stream));

    std::uint8_t modules(0);
    restored.implementation().program().moduleChain().forEach<LE::SW::Engine::ModuleParameters>(
        [&](LE::SW::Engine::ModuleParameters const &) { ++modules; });
    CHECK(modules == 3);
}

TEST_CASE("A save whose stream fails is reported as a failure", "[clap][state]")
{
    Entry const entry;
    Plugin const plugin;
    driveIntoAState(plugin);

    OutStream saved(16);
    saved.failAfter(32); // a disk that fills up part way through
    CHECK_FALSE(plugin.state().save(&*plugin, &saved));
}

////////////////////////////////////////////////////////////////////////////////
// What a host must not be able to do
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("A state that cannot be read is refused rather than half applied", "[clap][state]")
{
    Entry const entry;

    /// \note Every one of these leaves the plugin as it was. The failure that
    /// matters is not the `false` -- it is a chain half rebuilt from a document
    /// that stopped making sense in the middle, which is what a two-pass apply
    /// over a truncated array used to be able to do.
    auto const refuses([](std::vector<char> data, char const *const what) {
        INFO(what);
        Plugin const plugin;
        driveIntoAState(plugin);
        auto const before(plugin.allParameters());

        InStream stream(std::move(data));
        CHECK_FALSE(plugin.state().load(&*plugin, &stream));
        CHECK(plugin.allParameters() == before);
    });

    refuses({}, "an empty stream");
    refuses(asBuffer("not xml at all"), "text that is not a document");
    refuses(asBuffer("<SpectrumWorxPreset Format=\"3\"><Global>"), "a document cut in half");
    refuses(asBuffer("<SomethingElse Format=\"3\"></SomethingElse>"), "the wrong root element");
    refuses(
        asBuffer("<SpectrumWorxPreset Format=\"99\"><Global /><Modules /></SpectrumWorxPreset>"),
        "a format from the future");

    /// \note The private binary blob this replaced. Nothing shipped with it, so
    /// there is no reader; what there must be is a clean refusal rather than a
    /// parse of arbitrary bytes as if they were text.
    std::vector<char> legacyBlob{'S', 'W', 'X', '1', 0x1e, 0x01, 0x00, 0x00};
    legacyBlob.resize(3440, '\0');
    refuses(std::move(legacyBlob), "an SWX1 blob from a development session");
}

TEST_CASE("A stream that errors is not mistaken for an empty one", "[clap][state]")
{
    Entry const entry;
    Plugin const plugin;

    InStream stream(std::vector<char>{'<'});
    stream.fail();
    CHECK_FALSE(plugin.state().load(&*plugin, &stream));
}

TEST_CASE("State naming an effect this build does not have loads the rest", "[clap][state]")
{
    Entry const entry;
    Plugin const plugin;

    /// \note Degrading rather than refusing, which is the preset format's
    /// long-standing contract and now the session's too: an edition without an
    /// effect, or a project from a build that had one this does not, still opens
    /// with everything it can understand.
    std::string const state(
        "<SpectrumWorxPreset Format=\"3\" Version=\"3.0\" LastModified=\"\" Comment=\"\">"
        "<Global><p n=\"In\" v=\"0.25\" /></Global>"
        "<Modules>"
        "<Module effect=\"No Such Effect\"><p n=\"Bypass\" v=\"0\" /></Module>"
        "<Module effect=\"Ah-ah\"><p n=\"Bypass\" v=\"0\" /></Module>"
        "</Modules></SpectrumWorxPreset>");

    InStream stream(asBuffer(state));
    REQUIRE(plugin.state().load(&*plugin, &stream));

    std::vector<std::string> effects;
    plugin.implementation().program().moduleChain().forEach<LE::SW::Engine::ModuleParameters>(
        [&](LE::SW::Engine::ModuleParameters const &module) {
            effects.emplace_back(LE::SW::Effects::effectStreamingName(module.effectTypeIndex()));
        });
    REQUIRE(effects.size() == 1);
    CHECK(effects[0] == "Ah-ah");
    CHECK(plugin.implementation()
              .program()
              .parameters()
              .get<LE::SW::GlobalParameters::InputGain>()
              .getValue() == Catch::Approx(0.25f));
}
