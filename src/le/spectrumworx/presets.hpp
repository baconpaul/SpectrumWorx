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
#include "le/parameters/fusionAdaptors.hpp"
#include "le/spectrumworx/engine/parameters.hpp"
#endif // _MSC_VER
#include "le/utility/countof.hpp"
#include "le/utility/lexicalCast.hpp"
#include "le/utility/platformSpecifics.hpp"
#include "le/utility/tchar.hpp"
#include "le/utility/trace.hpp"
#include "le/utility/xml.hpp"

#include <optional>

#include <boost/fusion/algorithm/iteration/for_each_fwd.hpp>
#include <boost/mpl/bool_fwd.hpp>
#include "le/utility/intrusivePtr.hpp"
#include "le/utility/ignoreUnused.hpp"
#include <string_view>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
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
#ifdef LE_EXCEPTION_ON
    typedef boost::mpl::true_ load_result_t;
#else
    typedef bool load_result_t;
#endif // LE_EXCEPTION_ON

    Preset() { setMemoryAllocationTracer(); }
#ifdef LE_EXCEPTION_ON
    Preset(char *const pBuffer)
    {
        setMemoryAllocationTracer();
        loadFrom(pBuffer);
    }
#endif // LE_EXCEPTION_ON

    load_result_t loadFrom(char *pBuffer);
    unsigned int saveTo(char *pBuffer);

    void getHeader(PresetHeader &) const;
    void setHeader(PresetHeader const &);

    std::string_view getComment() const;

    Utility::XML::Document &xml() { return preset_; }
    Utility::XML::Document const &xml() const { return preset_; }

    Utility::XML::Element &root();
    Utility::XML::Element const &root() const;

    void reset() { xml().clear(); }

    using InMemoryPreset = std::unique_ptr<char[]>;
    static InMemoryPreset loadIntoMemory(juce::File const &);

    static void reportPresetLoadingError();

  private:
    void setMemoryAllocationTracer();

  private:
    Utility::XML::Document preset_;
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
    std::string_view mangleSpaces(char const *input) const;
    std::string_view unmangleSpaces(char const *input) const;

    char *allocateString(unsigned int size);

    Preset &preset() { return preset_; }
    Preset const &preset() const { return preset_; }

    Utility::XML::Document &xml() { return preset().xml(); }
    Utility::XML::Document const &xml() const { return preset().xml(); }

    using LFO = Parameters::LFOImpl;

  protected:
    friend class LFODataSaver;

    static std::string_view const &makeStringRef(std::string_view const &source) { return source; }
    static std::string_view makeStringRef(std::string_view::const_iterator source);
    template <typename T> std::string_view makeStringRef(T const binarySource)
    {
        auto const valueBufferSize(Utility::RequiredStringStorage<T>::value);
        char *LE_RESTRICT const buffer(allocateString(valueBufferSize));
        unsigned int const numberOfCharacters(Utility::lexical_cast(
            std::is_enum<T>::value ? static_cast<std::uint8_t>(binarySource) : binarySource,
            buffer));
        LE_ASSUME(numberOfCharacters <= valueBufferSize);
        return std::string_view(buffer, numberOfCharacters);
    }

  private:
    std::string_view fixSpaces(std::string_view input, char searchFor, char replaceWith) const;

  private:
    Preset &preset_;
}; // class PresetHandler

template <> std::string_view PresetHandler::makeStringRef<bool>(bool);

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
        auto const pParameterAttribute(getParameterAttribute(parameterName));
        return getParameterValue<T>(pParameterAttribute, parameterName);
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
                return getParameterValue<T>(pParameterNode, parameterName);
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
    template <typename T>
    std::optional<T> getParameterValue(Utility::XML::Object const *const pXMLElement,
                                       char const *const parameterName) const
    {
        if (pXMLElement)
        {
            return Utility::lexical_cast<T>(pXMLElement->value());
        }
        warnAboutMissingParameter(parameterName);
        return std::nullopt;
    }

    Utility::XML::Attribute const *getParameterAttribute(char const *parameterName) const;
    Utility::XML::Element const *getParameterNode(char const *parameterName) const;

    bool loadLFO(Utility::XML::Element const &parameterNode, LFO &lfo) const;

    static void warnAboutMissingParameter(char const *parameterName);

    std::string_view currentEffectName() const;
    std::string_view currentMangledEffectName() const;

    Utility::XML::Element const &parameters() const
    {
        LE_ASSUME(pParameters_);
        return *pParameters_;
    }

    bool switchedToModuleParameters() const;

  private:
    Utility::XML::Element const *LE_RESTRICT pParameters_;

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
/// \class PresetWithPreallocatedNodes
///
////////////////////////////////////////////////////////////////////////////////

class PresetWithPreallocatedFixedNodes : public Preset
{
  private:
    using ModuleNodes = std::array<Utility::XML::Element, Constants::maxNumberOfModules>;

  public:
    PresetWithPreallocatedFixedNodes();

    void setHeader(PresetHeader const &);

    Utility::XML::Element &headerNode() { return headerNode_; }
    Utility::XML::Element &globalParametersNode() { return globalParametersNode_; }
    Utility::XML::Element &moduleParametersNode() { return moduleParametersNode_; }

    ModuleNodes &moduleNodes() { return moduleNodes_; }
    ModuleNodes const &moduleNodes() const { return moduleNodes_; }

  private:
    Utility::XML::Element headerNode_;
    Utility::XML::Element globalParametersNode_;
    Utility::XML::Element moduleParametersNode_;

    ModuleNodes moduleNodes_;
}; // class PresetWithPreallocatedFixedNodes

////////////////////////////////////////////////////////////////////////////////
///
/// \class ParametersSaver
///
////////////////////////////////////////////////////////////////////////////////

class ParametersSaver : private PresetHandler
{
  public:
    ParametersSaver(PresetWithPreallocatedFixedNodes &);

    void saveEffectModuleChain(AutomatedModuleChain const &);

    //...mrmlj...temporarily reverting to old code for the 2.1 release...
    //void setSampleFileName( juce::String const & sampleFileName );
    void setSampleFileName(std::string_view const &sampleFileName);

    unsigned int saveTo(char *pBuffer);

  public: // For-each functor interface.
    using result_type = void;
    using const_qualified_lfo_t = LFO const;

    template <class Parameter> void operator()(Parameter const &parameter) const
    {
        const_cast<ParametersSaver &>(*this). //...mrmlj...because of boost::fusion::for_each...
            saveParameter<typename Parameter::param_type>(LE::Parameters::Name<Parameter>::string_,
                                                          parameter.getValue());
    }
    template <class Parameter> void operator()(Parameter const &parameter, LFO const &lfo) const
    {
        const_cast<ParametersSaver &>(*this). //...mrmlj...because of boost::fusion::for_each...
            saveParameter<typename Parameter::param_type>(LE::Parameters::Name<Parameter>::string_,
                                                          parameter.getValue(), lfo);
    }

    template <typename T>
    void LE_NOINLINE saveParameter(char const *const parameterName, T const parameterValue)
    {
        saveParameter(parameterName, makeStringRef<T>(parameterValue));
    }

    template <typename T>
    void LE_NOINLINE saveParameter(char const *const parameterName, T const parameterValue,
                                   LFO const &parameterLFO)
    {
        saveParameter(parameterName, makeStringRef<T>(parameterValue), parameterLFO);
    }

  private:
    void saveParameter(char const *parameterName, std::string_view parameterValue);
    void saveParameter(char const *parameterName, std::string_view parameterValue,
                       LFO const &parameterLFO);

    Utility::XML::Element &parameters()
    {
        LE_ASSUME(pParametersNode_);
        return *pParametersNode_;
    }

    PresetWithPreallocatedFixedNodes &preset()
    {
        return static_cast<PresetWithPreallocatedFixedNodes &>(PresetHandler::preset());
    }
    PresetWithPreallocatedFixedNodes const &preset() const
    {
        return const_cast<ParametersSaver &>(*this).preset();
    }

    Utility::XML::Element const *moduleNodesEnd() const
    {
        /// \note Quick-workaround for 'secure STL' array iterators not being
        /// simple pointers.
        ///                                   (15.10.2015.) (Domagoj Saric)
        return &preset().moduleNodes().back() + 1;
    }

  private:
    Utility::XML::Element *LE_RESTRICT pParametersNode_;
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
#ifdef LE_EXCEPTION_ON
    try
    {
#endif // LE_EXCEPTION_ON
        Preset preset;
        if (preset.loadFrom(inMemoryPreset) != true)
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

#if defined(LE_SW_DISABLE_SIDE_CHANNEL)
        if (!parametersLoader.getSampleFileName().empty())
        {
            GUI::warningMessageBox(MB_WARNING,
                                   "Loaded preset uses external sample files which are not "
                                   "supported by this edition of SpectrumWorx.",
                                   false);
        }
#else  // "normal plugin build"
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
#endif // side channel handling

        GlobalParameters::Parameters newParameters;
        boost::fusion::for_each(newParameters, parametersLoader);
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
#ifdef LE_EXCEPTION_ON
    }
    catch (...)
    {
        Preset::reportPresetLoadingError();
        return false;
    }
#endif // LE_EXCEPTION_ON

    /// \todo Add MIDI support.
    ///                                       (16.12.2009.) (Domagoj Saric)
} // loadPreset()

#ifndef LE_SW_SDK_BUILD
template <class PresetConsumer>
bool LE_COLD loadPreset(juce::File const &presetFile, bool const ignoreExternalSample,
                        juce::String *const pComment, char_t const *const presetName,
                        PresetConsumer consumer)
{
    auto const pPresetData(Preset::loadIntoMemory(presetFile));
    if (!pPresetData.get())
        return false;
    consumer
        .notifyHostAboutPresetChangeBegin(); //...mrmlj...assumes host initiated change != loading from file
    bool const success(loadPreset(pPresetData.get(), ignoreExternalSample, pComment, consumer));
    if (success)
    {
        /// \note Setting the new preset name can be important with VST2.4 hosts
        /// because of the way preset change notifications are implemented under
        /// that protocol (hosts usually react to audioMasterUpdateDisplay only
        /// if a program's name has changed).
        ///                                   (12.09.2014.) (Domagoj Saric)
        copyToBuffer(presetName, consumer.program().name());
    }
    consumer.notifyHostAboutPresetChangeEnd();
    return success;
}

class Program;
void savePreset(juce::File const &, juce::File const &externalSampleFile,
                juce::String const &comment, Program const &);
unsigned int savePreset(char *const data, juce::File const &externalSampleFile,
                        juce::String const &comment, Program const &);
#endif // !LE_SW_SDK_BUILD

LE_OPTIMIZE_FOR_SIZE_END()

//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif
