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
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
//------------------------------------------------------------------------------
LE_OPTIMIZE_FOR_SIZE_BEGIN()

// CDATA Sections [XML Standards]
// http://msdn.microsoft.com/en-us/library/ms256076.aspx
// http://msdn.microsoft.com/en-us/library/ms256177.aspx

/// \note Malformed input used to leave here as an exception -- RapidXML threw a
/// `rapidxml::parse_error` from inside the parser and `loadPreset`'s catch-all
/// turned it into "unable to load", which meant a bad preset unwound through
/// whatever the caller happened to be holding. (A `setjmp`/`longjmp` pair stood
/// beside it for the no-exceptions build, which this one is not.) TinyXML
/// reports through `TiXmlDocument::Error()` and needs neither.
///                                           (31.07.2026.) (SW port)
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
/// \return whether the buffer parsed. Throws nothing.
///
/// \note The buffer is `char const *` now. RapidXML parsed destructively, in
/// place, and every string in the document pointed back into it -- so the buffer
/// had to be writable and had to outlive the document. TinyXML copies.
///
////////////////////////////////////////////////////////////////////////////////

namespace
{
char const mangledSpace = '_';

/// \name XML name rules
/// \note Deliberately narrower than the specification, which allows a great deal
/// of Unicode: what has to pass is the ASCII in 57 effect names and their
/// parameters, and what has to be rejected is everything TinyXML's own
/// `ReadName` rejects. `:` is legal and excluded anyway -- it means a namespace
/// and no name here wants one.
/// @{
bool isNameStart(char const character)
{
    return std::isalpha(static_cast<unsigned char>(character)) || (character == mangledSpace);
}

bool isNameCharacter(char const character)
{
    return std::isalnum(static_cast<unsigned char>(character)) || (character == mangledSpace) ||
           (character == '-') || (character == '.');
}
/// @}

/// \brief The name-to-XML-name mapping, as a free function so that the read-side
/// repair can apply exactly the same one.
///
/// \note Idempotent, which is what lets the repair run over a name that is
/// already fine and over one that is not without having to tell them apart.
std::string mangleXmlName(std::string_view const name)
{
    std::string mangled;
    mangled.reserve(name.size() + 1);
    for (auto const character : name)
        mangled += isNameCharacter(character) ? character : mangledSpace;

    if (mangled.empty() || !isNameStart(mangled.front()))
        mangled.insert(mangled.begin(), mangledSpace);
    return mangled;
}
} // anonymous namespace

namespace
{
////////////////////////////////////////////////////////////////////////////////
//
// repairLegacyElementNames()
// --------------------------
//
////////////////////////////////////////////////////////////////////////////////
///
/// \brief Rewrites every element name the 2016 writer emitted that is not a
/// legal XML name.
///
/// \note A parameter's or effect's name becomes an element name verbatim, and
/// the 2016 writer replaced spaces and nothing else. Three kinds of name got
/// through: `<1>` .. `<12>` (TuneWorx's semitones), `<Pitch_Shifter_(pvd)>` and
/// its six siblings, and `<Center_(LFO_me!)>`. No conforming parser will read
/// any of them. RapidXML's `parse_fastest` mode never checked a name, which is
/// why it went unnoticed for fifteen years and why 25 of the 303 committed
/// presets need this. The writer mangles properly now (mangleName), so this is
/// for files written before it did.
///
/// \note Runs only after a strict parse has already failed, so a well-formed
/// preset never comes through here and never pays for it. A raw `<` in text
/// would be malformed anyway, which is what makes the pattern safe to key on.
///
/// \return nothing if there was nothing to repair, so that a genuinely broken
/// preset is not parsed twice.
///
////////////////////////////////////////////////////////////////////////////////

std::optional<std::string> repairLegacyElementNames(char const *const pBuffer)
{
    std::string_view const source(pBuffer);

    auto const endsName([](char const character) {
        return (character == ' ') || (character == '\t') || (character == '\r') ||
               (character == '\n') || (character == '>') || (character == '/');
    });

    std::string repaired;

    /// \note Two cursors, not one. Reusing the scan position as the copy
    /// position drops everything before the first repaired tag -- which is the
    /// root element, so the retry parses a document with no root and reports
    /// "document empty" from three steps away.
    std::size_t copied(0);
    std::size_t scan(0);
    for (;;)
    {
        auto const tag(source.find('<', scan));
        if (tag == source.npos)
            break;

        auto nameStart(tag + 1);
        if ((nameStart < source.size()) && (source[nameStart] == '/'))
            ++nameStart;

        // A declaration, a comment or a doctype: not an element name.
        if ((nameStart >= source.size()) || (source[nameStart] == '?') ||
            (source[nameStart] == '!'))
        {
            scan = tag + 1;
            continue;
        }

        auto nameEnd(nameStart);
        while ((nameEnd < source.size()) && !endsName(source[nameEnd]))
            ++nameEnd;

        auto const name(source.substr(nameStart, nameEnd - nameStart));
        auto const mangled(mangleXmlName(name));
        if (mangled != name)
        {
            repaired.append(source, copied, nameStart - copied);
            repaired += mangled;
            copied = nameEnd;
        }
        scan = nameEnd;
    }

    if (repaired.empty())
        return std::nullopt;

    repaired.append(source, copied, source.npos);
    return repaired;
}
} // anonymous namespace

bool Preset::loadFrom(char const *const pBuffer)
{
    document_.Clear();
    document_.Parse(pBuffer);
    if (!document_.Error())
        return true;

    if (auto const repaired(repairLegacyElementNames(pBuffer)); repaired)
    {
        document_.Clear();
        document_.Parse(repaired->c_str());
        if (!document_.Error())
        {
            LE_TRACE_LOGONLY("SW: preset uses 2016 numeric element names; repaired on read.");
            return true;
        }
    }

    LE_TRACE("SW preset parsing failed (%s @ row %d).", document_.ErrorDesc(),
             document_.ErrorRow());
    return false;
}

#ifndef LE_SW_SDK_BUILD
////////////////////////////////////////////////////////////////////////////////
//
// Preset::saveTo()
// ----------------
//
////////////////////////////////////////////////////////////////////////////////
///
/// \note Tabs, and a trailing newline after the root: that is what the 2016
/// writer emitted and what the 303 committed presets look like, so it is what
/// TiXmlPrinter is told to emit. The terminator is written too, because the
/// 2016 writer put one on disk -- 193 of the 303 files end in a NUL byte.
///
////////////////////////////////////////////////////////////////////////////////

unsigned int Preset::saveTo(std::span<char> const buffer) const
{
    //...mrmlj...an ugly temporary way to verify that the header was set before saving...
    /// \note Braces, not parentheses. `PresetHeader dummyHeader( juce::String() );`
    /// is the most vexing parse -- it declares a function -- so this check had
    /// never once run: the first build to compile this file is the one that
    /// rejected it.
    ///                                       (31.07.2026.) (SW port)
#ifndef NDEBUG
    PresetHeader dummyHeader{juce::String()};
    getHeader(dummyHeader);
#endif // NDEBUG

    TiXmlPrinter printer;
    printer.SetIndent("\t");
    document_.Accept(&printer);

    auto const size(printer.Size() + 1 /*terminator*/);
    if (size > buffer.size())
    {
        LE_TRACE("SW: preset of %u bytes does not fit a %u byte buffer.",
                 static_cast<unsigned int>(size), static_cast<unsigned int>(buffer.size()));
        return 0;
    }

    *std::copy_n(printer.CStr(), printer.Size(), buffer.data()) = '\0';
    return static_cast<unsigned int>(size);
}
#endif // LE_SW_SDK_BUILD

TiXmlElement &Preset::root()
{
    auto *const pHeaderNode(document_.FirstChildElement(headerNodeName_));
    LE_ASSERT_MSG(pHeaderNode, "Preset has no root node.");
    return *pHeaderNode;
}

TiXmlElement const &Preset::root() const { return const_cast<Preset &>(*this).root(); }

namespace
{
void copyAndNullTerminate(TiXmlElement const &headerNode, char const *const attributeName,
                          char *const pTargetBuffer, std::size_t const capacity)
{
    auto const *const pValue(headerNode.Attribute(attributeName));
    LE_ASSERT(pValue);
    if (!pValue)
    {
        *pTargetBuffer = '\0';
        return;
    }
    std::string_view const value(pValue);
    auto const copied(std::min(value.size(), capacity - 1));
    *std::copy_n(value.begin(), copied, pTargetBuffer) = '\0';
}
} // anonymous namespace

void Preset::getHeader(PresetHeader &header) const
{
    auto const &headerNode(root());
    copyAndNullTerminate(headerNode, header.attributeNames.version, header.version,
                         sizeof(header.version));
    copyAndNullTerminate(headerNode, header.attributeNames.timeStamp, header.timeStamp,
                         sizeof(header.timeStamp));
    copyAndNullTerminate(headerNode, header.attributeNames.comment, header.comment,
                         sizeof(header.comment));
}

/// \note "Expects the header parameter to live until after the last call to
/// saveTo()" stood here, and does not any more: RapidXML stored the pointer it
/// was handed, TinyXML copies the string.
void Preset::setHeader(PresetHeader const &header)
{
    auto &headerNode(root());
    headerNode.SetAttribute(header.attributeNames.version, header.version);
    headerNode.SetAttribute(header.attributeNames.timeStamp, header.timeStamp);
    headerNode.SetAttribute(header.attributeNames.comment, header.comment);
}

std::string_view Preset::getComment() const
{
    auto const *const pComment(root().Attribute(PresetHeader::AttributeNames::comment));
    LE_ASSERT(pComment);
    return pComment ? std::string_view(pComment) : std::string_view();
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

/// \note The RapidXML allocation tracer that stood here is gone with the arena
/// it traced. It printed a line per node on every debug preset load, which the
/// corpus test turned into several hundred.
///                                           (31.07.2026.) (SW port)

std::string PresetHandler::mangleName(std::string_view const parameterName)
{
    LE_ASSERT(!parameterName.empty());
    return mangleXmlName(parameterName);
}

template <> std::string PresetHandler::makeString<bool>(bool const binarySource)
{
    return binarySource ? "1" : "0";
}

/// \note A preset with no `<Global>` node used to throw from this constructor,
/// which `loadPreset`'s catch-all turned into "unable to load". It reports the
/// same thing without the throw: `pParameters_` is null and every getter misses,
/// which the missing-parameter path already handles.
ParametersLoader::ParametersLoader(Preset const &preset)
    : PresetHandler(const_cast<Preset &>(preset)), syncedLFOFound_(false)
{
    pParameters_ = preset.root().FirstChildElement(globalParametersNodeName_);
    LE_ASSERT_MSG(pParameters_, "Preset node not found");
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
        auto const *const pModuleParameters(
            preset().root().FirstChildElement(moduleParametersNodeName_));
#if 0
        if ( !pModuleParameters )
            RAPIDXML_PARSE_ERROR( "Module parameters node not found", nullptr );
#else
        LE_ASSERT_MSG(pModuleParameters, "Module parameters node not found");
        if (!pModuleParameters)
            return ModuleChain();
#endif
        pParameters_ = pModuleParameters->FirstChildElement();
    }

    ModuleChain newChain;
    std::int8_t const noModule(-1);
    std::uint8_t moduleIndex(0);
    while (pParameters_)
    {
        using namespace Effects;
        auto const effectName(currentMangledEffectName());
        auto const effectIndex(effectIndexFromMangledName(effectName));
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
        pParameters_ = pParameters_->NextSiblingElement();
    }

#ifndef LE_SW_SDK_BUILD
    LE_ASSERT_MSG(moduleIndex <= SW::Constants::maxNumberOfModules,
                  "Preset loaded too many modules?");
#endif // LE_SW_SDK_BUILD
    return newChain;
}

bool ParametersLoader::switchedToModuleParameters() const
{
    return pParameters_ &&
           (std::string_view(parameters().Value()) /*...mrmlj...== moduleParametersNodeName_*/
            != globalParametersNodeName_);
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

/// \brief Which effect an element name belongs to.
///
/// \note Mangled-to-mangled, rather than unmangling the element name and looking
/// it up. The mangling is not invertible -- "Pitch Shifter (pvd)" and
/// "Pitch_Shifter__pvd_" both come from characters that all map to `_` -- so the
/// comparison has to happen on the side that is a function of the other. 57
/// string compares per module in a cold path.
///
/// \note This is also what makes a repaired legacy preset work: the repair
/// applies the same mangling the writer does, so `<Pitch_Shifter_(pvd)>` from
/// 2013 and `<Pitch_Shifter__pvd_>` written today both arrive here as the
/// latter.
std::int8_t ParametersLoader::effectIndexFromMangledName(std::string_view const mangledName)
{
    for (std::uint8_t effect(0); effect < Effects::Constants::numberOfEffects; ++effect)
        if (mangleName(Effects::effectName(effect)) == mangledName)
            return static_cast<std::int8_t>(effect);
    return -1;
}

/// \note The 2016 version wrote a NUL over the byte after the element's name --
/// into the parse buffer, because RapidXML's names are not terminated and the
/// mangling wanted a C string. TinyXML's are.
char const *ParametersLoader::currentMangledEffectName() const
{
    LE_ASSERT_MSG(switchedToModuleParameters(), "Not yet switched to module parameters.");
    return parameters().Value();
}

// sampleAttributeName_ contains no spaces
std::string_view ParametersLoader::getSampleFileName()
{
    LE_ASSERT_MSG(!switchedToModuleParameters(),
                  "Sample file name must be fetched before switching to module parameters.");
    auto const *const pSampleFileName(getParameterAttribute(sampleAttributeName_));
    return pSampleFileName ? std::string_view(pSampleFileName) : std::string_view();
}

bool ParametersLoader::isPre27Preset() const
{
    auto const *const pVersion(preset().root().Attribute("Version"));
    if (!pVersion)
        return true;
    return Utility::lexical_cast<float>(pVersion) < 2.7f;
}

namespace
{
class LFODataLoader
{
  public:
    LFODataLoader(TiXmlElement const &parameterNode, Parameters::LFOImpl const &lfo)
        : parameterNode_(parameterNode), lfo_(lfo)
    {
    }

    template <class LFOParameter> void operator()(LFOParameter &element) const
    {
        using namespace Parameters;
        doLoad(std::string(name<LFOParameter>()).c_str(), element);
    }

  private:
    template <class LFOParameter>
    void doLoad(char const *const elementName, LFOParameter &element) const
    {
        auto const *const pElementValue(parameterNode_.Attribute(elementName));
        if (pElementValue)
        {
            element = lfo_.adjustValueFromPreset<LFOParameter>(
                static_cast<typename LFOParameter::value_type>(
                    Utility::lexical_cast<typename LFOParameter::binary_type>(pElementValue)));
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
    TiXmlElement const &parameterNode_;
    Parameters::LFOImpl const &lfo_;
}; // class LFODataLoader
} // anonymous namespace

bool ParametersLoader::loadLFO(TiXmlElement const &parameterNode, LFO &lfo) const
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

LE_NOINLINE char const *
ParametersLoader::getParameterAttribute(char const *const parameterName) const
{
    if (!pParameters_)
        return nullptr;
    return parameters().Attribute(mangleName(parameterName).c_str());
}

LE_NOINLINE TiXmlElement const *
ParametersLoader::getParameterNode(char const *const parameterName) const
{
    if (!pParameters_)
        return nullptr;
    return parameters().FirstChildElement(mangleName(parameterName).c_str());
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
SavedPreset::SavedPreset()
{
    auto *const pHeaderNode(new TiXmlElement(headerNodeName_));
    xml().LinkEndChild(pHeaderNode);

    pGlobalParametersNode_ = new TiXmlElement(globalParametersNodeName_);
    pModuleParametersNode_ = new TiXmlElement(moduleParametersNodeName_);
    pHeaderNode->LinkEndChild(pGlobalParametersNode_);
    pHeaderNode->LinkEndChild(pModuleParametersNode_);
}

void SavedPreset::setHeader(PresetHeader const &header) { Preset::setHeader(header); }

ParametersSaver::ParametersSaver(SavedPreset &preset)
    : PresetHandler(preset), pParametersNode_(&preset.globalParametersNode())
{
}

unsigned int ParametersSaver::saveTo(std::span<char> const buffer) const
{
    LE_ASSERT_MSG(moduleChainSaved_, "Module chain parameters not yet saved/parsed.");
    return preset().saveTo(buffer);
}

void ParametersSaver::saveEffectModuleChain(AutomatedModuleChain const &moduleChain)
{
    LE_ASSERT_MSG(!moduleChainSaved_, "Already switched to modules."); //...mrmlj...
    moduleChainSaved_ = true;

    moduleChain.forEach<PresetModule>([&](PresetModule const &module) {
        auto *const pModuleNode(
            new TiXmlElement(mangleName(Effects::effectName(module.effectTypeIndex())).c_str()));
        preset().moduleParametersNode().LinkEndChild(pModuleNode);
        pParametersNode_ = pModuleNode;
        module.savePresetParameters(*this);
    });
}

void ParametersSaver::saveParameter(char const *const parameterName,
                                    std::string const &parameterValue)
{
    parameters().SetAttribute(mangleName(parameterName), parameterValue);
}

// ...mrmlj...cannot put LFODataSaver into the anonymous namespace because it is
// declared as friend in the PresetHandler class...clean this up...
//namespace
//{
class LFODataSaver
{
  public:
    using LFO = Parameters::LFOImpl;

    LFODataSaver(PresetHandler &handler, TiXmlElement &parameterNode, LFO const &lfo)
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

        using namespace Parameters;
        parameterNode_.SetAttribute(std::string(name<LFOParameter>()),
                                    PresetHandler::makeString(lfo_.adjustValueForPreset(element)));
    }

#pragma warning(pop)

  private:
    PresetHandler &handler_;
    TiXmlElement &parameterNode_;
    LFO const &lfo_;
};
//} // anonymous namespace

/// \note An LFO-able parameter is an element with the value as its text and the
/// LFO's settings as its attributes, rather than a plain attribute -- which is
/// why the loader has to look for both.
void ParametersSaver::saveParameter(char const *const parameterName,
                                    std::string const &parameterValue, LFO const &parameterLFO)
{
    auto *const pParameterNode(new TiXmlElement(mangleName(parameterName).c_str()));
    pParameterNode->LinkEndChild(new TiXmlText(parameterValue));

    LE::Parameters::forEach(parameterLFO.parameters(),
                            LFODataSaver(*this, *pParameterNode, parameterLFO));

    parameters().LinkEndChild(pParameterNode);
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
    saveParameter(sampleAttributeName_, std::string(sampleFileName));
}

unsigned int savePreset(std::span<char> const data, juce::File const &externalSampleFile,
                        juce::String const &comment, Program const &program)
{
    PresetHeader const presetHeader(comment);
    SavedPreset preset;
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
