////////////////////////////////////////////////////////////////////////////////
///
/// presets.cpp
/// -----------
///
/// Copyright (c) 2009 - 2016. Little Endian Ltd.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#include "presets.hpp"

#ifdef LE_SW_SDK_BUILD
#include "le/spectrumworx/engine/moduleImpl.hpp"
#else
#include "configuration/versionConfiguration.hpp"
#include "core/automatedModuleChain.hpp"
#include "core/modules/factory.hpp"
#include "core/modules/moduleDSPAndGUI.hpp"
#endif // LE_SW_SDK_BUILD

#include "le/math/conversion.hpp"
#include "le/math/math.hpp"
#include "le/parameters/parametersUtilities.hpp"
#include "le/parameters/lfo.hpp"
#include "le/parameters/uiElements.hpp" //...mrmlj...only for the warnAboutMissingParameter() temporary workaround
#include "le/spectrumworx/effects/configuration/effectNames.hpp"
#include "le/spectrumworx/effects/configuration/includedEffects.hpp"
#include "le/utility/countof.hpp"
#include "le/utility/tracePrivate.hpp"

#include "le/utility/assert.hpp"
#include "le/utility/ignoreUnused.hpp"
#include "le/utility/intrusivePtr.hpp"
#include <algorithm>

#ifndef LE_EXCEPTION_ON
#include <csetjmp>
#include <optional>
#include <string_view>
#endif // LE_EXCEPTION_ON
//------------------------------------------------------------------------------
LE_OPTIMIZE_FOR_SIZE_BEGIN()

// CDATA Sections [XML Standards]
// http://msdn.microsoft.com/en-us/library/ms256076.aspx
// http://msdn.microsoft.com/en-us/library/ms256177.aspx

#ifdef RAPIDXML_NO_EXCEPTIONS
namespace rapidxml
{
#ifdef LE_EXCEPTION_ON
LE_NOINLINE LE_COLD void parse_error_handler(char const * /*what*/, void * /*where*/)
{
    throw std::exception();
}
#else
namespace
{
::jmp_buf preParseEnvironment;
}
LE_NOINLINE LE_COLD void parse_error_handler(char const *const what, void *const where)
{
    LE::Utility::ignoreUnused(what && where);
    LE_TRACE("SW preset parsing failed (%s @ %s).", what, static_cast<char const *>(where));
    longjmp(preParseEnvironment, EXIT_FAILURE);
}
#endif // LE_EXCEPTION_ON
} // namespace rapidxml
#endif // RAPIDXML_NO_EXCEPTIONS
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace GUI
{
void warningMessageBox(std::string_view title, std::string_view message, bool canBlock);
}
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------

#if defined(LE_SW_SDK_BUILD)
using PresetModule = Engine::ModuleDSP;
#else
using PresetModule = SW::Module;
#endif // LE_SW_SDK_BUILD

PresetHeader::PresetHeader(juce::String const &commentParam)
{
    LE_ASSERT(commentParam.length() < _countof(comment) - 1);

/// \note Two levels, so that the argument is macro-expanded before it is
/// stringized. Was BOOST_PP_STRINGIZE, which is the same two lines.
#define LE_STRINGIZE_(x) #x
#define LE_STRINGIZE(x) LE_STRINGIZE_(x)
    std::strcpy(version, LE_STRINGIZE(SW_VERSION_MAJOR) "." LE_STRINGIZE(SW_VERSION_MINOR)
#if SW_VERSION_PATCH
                             LE_STRINGIZE(SW_VERSION_PATCH)
#endif // SW_VERSION_PATCH
    );
#undef LE_STRINGIZE
#undef LE_STRINGIZE_
    setCurrentTime();
    commentParam.copyToUTF8(comment, sizeof(comment));
}

void PresetHeader::setCurrentTime()
{
#ifdef _MSC_VER
    SYSTEMTIME currentUTCTime;
    ::GetSystemTime(&currentUTCTime);
    unsigned int const dateCharsWritten(::GetDateFormatA(
        LOCALE_INVARIANT, 0, &currentUTCTime, "dd'.'MM'.'yyyy", timeStamp, _countof(timeStamp)));
    timeStamp[dateCharsWritten - 1] = ' ';
    unsigned int const timeCharsWritten(::GetTimeFormatA(
        LOCALE_INVARIANT, 0, &currentUTCTime, "HH':'mm", &timeStamp[dateCharsWritten],
        _countof(timeStamp) - (dateCharsWritten - 1)));
    LE_ASSERT((dateCharsWritten + timeCharsWritten) <= _countof(timeStamp));
    LE_ASSERT(std::strlen(timeStamp) == (dateCharsWritten - 1 + timeCharsWritten));
    LE::Utility::ignoreUnused(timeCharsWritten);
#else
    ::time_t const currentUTCTime(::time(nullptr));
    LE_VERIFY(
        ::strftime(timeStamp, sizeof(timeStamp), "%d.%m.%Y %H:%M", ::gmtime(&currentUTCTime)) > 0);
#endif // _MSC_VER
}

namespace
{
char const headerNodeName_[] = "SpectrumWorxPreset";
char const parametersNodeName_[] = "Parameters";
char const globalParametersNodeName_[] = "Global";
char const moduleParametersNodeName_[] = "Modules";
char const moduleNodeName_[] = "Module";
char const moduleIDAttributeName_[] = "ID";
char const sampleAttributeName_[] = "Sample";
} // namespace

char const PresetHeader::AttributeNames::version[] = "Version";
char const PresetHeader::AttributeNames::timeStamp[] = "LastModified";
char const PresetHeader::AttributeNames::comment[] = "Comment";

////////////////////////////////////////////////////////////////////////////////
//
// Preset::loadFrom()
// ------------------
//
////////////////////////////////////////////////////////////////////////////////
///
/// \throws ifdef  RAPIDXML_NO_EXCEPTIONS: std::exception (error parsing XML)
///         ifndef RAPIDXML_NO_EXCEPTIONS: rapidxml::parse_error
///
////////////////////////////////////////////////////////////////////////////////
// Implementation note:
//   We use destructive parsing (for entity translation) so the input buffer
// must be writable.
//                                            (18.11.2011.) (Domagoj Saric)
////////////////////////////////////////////////////////////////////////////////

#pragma warning(push)
#pragma warning(                                                                                   \
    disable : 4611) // Interaction between '_setjmpex' and C++ object destruction is non-portable.
Preset::load_result_t Preset::loadFrom(char *const pBuffer)
{
#ifndef LE_EXCEPTION_ON
    if (setjmp(rapidxml::preParseEnvironment))
        return std::false_type();
#endif // LE_EXCEPTION_ON
    xml().parse(const_cast<char *>(pBuffer));
    return std::true_type();
}
#pragma warning(pop)

#ifndef LE_SW_SDK_BUILD
unsigned int Preset::saveTo(char *const pBuffer)
{
//...mrmlj...an ugly temporary way to verify that the header was set before saving...
/// \note Braces, not parentheses. `PresetHeader dummyHeader( juce::String() );`
/// is the most vexing parse -- it declares a function -- so this check has
/// never once run: the first build to compile this file is the one that
/// rejected it.
///                                           (31.07.2026.) (SW port)
#ifndef NDEBUG
    PresetHeader dummyHeader{juce::String()};
    getHeader(dummyHeader);
#endif // NDEBUG
    // Implementation note:
    //   RapidXML does not null-terminate printed data yet expects it to be so
    // when parsing.
    //                                            (17.12.2009.) (Domagoj Saric)
    char *end(rapidxml::print(pBuffer, xml(), 0));
    *end++ = '\0';
    return static_cast<unsigned int>(end - pBuffer);
}
#endif // LE_SW_SDK_BUILD

Utility::XML::Element &Preset::root()
{
    auto const pHeaderNode(static_cast<Utility::XML::Element *>(preset_.first_node()));
    LE_ASSERT(pHeaderNode);
    LE_ASSERT(pHeaderNode->name() == headerNodeName_);
    return *pHeaderNode;
}

Utility::XML::Element const &Preset::root() const { return const_cast<Preset &>(*this).root(); }

namespace
{
void copyAndNullTerminate(Utility::XML::Element const &headerNode,
                          std::string_view const attributeName, char *const pTargetBuffer)
{
    auto const pHeaderAttribute(headerNode.attribute(attributeName));
    LE_ASSERT(pHeaderAttribute);
    auto const value(Utility::XML::value(*pHeaderAttribute));
    *std::copy(value.begin(), value.end(), pTargetBuffer) = '\0';
}
} // anonymous namespace

void Preset::getHeader(PresetHeader &header) const
{
    auto const &headerNode(root());
    copyAndNullTerminate(headerNode, header.attributeNames.version, header.version);
    copyAndNullTerminate(headerNode, header.attributeNames.timeStamp, header.timeStamp);
    copyAndNullTerminate(headerNode, header.attributeNames.comment, header.comment);
}

////////////////////////////////////////////////////////////////////////////////
//
// Preset::setHeader()
// -------------------
//
////////////////////////////////////////////////////////////////////////////////
///
/// Expects the header parameter to live until after the last call to saveTo().
///
/// \throws nothing
///
////////////////////////////////////////////////////////////////////////////////

void Preset::setHeader(PresetHeader const &header)
{
    auto &headerNode(root());
    headerNode.attribute(header.attributeNames.version)->value(header.version);
    headerNode.attribute(header.attributeNames.timeStamp)->value(header.timeStamp);
    headerNode.attribute(header.attributeNames.comment)->value(header.comment);
}

std::string_view Preset::getComment() const
{
    auto const &headerNode(root());
    auto const *const pCommentAttribute(
        headerNode.attribute(PresetHeader::AttributeNames::comment));
    LE_ASSERT(pCommentAttribute);
    return Utility::XML::value(*pCommentAttribute);
}

////////////////////////////////////////////////////////////////////////////////
//
// Problem reporting
// -----------------
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
LE_COLD void defaultPresetProblemReporter(PresetProblem const problem,
                                          std::string_view const detail)
{
    switch (problem)
    {
    case PresetProblem::LoadFailed:
        GUI::warningMessageBox(MB_ERROR, "Unable to load preset.", false);
        return;
    case PresetProblem::SaveFailed:
        GUI::warningMessageBox(MB_ERROR, "Unable to save preset.", true);
        return;
    case PresetProblem::UnknownEffect:
        GUI::warningMessageBox(MB_ERROR " unknown effect in preset.", detail, false);
        return;
    case PresetProblem::EffectNotAvailable:
        GUI::warningMessageBox(MB_WARNING " effect not available in this edition.", detail, false);
        return;
    case PresetProblem::MissingParameter:
        GUI::warningMessageBox("Missing parameter value in preset", detail, true);
        return;
    case PresetProblem::ExternalSampleIgnored:
        GUI::warningMessageBox(MB_WARNING,
                               "Loaded preset uses external sample files which are not "
                               "supported by this edition of SpectrumWorx.",
                               false);
        return;
    case PresetProblem::TempoSyncedLFOWithoutTempo:
        GUI::warningMessageBox(MB_WARNING,
                               "Loaded preset uses tempo-synced LFOs but the host does not "
                               "provide tempo information.",
                               false);
        return;
    }
}

PresetProblemReporter presetProblemReporter{&defaultPresetProblemReporter};
} // anonymous namespace

PresetProblemReporter setPresetProblemReporter(PresetProblemReporter const reporter)
{
    auto *const previous(presetProblemReporter);
    presetProblemReporter = reporter ? reporter : &defaultPresetProblemReporter;
    return previous;
}

LE_COLD void reportPresetProblem(PresetProblem const problem, std::string_view const detail)
{
    presetProblemReporter(problem, detail);
}

void Preset::reportPresetLoadingError()
{
    LE_TRACE("Unable to load preset.");
    reportPresetProblem(PresetProblem::LoadFailed);
}

#ifdef _DEBUG
namespace
{
void *tracingAllocator(std::size_t const size)
{
    char *const pMemory(new char[size]);
    LE_TRACE("RapidXML allocating %ul bytes at %p.", size, pMemory);
    return pMemory;
}
void tracingDeallocator(void *const pMemory)
{
    LE_TRACE("RapidXML freeing memory at %p.", pMemory);
    delete[] static_cast<char *>(pMemory);
}
} // anonymous namespace
#endif // _DEBUG

void Preset::setMemoryAllocationTracer()
{
#ifdef _DEBUG
    xml().set_allocator(&tracingAllocator, &tracingDeallocator);
#endif // _DEBUG
}

namespace
{
char const space = ' ';
char const mangledSpace = '_';
} // anonymous namespace

std::string_view PresetHandler::fixSpaces(std::string_view const input, char const searchFor,
                                          char const replaceWith) const
{
    LE_ASSERT(!input.empty());
    if (input.find(searchFor) == input.npos)
        return input;

    auto const inputSize(static_cast<std::uint16_t>(input.size()));
    auto *LE_RESTRICT const fixedInputBuffer(
        const_cast<PresetHandler &>(*this).allocateString(inputSize));
    auto *const pEnd(
        std::ranges::replace_copy(input, fixedInputBuffer, searchFor, replaceWith).out);
    return {fixedInputBuffer, static_cast<std::uint16_t>(pEnd - fixedInputBuffer)};
}

std::string_view PresetHandler::mangleSpaces(char const *const input) const
{
    return fixSpaces(input, space, mangledSpace);
}
std::string_view PresetHandler::unmangleSpaces(char const *const input) const
{
    return fixSpaces(input, mangledSpace, space);
}

char *PresetHandler::allocateString(unsigned int const size)
{
    LE_ASSUME(size);
    return xml().allocate_string(nullptr, size);
}

std::string_view PresetHandler::makeStringRef(std::string_view::const_iterator const source)
{
    return source;
}

template <> std::string_view PresetHandler::makeStringRef<bool>(bool const binarySource)
{
    static char const characters[] = {'0', '1'};
    char const &character(characters[binarySource]);
    return std::string_view(&character, 1);
}

ParametersLoader::ParametersLoader(Preset const &preset)
    : PresetHandler(const_cast<Preset &>(preset)), syncedLFOFound_(false)
{
    pParameters_ = preset.root().child(globalParametersNodeName_);
    if (!pParameters_)
#ifdef RAPIDXML_NO_EXCEPTIONS
        rapidxml::parse_error_handler
#else
        throw rapidxml::parse_error
#endif // RAPIDXML_NO_EXCEPTIONS
            ("Preset node not found", 0);
}

#ifdef LE_SW_SDK_BUILD //...mrmlj...
namespace Engine
{
LE::Utility::IntrusivePtr<PresetModule> createModule(std::uint8_t effectIndex);
}
#define MB_WARNING "SW SDK warning:"
#define MB_ERROR "SW SDK error:"
#endif // LE_SW_SDK_BUILD
LE_COLD ParametersLoader::ModuleChain ParametersLoader::loadModuleChain(ModuleChain &currentChain)
{
    LE_ASSERT_MSG(!switchedToModuleParameters(), "Already switched to module parameters.");

    {
        auto const pModuleParameters(preset().root().child(moduleParametersNodeName_));
#if 0
        if ( !pModuleParameters )
            RAPIDXML_PARSE_ERROR( "Module parameters node not found", nullptr );
#else
        LE_ASSERT_MSG(pModuleParameters, "Module parameters node not found");
        if (!pModuleParameters)
            return ModuleChain();
#endif
        pParameters_ = static_cast<Utility::XML::Element const *>(pModuleParameters->first_node());
    }

    ModuleChain newChain;
    std::int8_t const noModule(-1);
    std::uint8_t moduleIndex(0);
    while (pParameters_)
    {
        using namespace Effects;
        auto const effectName(currentEffectName());
        auto const effectIndex(Effects::effectIndex(effectName));
        bool const foundEffect(effectIndex != noModule);
        bool const effectEnabled(foundEffect && includedEffects[effectIndex]);
#ifdef LE_SW_FULL
        LE_ASSUME(effectEnabled == true);
#endif // LE_SW_FULL
        if (foundEffect && effectEnabled)
        {
            LE_ASSUME(effectIndex >= 0);
            using namespace Engine;
            auto pPreexistingModule(std::find_if(
                currentChain.begin(), currentChain.end(), [=](ModuleNode const &module) {
                    return actualModule<PresetModule>(module).effectTypeIndex() == effectIndex;
                }));
            bool const preexistingModule(!currentChain.isEnd(pPreexistingModule));
            auto pModule(preexistingModule ? &actualModule<PresetModule>(*pPreexistingModule)
#ifdef LE_SW_SDK_BUILD
                                           : Engine::createModule(effectIndex)
#else
                                           : ModuleFactory::create<PresetModule>(effectIndex)
#endif // LE_SW_SDK_BUILD
            );
            if (pModule)
            {
                if (preexistingModule)
                    currentChain.remove(*pModule);
                newChain.push_back(*pModule);
                pModule->loadPresetParameters(*this);
                ++moduleIndex;
            }
        }
        else
        {
            reportPresetProblem(foundEffect ? PresetProblem::EffectNotAvailable
                                            : PresetProblem::UnknownEffect,
                                effectName);
        }
        pParameters_ = static_cast<Utility::XML::Element const *>(pParameters_->next_sibling());
    }

#ifndef LE_SW_SDK_BUILD
    LE_ASSERT_MSG(moduleIndex <= SW::Constants::maxNumberOfModules,
                  "Preset loaded too many modules?");
#endif // LE_SW_SDK_BUILD
    return newChain;
}

bool ParametersLoader::switchedToModuleParameters() const
{
    return parameters().name() /*...mrmlj...== moduleParametersNodeName_*/ !=
           globalParametersNodeName_;
}

#if LE_SW_ENGINE_WINDOW_PRESUM
ParametersLoader::result_type
ParametersLoader::operator()(Engine::WindowSizeFactor &parameter) const
{
    using Parameter = Engine::WindowSizeFactor;
    using binary_type = Parameter::binary_type;
    auto const parameterName(Parameters::Name<Parameter>::string_);
    auto const pParameterAttribute(getParameterAttribute(parameterName));
    std::optional<binary_type> const parameterValue(
        (pParameterAttribute || !isPre27Preset())
            ? getParameterValue<binary_type>(pParameterAttribute, parameterName)
            : Engine::WindowSizeFactor::default_());
    if (parameterValue.has_value() && parameter.isValidValue(*parameterValue))
        parameter.setValue(*parameterValue);
}
#endif // LE_SW_ENGINE_WINDOW_PRESUM

std::string_view ParametersLoader::currentEffectName() const
{
    //...mrmlj...PresetHandler::fixSpaces() expects a terminated C string (see
    //...mrmlj...the related note at the function declaration site)...
    auto const terminatedCurrentMangledEffectName(currentMangledEffectName());
    *const_cast<char *>(terminatedCurrentMangledEffectName.end()) = '\0';
    return unmangleSpaces(terminatedCurrentMangledEffectName.begin());
}

std::string_view ParametersLoader::currentMangledEffectName() const
{
    LE_ASSERT_MSG(switchedToModuleParameters(), "Not yet switched to module parameters.");
    return parameters().name();
}

// sampleAttributeName_ contains no spaces
std::string_view ParametersLoader::getSampleFileName()
{
    LE_ASSERT_MSG(!switchedToModuleParameters(),
                  "Sample file name must be fetched before switching to module parameters.");
    auto const pSampleAttribute(getParameterAttribute(sampleAttributeName_));
    return pSampleAttribute ? Utility::XML::value(*pSampleAttribute) : std::string_view();
}

bool ParametersLoader::isPre27Preset() const
{
    auto const pVersion(preset().root().attribute("Version"));
    if (!pVersion)
        return true;
    float const versionValue(Utility::lexical_cast<float>(pVersion->value()));
    return versionValue < 2.7f;
}

namespace
{
class LFODataLoader
{
  public:
    LFODataLoader(rapidxml::xml_node<> const &parameterNode, Parameters::LFOImpl const &lfo)
        : parameterNode_(parameterNode), lfo_(lfo)
    {
    }

    template <class LFOParameter> void operator()(LFOParameter &element) const
    {
        using namespace Parameters;
        doLoad(name<LFOParameter>().begin(), name<LFOParameter>().size(), element);
    }

  private:
    template <class LFOParameter>
    void doLoad(char const *const elementName, std::size_t const elementNameSize,
                LFOParameter &element) const
    {
        auto const pElementAttribute(parameterNode_.first_attribute(elementName, elementNameSize));
        if (pElementAttribute)
        {
            element = lfo_.adjustValueFromPreset<LFOParameter>(
                static_cast<typename LFOParameter::value_type>(
                    Utility::lexical_cast<typename LFOParameter::binary_type>(
                        pElementAttribute->value())));
        }
        else
        {
            /// \note If the preset does not specify a specific LFO
            /// parameter we need to explicitly reset it to default in order
            /// to properly handle reused module instances (which might have
            /// the particular parameter set to a non-default value). This
            /// also covers the case of old/pre-synced-LFOs presets (for
            /// which the sync type parameter needs to be set to the default
            /// 'free' value).
            ///                           (09.10.2014.) (Domagoj Saric)
            element = LFOParameter::default_();
        }
    }

  private:
    rapidxml::xml_node<> const &parameterNode_;
    Parameters::LFOImpl const &lfo_;
}; // class LFODataLoader
} // anonymous namespace

bool ParametersLoader::loadLFO(Utility::XML::Element const &parameterNode, LFO &lfo) const
{
    /// \todo Clean up this coupling by removing any special internal LFO class
    /// knowledge from this function/class.
    ///                                       (18.02.2011.) (Domagoj Saric)

    // Implementation note:
    //   The LFO parameters have to be loaded in reverse order in order to load
    // the SyncTypes parameter before the PeriodScale parameter because the
    // LFO::adjustvalueFromPreset<PeriodScale>() function assumes the SyncTypes
    // parameter to already be loaded/set.
    //                                        (18.02.2011.) (Domagoj Saric)
    LE::Parameters::forEachReversed(lfo.parameters(), LFODataLoader(parameterNode, lfo));
    syncedLFOFound_ |= lfo.enabled() & (lfo.syncTypes() != LFO::Free);
    return lfo.enabled();
}

LE_NOINLINE Utility::XML::Attribute const *
ParametersLoader::getParameterAttribute(char const *const parameterName) const
{
    return parameters().attribute(mangleSpaces(parameterName));
}

LE_NOINLINE Utility::XML::Element const *
ParametersLoader::getParameterNode(char const *const parameterName) const
{
    return parameters().child(mangleSpaces(parameterName));
}

LE_COLD void ParametersLoader::warnAboutMissingParameter(char const *const pParameterName)
{
    LE_ASSERT(pParameterName);
    std::string_view const parameterName(pParameterName);
    if (
#ifdef LE_PV_USE_TSS
        (parameterName != "Transient sensitivity") &&
#endif // LE_PV_USE_TSS
        (parameterName != "Gate"))
    {
        LE_TRACE_LOGONLY("Missing parameter value in preset (%s).", pParameterName);
        reportPresetProblem(PresetProblem::MissingParameter, parameterName);
    }
}

#ifndef LE_SW_SDK_BUILD
PresetWithPreallocatedFixedNodes::PresetWithPreallocatedFixedNodes()
    : headerNode_(headerNodeName_), globalParametersNode_(globalParametersNodeName_),
      moduleParametersNode_(moduleParametersNodeName_)
{
    xml().append_node(&headerNode_);
    headerNode_.append_node(&globalParametersNode_);
    headerNode_.append_node(&moduleParametersNode_);
}

void PresetWithPreallocatedFixedNodes::setHeader(PresetHeader const &header)
{
    headerNode_.append_attribute(
        xml().allocate_attribute(header.attributeNames.version, header.version));
    headerNode_.append_attribute(
        xml().allocate_attribute(header.attributeNames.timeStamp, header.timeStamp));
    headerNode_.append_attribute(
        xml().allocate_attribute(header.attributeNames.comment, header.comment));
}

ParametersSaver::ParametersSaver(PresetWithPreallocatedFixedNodes &preset)
    : PresetHandler(preset), pParametersNode_(&preset.globalParametersNode())
{
}

unsigned int ParametersSaver::saveTo(char *const pBuffer)
{
    LE_ASSERT_MSG(pParametersNode_ != &preset().globalParametersNode(),
                  "Module chain parameters not yet saved/parsed.");
    LE_ASSERT((pParametersNode_ == moduleNodesEnd()) || !pParametersNode_->parent() ||
              (pParametersNode_->parent() == &preset().moduleParametersNode()));
    return preset().saveTo(pBuffer);
}

void ParametersSaver::saveEffectModuleChain(AutomatedModuleChain const &moduleChain)
{
    LE_ASSERT_MSG(pParametersNode_ == &preset().globalParametersNode(),
                  "Already switched to modules."); //...mrmlj...
    pParametersNode_ = &preset().moduleNodes().front();
    moduleChain.forEach<PresetModule>([&](PresetModule const &module) {
        auto const name(mangleSpaces(Effects::effectName(module.effectTypeIndex())));
        pParametersNode_->setName(name);
        module.savePresetParameters(*this);
        preset().moduleParametersNode().append_node(pParametersNode_++);
        LE_ASSERT(pParametersNode_ == moduleNodesEnd() || pParametersNode_->name().empty());
    });
}

void ParametersSaver::saveParameter(char const *const parameterName,
                                    std::string_view const parameterValue)
{
    std::string_view const fixedParameterName(mangleSpaces(parameterName));
    parameters().append_attribute(
        xml().allocate_attribute(fixedParameterName.begin(), parameterValue.begin(),
                                 fixedParameterName.size(), parameterValue.size()));
}

// ...mrmlj...cannot put LFODataSaver into the anonymous namespace because it is
// declared as friend in the PresetHandler class...clean this up...
//namespace
//{
class LFODataSaver
{
  public:
    using LFO = Parameters::LFOImpl;

    LFODataSaver(PresetHandler &handler, rapidxml::xml_node<> &parameterNode, LFO const &lfo)
        : handler_(handler), parameterNode_(parameterNode), lfo_(lfo)
    {
    }

#pragma warning(push)
#pragma warning(disable : 4127) // Conditional expression is constant.

    template <class LFOParameter> void operator()(LFOParameter const &element) const
    {
        // Implementation note:
        //   A preset with a bank of five TuneWorx modules breaches the 4096
        // bytes limit. As a workaround we save only LFO parameters that
        // have non default values to reduce the size of the presets.
        //                                (21.07.2011.) (Domagoj Saric)
        // Implementation note:
        //   The SyncTypes parameter has to be saved always, otherwise the
        // preset gets loaded as an 'old'/'pre-synced-LFOs' preset (with the
        // default sync type set to 'Free' for all parameters of all
        // modules, see the note in ParametersLoader::loadLFO() for more
        // info).
        //                                (26.07.2011.) (Domagoj Saric)
        if (!std::is_same<LFOParameter, LFO::SyncTypes>::value &&
            Math::equal(element.getValue(), element.default_()))
            return;

        std::string_view const stringValue(
            handler_.makeStringRef(lfo_.adjustValueForPreset(element)));

        using namespace Parameters;
        parameterNode_.append_attribute(
            handler_.xml().allocate_attribute(name<LFOParameter>().begin(), stringValue.begin(),
                                              name<LFOParameter>().size(), stringValue.size()));
    }

#pragma warning(pop)

  private:
    PresetHandler &handler_;
    rapidxml::xml_node<> &parameterNode_;
    LFO const &lfo_;
};
//} // anonymous namespace

void ParametersSaver::saveParameter(char const *const parameterName,
                                    std::string_view const parameterValue, LFO const &parameterLFO)
{
    std::string_view const fixedParameterName(mangleSpaces(parameterName));

    rapidxml::xml_node<> &parameterNode(*xml().allocate_node(
        rapidxml::node_element, fixedParameterName.begin(), parameterValue.begin(),
        fixedParameterName.size(), parameterValue.size()));

    LE::Parameters::forEach(parameterLFO.parameters(),
                            LFODataSaver(*this, parameterNode, parameterLFO));

    parameters().append_node(&parameterNode);
}

/*
    ...mrmlj...temporarily reverting to old code for the 2.1 release...
void ParametersSaver::setSampleFileName( juce::String const & sampleFileName )
{
    std::size_t const sampleFileNameLength( sampleFileName.length() );
    char * const pSampleFileName( xml().allocate_string( sampleFileName, sampleFileNameLength + 1 ) );
    /// \todo Add checks here and in all similar places that an attribute is not
    /// saved more than once.
    ///                                       (03.02.2010.) (Domagoj Saric)
    saveParameter( sampleAttributeName_, std::string_view( pSampleFileName, sampleFileNameLength ) );
}*/

void ParametersSaver::setSampleFileName(std::string_view const &sampleFileName)
{
    /// \todo Add checks here and in all similar places that an attribute is not
    /// saved more than once.
    ///                                           (03.02.2010.) (Domagoj Saric)
    saveParameter(sampleAttributeName_, sampleFileName);
}

unsigned int savePreset(char *const data, juce::File const &externalSampleFile,
                        juce::String const &comment, Program const &program)
{
    PresetHeader const presetHeader(comment);
    PresetWithPreallocatedFixedNodes preset;
    ParametersSaver parametersSaver(preset);

    preset.setHeader(presetHeader);

    LE::Parameters::forEach(program.parameters(), parametersSaver);

    if (externalSampleFile != juce::File())
    {
        /*  ...mrmlj...temporarily reverting to old code for the 2.1 release...

        // Implementation note:
        //   For "known"/"factory default" samples (that we supply with
        // SpectrumWorx and that reside in the "Samples" folder we only save
        // the file name (so that the presets do not look 'weird' to users if
        // they open them in a text editor on a Mac that has a completely
        // different folder structure than Windows).
        //                                    (06.12.2010.) (Domagoj Saric)
        juce::String const sampleFileName
        (
        sample_.sampleFile().isAChildOf( GUI::rootPath().getChildFile( "Samples" ) )
        ? sample_.sampleFile().getFileName    ()
        : sample_.sampleFile().getFullPathName()
        );
        parametersSaver.setSampleFileName( sampleFileName );
        */
        juce::String const &sampleFileName(externalSampleFile.getFullPathName());
#if JUCE_STRING_UTF_TYPE == 8
        char const *const pSampleFileName(sampleFileName.toUTF8());
        unsigned int const sampleFileNameLength(std::strlen(pSampleFileName));
#else
        unsigned int const sampleFileNameLength(
            static_cast<unsigned int>(sampleFileName.getNumBytesAsUTF8()));
        char *const pSampleFileName(static_cast<char *>(_alloca(sampleFileNameLength + 1)));
        LE_VERIFY(sampleFileName.copyToUTF8(pSampleFileName, sampleFileNameLength + 1) ==
                  signed(sampleFileNameLength + 1));
#endif // JUCE_STRING_UTF_TYPE
        parametersSaver.setSampleFileName(std::string_view(pSampleFileName, sampleFileNameLength));
    }

    parametersSaver.saveEffectModuleChain(program.moduleChain());

    return preset.saveTo(data);
}

#endif // LE_SW_SDK_BUILD

//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------

LE_OPTIMIZE_FOR_SIZE_END()
