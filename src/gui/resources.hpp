////////////////////////////////////////////////////////////////////////////////
///
/// \file resources.hpp
/// -------------------
///
///   The editor's bitmaps and fonts, read out of the binary.
///
///   In 2016 these lived on disk. The plugin found them by mmapping a
/// `SpectrumWorx.paths` file that the installer wrote next to the binary
/// (gui.cpp's initializePaths / mapPathsFile / rootPath / resourcesPath), so a
/// plugin that had merely been copied rather than installed came up with no
/// skin at all. They are compiled in now, and that whole path machinery goes
/// with them.
///
/// Copyright (c) 2010 - 2016. Little Endian Ltd.
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef resources_hpp__D4A81C36_7E92_4B05_9F17_2C8E6A4D530B
#define resources_hpp__D4A81C36_7E92_4B05_9F17_2C8E6A4D530B
//------------------------------------------------------------------------------
#include <juce_graphics/juce_graphics.h>
//------------------------------------------------------------------------------
namespace LE
{
//------------------------------------------------------------------------------
namespace SW
{
//------------------------------------------------------------------------------
namespace GUI
{
//------------------------------------------------------------------------------

/// \brief Widget name to skin file number, the single source of truth.
///
/// \note The 2016 values were multi-character literals -- `EditorBackground =
/// '01'` -- which resourceBitmap<>() then took apart with boost::mpl::string to
/// recover the two digits of the file name. Multi-character literals have an
/// implementation-defined value, and the only thing ever wanted from them was
/// the number, so these are now the number. The names, and therefore every call
/// site, are unchanged.
///
/// \note A list rather than a bare enum so that a test can walk it and assert
/// every named bitmap is actually in the binary -- the failure it replaces (a
/// mistyped number) is otherwise a blank widget nobody notices until someone
/// looks at a screenshot. Same idiom as LE_SW_EFFECT_LIST.
///                                       (28.07.2026.) (SW port)
///
/// \brief x( Name, fileNumber )
// clang-format off
#define LE_SW_RESOURCE_BITMAP_LIST(x)                       \
    /* Editor */                                            \
    x(EditorBackground,        1)                           \
    x(EditorKnobStrip,         2)                           \
    x(AddModule,               6)                           \
    x(PresetOff,               8)                           \
    x(PresetOn,                9)                           \
    x(SettingsOff,            10)                           \
    x(SettingsOn,             11)                           \
                                                            \
    /* Module panel */                                      \
    x(ModuleKnobStrip,         3)                           \
    x(ModuleOn,                4)                           \
    x(ModuleMuted,             5)                           \
    x(SymmetricKnobStrip,     12)                           \
    x(TriggerBtnOff,          13)                           \
    x(TriggerBtnOn,           14)                           \
    x(Eject,                  16)                           \
    x(ModuleBg,               55)                           \
    x(ModuleBgSelected,       56)                           \
    x(ModuleKnobSelected,     58)                           \
    x(ModuleCombo,            59)                           \
    x(ModuleComboOn,          60)                           \
    x(SmallLinearKnobStrip,   63)                           \
    x(SmallSymmetricKnobStrip,64)                           \
    x(SmallModuleKnobSelected,65)                           \
    x(ModuleKnobLFOed,        68)                           \
                                                            \
    /* Settings panel */                                    \
    x(SettingsEngineBg,       17)                           \
    x(SettingsIntrfcBg,       17) /* ...same as engine... */\
    x(SettingsAboutBg,        20)                           \
    x(SettingsEngineOff,      21)                           \
    x(SettingsEngineOn,       22)                           \
    x(SettingsGUIOff,         23)                           \
    x(SettingsGUIOn,          24)                           \
    x(SettingsAboutOff,       27)                           \
    x(SettingsAboutOn,        28)                           \
    x(SettingsCombo,          61)                           \
    x(SettingsComboOn,        62)                           \
    x(UsersGuideUp,           66)                           \
    x(UsersGuideDown,         67)                           \
                                                            \
    /* Preset browser */                                    \
    x(PresetBackground,        7)                           \
    x(PresetSaveUp,           30)                           \
    x(PresetSaveDown,         31)                           \
    x(PresetDeleteUp,         32)                           \
    x(PresetDeleteDown,       33)                           \
    x(PresetSaveAsUp,         34)                           \
    x(PresetSaveAsDown,       35)                           \
                                                            \
    /* LFO */                                               \
    x(LFOSliderThumb,         40)                           \
    x(LEDOff,                 41)                           \
    x(LEDOn,                  42)                           \
    x(LFOSine,                43)                           \
    x(LFOTriangle,            44)                           \
    x(LFOSawtooth,            45)                           \
    x(LFOReverseSaw,          46)                           \
    x(LFOSquare,              47)                           \
    x(LFOExponent,            48)                           \
    x(LFORandomHold,          49)                           \
    x(LFORandomSlide,         50)                           \
    x(LFORandomWhacko,        51)                           \
    x(LFODirac,               52)                           \
    x(LFOdIRAC,               53)                           \
    x(ChangeWaveform,         57)
// clang-format on

enum ResourceBitmaps
{
#define LE_SW_AUX_RESOURCE_BITMAP(name, number) name = number,
    LE_SW_RESOURCE_BITMAP_LIST(LE_SW_AUX_RESOURCE_BITMAP)
#undef LE_SW_AUX_RESOURCE_BITMAP
}; // enum ResourceBitmaps

/// The highest number `assets/skin` holds, and the size of the cache.
unsigned int constexpr numberOfResourceBitmaps = 68;

/// \brief Decodes on first use and caches; the reference stays valid until
/// releaseCachedResources().
///
/// \note The 2016 cache was a function-local static inside a template, so there
/// was one juce::Image per instantiation and no way to release any of them --
/// they outlived the JUCE they were allocated under. This is one array, and it
/// can be emptied.
///
/// \note The numbering has holes: 15, 18 and 54 do not exist, and 19, 25, 26,
/// 29 and 36 to 39 exist but no widget names them. A hole yields an invalid
/// image rather than an assertion, so that iterating the range is legal; the
/// check that every *named* bitmap resolves is a test (skinTests.cpp), which is
/// a better place for it than every call.
juce::Image const &resourceBitmap(unsigned int number);

bool hasResourceBitmap(unsigned int number);

template <unsigned int bitmapID> juce::Image const &resourceBitmap()
{
    return resourceBitmap(bitmapID);
}

/// Bitstream Vera, the skin's font, loaded straight from the embedded bytes.
///
/// \note 2016 registered the two .ttf files *with the operating system*
/// (AddFontResourceEx / CTFontManagerRegisterFontsForURL) and then referred to
/// them by family name, which needed a file on disk, leaked the registration if
/// the plugin was unloaded abruptly, and let a system font of the same name win.
/// JUCE 8 can make a Typeface directly out of memory.
///                                       (28.07.2026.) (SW port)
juce::Typeface::Ptr regularTypeface();
juce::Typeface::Ptr boldTypeface();

/// Drops every decoded image and both typefaces. Called when the last editor
/// goes away, so that nothing outlives the JUCE instance it was created under.
void releaseCachedResources();

//------------------------------------------------------------------------------
} // namespace GUI
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------
#endif // resources_hpp
