////////////////////////////////////////////////////////////////////////////////
///
/// \file presets.hpp
/// -----------------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef presets_hpp__6021812F_90A0_4BFC_A0EF_6413D7485312
#define presets_hpp__6021812F_90A0_4BFC_A0EF_6413D7485312
//------------------------------------------------------------------------------
#ifndef LE_SW_SDK_BUILD
#include "configuration/constants.hpp"

#ifndef _MSC_VER
#include "configuration/versionConfiguration.hpp"
#include "gui/gui.hpp" // warningMessageBox()
#endif
#endif // !LE_SW_SDK_BUILD

#ifndef _MSC_VER // for eager compilers
#include "le/math/conversion.hpp"
#include "le/parameters/parametersUtilities.hpp"
#include "le/spectrumworx/engine/parameters.hpp"
#endif // _MSC_VER
#include "le/utility/countof.hpp"
#include "le/utility/lexicalCast.hpp"
#include "le/utility/platformSpecifics.hpp"
#include "le/utility/tchar.hpp"
#include "le/utility/trace.hpp"

#include <tinyxml/tinyxml.h>

#include <optional>

#include "le/parameters/parametersUtilities.hpp"
#include "le/utility/intrusivePtr.hpp"
#include "le/utility/ignoreUnused.hpp"
#include <string_view>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <utility> // rvalues
//------------------------------------------------------------------------------
namespace juce
{
class File;
class String;
} // namespace juce
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace Parameters
{
class LFOImpl;
template <class Parameter> struct Name;
} // namespace Parameters
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------

LE_OPTIMIZE_FOR_SIZE_BEGIN()

class SpectrumWorx;

#if LE_SW_ENGINE_WINDOW_PRESUM
namespace Engine
{
struct WindowSizeFactor;
}
#endif // LE_SW_ENGINE_WINDOW_PRESUM

////////////////////////////////////////////////////////////////////////////////
///
/// \enum PresetProblem
///
/// \brief Everything that can be wrong with a preset, and where it goes.
///
/// \note These were `GUI::warningMessageBox` calls made from inside the engine,
/// one dialog per problem. Two things wrong with that. It is a layering
/// inversion -- `sw-dsp` reaching up into the GUI -- and, more concretely, a
/// 2009-2011 preset does not mention parameters that the 2016 effects grew: the
/// 303 committed factory presets raise **806** MissingParameter reports between
/// them, which as dialogs is a wall of them in front of a user who opened a
/// bank.
///
///   So the preset layer says what happened and the caller decides. The default
/// reporter is still the message box, so a plugin behaves as it did; the corpus
/// test counts instead, which is how the number above is known.
///
////////////////////////////////////////////////////////////////////////////////

enum struct PresetProblem : std::uint8_t
{
    LoadFailed,         ///< not a preset, or the parse failed
    SaveFailed,         ///< the file could not be written
    UnknownEffect,      ///< names an effect this build does not have
    EffectNotAvailable, ///< names an effect this edition excludes
    MissingParameter,   ///< the effect has a parameter the preset does not mention
    TempoSyncedLFOWithoutTempo
};

/// \param detail the effect or parameter name where there is one, else empty.
using PresetProblemReporter = void (*)(PresetProblem, std::string_view detail);

/// \brief Installs a reporter, returning the previous one. `[main-thread]`, and
/// deliberately not a stack: a caller that wants nesting keeps what this
/// returned and puts it back.
PresetProblemReporter setPresetProblemReporter(PresetProblemReporter);

void reportPresetProblem(PresetProblem, std::string_view detail = {});

////////////////////////////////////////////////////////////////////////////////
///
/// \struct PresetHeader
///
////////////////////////////////////////////////////////////////////////////////

struct PresetHeader
{
    static unsigned int const maxCommentLength = 256;

    PresetHeader(juce::String const &comment);

    char version[8];
    char timeStamp[64];
    char comment[maxCommentLength];

    void setCurrentTime();

    struct AttributeNames
    {
        static char const version[];
        static char const timeStamp[];
        static char const comment[];
    } attributeNames;
}; // struct PresetHeader

////////////////////////////////////////////////////////////////////////////////
///
/// \class Preset
///
////////////////////////////////////////////////////////////////////////////////

class Preset
{
  public:
    Preset(Preset const &) = delete; // makes non-copyable
    Preset &operator=(Preset const &) = delete;

    using InMemoryPresetBuffer = std::array<char, 4096>;

  public:
    Preset() = default;
    explicit Preset(char const *const pBuffer) { loadFrom(pBuffer); }

    /// \brief Parses \p pBuffer, repairing the 2016 writer's illegal element
    /// names if it has to. See mangleName().
    ///
    /// \note Plain `bool`. It was `std::true_type` whenever exceptions were on
    /// -- which is always -- so `loadFrom(...) != true` at the one call site was
    /// a comparison that could not fail, and a malformed preset was reported by
    /// RapidXML *throwing* instead. TinyXML says so in its return value.
    ///                                       (31.07.2026.) (SW port)
    bool loadFrom(char const *pBuffer);

    /// \brief Prints the preset into \p buffer.
    /// \return the number of bytes written including the terminator, or 0 if
    /// the preset does not fit. RapidXML's printer could not tell you: it took
    /// a bare `char *`, and the 2016 sources already record that five TuneWorx
    /// modules overrun the 4096 bytes every caller hands it.
    unsigned int saveTo(std::span<char> buffer) const;

    void getHeader(PresetHeader &) const;
    void setHeader(PresetHeader const &);

    std::string_view getComment() const;

    TiXmlDocument &xml() { return document_; }
    TiXmlDocument const &xml() const { return document_; }

    TiXmlElement &root();
    TiXmlElement const &root() const;

    void reset() { document_.Clear(); }

    /// \note A preset arrives as a NUL-terminated buffer. Where it comes from is
    /// presetFile.hpp's business, not this header's.
    using InMemoryPreset = std::unique_ptr<char[]>;

    static void reportPresetLoadingError();

  private:
    TiXmlDocument document_;
}; // class Preset

////////////////////////////////////////////////////////////////////////////////
///
/// \class PresetHandler
///
////////////////////////////////////////////////////////////////////////////////

class PresetHandler
{
  protected:
    PresetHandler(Preset &preset) : preset_(preset) {}

  protected:
    ////////////////////////////////////////////////////////////////////////////
    // Parameter name <-> XML name.
    //
    // \note A parameter's or effect's name is used verbatim as an element or
    // attribute name, and three things they are called are not legal XML names:
    // a space ("Start frequency"), a leading digit -- TuneWorx's twelve
    // semitones are called "1" .. "12" -- and the punctuation in
    // "Center (LFO me!)" and the seven "... (pvd)" effects. The 2016 writer
    // replaced the space and nothing else, and got away with it because
    // RapidXML's fastest parse mode never checks a name. TinyXML does, and 25 of
    // the 303 factory presets do not parse until read through
    // repairLegacyElementNames().
    //
    // \note Not invertible, and it does not need to be: nothing unmangles. A
    // preset is matched to an effect by mangling the effect's name and comparing,
    // which is why a mangling that maps "(" and " " to the same character is
    // still unambiguous.
    //
    // \note The 2016 originals returned a view into RapidXML's arena, because
    // every string an attribute referred to had to outlive the document.
    // TinyXML copies what it is given, so these own what they return and the
    // arena, the `allocateString` that fed it and the lifetime rule that came
    // with them are all gone.
    //                                        (31.07.2026.) (SW port)
    ////////////////////////////////////////////////////////////////////////////

    static std::string mangleName(std::string_view parameterName);

    Preset &preset() { return preset_; }
    Preset const &preset() const { return preset_; }

    TiXmlDocument &xml() { return preset().xml(); }
    TiXmlDocument const &xml() const { return preset().xml(); }

    using LFO = Parameters::LFOImpl;

  protected:
    friend class LFODataSaver;

    static std::string makeString(std::string_view const source) { return std::string(source); }
    template <typename T> static std::string makeString(T const binarySource)
    {
        std::array<char, Utility::RequiredStringStorage<T>::value> buffer;
        auto const numberOfCharacters(Utility::lexical_cast(
            std::is_enum<T>::value ? static_cast<std::uint8_t>(binarySource) : binarySource,
            buffer.data()));
        LE_ASSUME(numberOfCharacters <= buffer.size());
        return std::string(buffer.data(), numberOfCharacters);
    }

  private:
    static std::string fixSpaces(std::string_view input, char searchFor, char replaceWith);

  private:
    Preset &preset_;
}; // class PresetHandler

template <> std::string PresetHandler::makeString<bool>(bool);

////////////////////////////////////////////////////////////////////////////////
///
/// \class ParametersLoader
///
////////////////////////////////////////////////////////////////////////////////

LE_IMPL_NAMESPACE_BEGIN(Engine)
class ModuleChainImpl;
class ModuleProcessorImpl;
LE_IMPL_NAMESPACE_END(Engine)
class AutomatedModuleChain;

class ParametersLoader : private PresetHandler
{
  public:
    ParametersLoader(Preset const &);

#ifdef LE_SW_SDK_BUILD
    typedef Engine::ModuleChainImpl ModuleChain;
#else
    typedef AutomatedModuleChain ModuleChain;
#endif // LE_SW_SDK_BUILD

    ModuleChain loadModuleChain(ModuleChain &currentChain);

    std::string_view getSampleFileName();

    bool syncedLFOFound() const { return syncedLFOFound_; }

    bool isPre27Preset() const;

    template <typename T>
    LE_NOINLINE std::optional<T> getSimpleParameterValue(char const *const parameterName) const
    {
        return getParameterValue<T>(getParameterAttribute(parameterName), parameterName);
    }

    template <typename T>
    std::optional<T> getLFOParameterValue(char const *const parameterName, LFO &lfo) const
    {
        auto const pParameterNode(getParameterNode(parameterName));
        if (pParameterNode)
        {
            if (loadLFO(*pParameterNode, lfo))
                return std::nullopt;
            else
                return getParameterValue<T>(pParameterNode->GetText(), parameterName);
        }
        else
        {
            // Implementation note:
            //   Fallback to handle old presets where not all module parameters
            // were LFO-able parameters.
            //                                (14.07.2011.) (Domagoj Saric)
            return getSimpleParameterValue<T>(parameterName);
        }
    }

  public: // For-each functor interface.
    using result_type = void;
    using const_qualified_lfo_t = LFO;

#if LE_SW_ENGINE_WINDOW_PRESUM
    result_type operator()(Engine::WindowSizeFactor &) const;
#endif // LE_SW_ENGINE_WINDOW_PRESUM

    template <class Parameter> void operator()(Parameter &parameter) const
    {
        using binary_type = typename Parameter::binary_type;
        std::optional<binary_type> const parameterValue(
            getSimpleParameterValue<binary_type>(LE::Parameters::Name<Parameter>::string_));
        if (parameterValue.has_value() && parameter.isValidValue(*parameterValue))
            parameter.setValue(*parameterValue);
    }

    template <class Parameter> void operator()(Parameter &parameter, LFO &lfo) const
    {
        using binary_type = typename Parameter::binary_type;
        std::optional<binary_type> const parameterValueWithoutLFO(getLFOParameterValue<binary_type>(
            Parameters::Name<Parameter>::string_, lfo, &parameter));
        if (parameterValueWithoutLFO.has_value() &&
            parameter.isValidValue(*parameterValueWithoutLFO))
            parameter.setValue(*parameterValueWithoutLFO);
    }

  private:
    /// \note One overload over a plain C string, where there were two over a
    /// RapidXML base class: TinyXML's attribute value and element text arrive as
    /// `char const *` already, and `TiXmlElement::Value()` is the element's
    /// *name*, so a shared base would have read the wrong thing for one of them.
    template <typename T>
    std::optional<T> getParameterValue(char const *const text,
                                       char const *const parameterName) const
    {
        if (text)
            return Utility::lexical_cast<T>(text);
        warnAboutMissingParameter(parameterName);
        return std::nullopt;
    }

    char const *getParameterAttribute(char const *parameterName) const;
    TiXmlElement const *getParameterNode(char const *parameterName) const;

    bool loadLFO(TiXmlElement const &parameterNode, LFO &lfo) const;

    static void warnAboutMissingParameter(char const *parameterName);

    static std::int8_t effectIndexFromMangledName(std::string_view mangledName);

    char const *currentMangledEffectName() const;

    TiXmlElement const &parameters() const
    {
        LE_ASSUME(pParameters_);
        return *pParameters_;
    }

    bool switchedToModuleParameters() const;

  private:
    TiXmlElement const *LE_RESTRICT pParameters_;

    mutable bool syncedLFOFound_;

#ifdef LE_SW_SDK_BUILD //...mrmlj...ugh...
    friend class Engine::ModuleProcessorImpl;
#else
    template <class PresetConsumer>
    friend bool loadPreset(char *inMemoryPreset, bool ignoreExternalSample, juce::String *pComment,
                           PresetConsumer);
#endif
}; // class ParametersLoader

#ifndef LE_SW_SDK_BUILD
////////////////////////////////////////////////////////////////////////////////
///
/// \class SavedPreset
///
/// \brief A preset being built: the three fixed nodes every one of them has.
///
/// \note Was `PresetWithPreallocatedFixedNodes`, which held the header node,
/// the two section nodes and *five module nodes* as members -- because RapidXML
/// allocates from an arena and does not own what you append, so pre-allocating
/// them was how you avoided the allocation. TinyXML owns its children, so a node
/// held by value cannot be linked into a document at all; the five module nodes
/// and the `moduleNodesEnd()` walk over them go with the class.
///                                           (31.07.2026.) (SW port)
///
////////////////////////////////////////////////////////////////////////////////

class SavedPreset : public Preset
{
  public:
    SavedPreset();

    void setHeader(PresetHeader const &);

    TiXmlElement &globalParametersNode() { return *pGlobalParametersNode_; }
    TiXmlElement &moduleParametersNode() { return *pModuleParametersNode_; }

  private:
    /// \note Owned by the document, which deletes them.
    TiXmlElement *pGlobalParametersNode_;
    TiXmlElement *pModuleParametersNode_;
}; // class SavedPreset

////////////////////////////////////////////////////////////////////////////////
///
/// \class ParametersSaver
///
////////////////////////////////////////////////////////////////////////////////

class ParametersSaver : private PresetHandler
{
  public:
    ParametersSaver(SavedPreset &);

    void saveEffectModuleChain(AutomatedModuleChain const &);

    //...mrmlj...temporarily reverting to old code for the 2.1 release...
    //void setSampleFileName( juce::String const & sampleFileName );
    void setSampleFileName(std::string_view const &sampleFileName);

    unsigned int saveTo(std::span<char> buffer) const;

  public: // For-each functor interface.
    using result_type = void;
    using const_qualified_lfo_t = LFO const;

    template <class Parameter> void operator()(Parameter const &parameter) const
    {
        const_cast<ParametersSaver &>(*this). //...mrmlj...because of forEach()...
            saveParameter<typename Parameter::param_type>(LE::Parameters::Name<Parameter>::string_,
                                                          parameter.getValue());
    }
    template <class Parameter> void operator()(Parameter const &parameter, LFO const &lfo) const
    {
        const_cast<ParametersSaver &>(*this). //...mrmlj...because of forEach()...
            saveParameter<typename Parameter::param_type>(LE::Parameters::Name<Parameter>::string_,
                                                          parameter.getValue(), lfo);
    }

    template <typename T>
    void LE_NOINLINE saveParameter(char const *const parameterName, T const parameterValue)
    {
        saveParameter(parameterName, makeString<T>(parameterValue));
    }

    template <typename T>
    void LE_NOINLINE saveParameter(char const *const parameterName, T const parameterValue,
                                   LFO const &parameterLFO)
    {
        saveParameter(parameterName, makeString<T>(parameterValue), parameterLFO);
    }

  private:
    void saveParameter(char const *parameterName, std::string const &parameterValue);
    void saveParameter(char const *parameterName, std::string const &parameterValue,
                       LFO const &parameterLFO);

    TiXmlElement &parameters()
    {
        LE_ASSUME(pParametersNode_);
        return *pParametersNode_;
    }

    SavedPreset &preset() { return static_cast<SavedPreset &>(PresetHandler::preset()); }
    SavedPreset const &preset() const { return const_cast<ParametersSaver &>(*this).preset(); }

  private:
    /// Where saveParameter() writes: the globals node, then each module's in turn.
    TiXmlElement *LE_RESTRICT pParametersNode_;

    /// \note Whether saveEffectModuleChain() has run, which the assertions in
    /// saveTo() used to get from comparing pParametersNode_ against a
    /// pre-allocated array's bounds.
    bool moduleChainSaved_{false};
}; // class ParametersSaver
#endif // LE_SW_SDK_BUILD

#ifndef _MSC_VER
LE_IMPL_NAMESPACE_BEGIN(Engine)
class ModuleNode;
template <class ActualModule> ActualModule &actualModule(ModuleNode &);
LE_IMPL_NAMESPACE_END(Engine)
namespace GlobalParameters
{
struct Parameters;
}
#endif // _MSC_VER

using char_t = juce::String::CharPointerType::CharType;

template <class PresetConsumer>
LE_COLD bool loadPreset(char *LE_RESTRICT const inMemoryPreset, bool const ignoreExternalSample,
                        juce::String *LE_RESTRICT const pComment, PresetConsumer const consumer)
{
    {
        Preset preset;
        if (!preset.loadFrom(inMemoryPreset))
        {
            Preset::reportPresetLoadingError();
            return false;
        }

        if (pComment)
        {
            auto const comment(preset.getComment());
            *pComment =
                juce::String::fromUTF8(comment.begin(), static_cast<unsigned int>(comment.size()));
        }

        ParametersLoader parametersLoader(preset);

        auto loader(consumer.presetLoader(ignoreExternalSample));

        if (loader.wantsSampleFile())
        {
            // Implementation note:
            //   The sample file name must be fetched before switching to module
            // parameters (see the implementation of the
            // ParametersLoader::getSampleFileName() member function).
            //                                (15.12.2011.) (Domagoj Saric)
            /// \todo Clean up this spaghetti.
            ///                               (15.12.2011.) (Domagoj Saric)
            loader.setSample(parametersLoader.getSampleFileName());
        }

        GlobalParameters::Parameters newParameters;
        LE::Parameters::forEach(newParameters, parametersLoader);
        auto &currentChain(loader.targetChain());
        //...mrmlj...clang's early template instantiation...AutomatedModuleChain newChain;
        typename std::remove_reference<decltype(currentChain)>::type newChain;
        {
            auto const lock(loader.processingLock()); //...mrmlj...
            newChain = parametersLoader.loadModuleChain(currentChain);
            LE::Utility::ignoreUnused(lock);
        }
#ifndef LE_SW_SDK_BUILD
        if (loader.onlySetParameters())
        {
            loader.targetGlobalParameters() = newParameters;
            currentChain = std::move(newChain);
            return true;
        }
#endif // LE_SW_SDK_BUILD

        {
            auto const automationBlocker(loader.automationBlocker());
            if (!loader.setNewGlobalParameters(newParameters))
            {
                Preset::reportPresetLoadingError();
                return false;
            }
            LE::Utility::ignoreUnused(automationBlocker);
        }

        auto const initialiseModule(loader.moduleInitialiser());

        using Module = typename PresetConsumer::Module;
        std::uint8_t moduleIndex(0);
        std::for_each(newChain.begin(), newChain.end(), [&](Engine::ModuleNode &module) {
            if (!initialiseModule(Engine::actualModule<Module>(module), moduleIndex++))
                newChain.remove(module);
        });
        {
            auto const lock(loader.processingLock()); //...mrmlj...
            currentChain = std::move(newChain);
            LE_ASSERT(newChain.size() == 0);
            LE_ASSERT(currentChain.size() == moduleIndex);
            LE::Utility::ignoreUnused(lock);
        }
        loader.moduleChainFinished(moduleIndex, parametersLoader.syncedLFOFound());
        return true;
    }

    /// \todo Add MIDI support.
    ///                                       (16.12.2009.) (Domagoj Saric)
} // loadPreset()

#ifndef LE_SW_SDK_BUILD
class Program;

/// \note The `juce::File` overloads of these two -- and the `loadPreset` that
/// reads a file before parsing it -- are in presetFile.hpp. This translation
/// unit opens no files, which is what `LE_NO_PRESETS` used to stand in for.
unsigned int savePreset(std::span<char> data, juce::File const &externalSampleFile,
                        juce::String const &comment, Program const &);
#endif // !LE_SW_SDK_BUILD

LE_OPTIMIZE_FOR_SIZE_END()

//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif
