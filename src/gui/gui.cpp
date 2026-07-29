//------------------------------------------------------------------------------
//...mrmlj...cleanup...
//------------------------------------------------------------------------------
#include "gui.hpp"

#include "core/host_interop/plugin2Host.hpp" //...mrmlj...only for Plugin2HostPassiveInteropController::ParameterLabelGetter...
#include "gui/editor/spectrumWorxEditor.hpp"

#include "le/spectrumworx/engine/setup.hpp"
#include "le/utility/countof.hpp"
#include "le/utility/cstdint.hpp"
#include "le/utility/lexicalCast.hpp"
#include "le/utility/platformSpecifics.hpp"

#include "le/utility/assert.hpp"
#include "le/utility/polymorphicDowncast.hpp"
#ifdef _WIN32
#include <algorithm>
#endif // _WIN32

#ifdef __APPLE__
#include "dlfcn.h"
#include "ApplicationServices/ApplicationServices.h" // only for CoreGraphics...
#include "le/utility/ignoreUnused.hpp"
#endif // __APPLE__

#include <array>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string_view>
#include "le/utility/span.hpp"
//------------------------------------------------------------------------------
#ifdef __APPLE__
void const *swDLLAddress;
#endif // __APPLE__
#ifdef _WIN32
extern "C" IMAGE_DOS_HEADER __ImageBase;
#endif // _WIN32
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

#ifdef __APPLE__
// gui.mmm forward declarations.
void initialiseMac() noexcept;

void makeEditorChild(juce::ComponentPeer &editor, juce::ComponentPeer &childToBe) noexcept;
void detachFromEditor(juce::ComponentPeer &editor, juce::ComponentPeer &child) noexcept;

void hideCursor() noexcept;
void showCursor() noexcept;
#endif

////////////////////////////////////////////////////////////////////////////////
//
// ReferenceCountedGUIInitializationGuard static member definitions.
// -----------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////////

std::uint8_t ReferenceCountedGUIInitializationGuard::guiInitializationReferenceCount(0);

namespace
{
void onGUIInitialization();
void onGUIShutdown();
} // anonymous namespace

ReferenceCountedGUIInitializationGuard::ReferenceCountedGUIInitializationGuard()
{
    if (guiInitializationReferenceCount && !isThisTheGUIThread())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                               "SpectrumWorx error:",
                                               "We are sorry but SpectrumWorx does not currently "
                                               "support multiple editor instances with this host.");
        throw std::exception();
    }

    if (guiInitializationReferenceCount++ == 0)
    {
#if defined(__APPLE__)
        initialiseMac();
#elif defined(_WIN32)
        juce::Process::setCurrentModuleInstanceHandle(&__ImageBase);
#endif // __APPLE__
        //...mrmlj...
        JUCE_AUTORELEASEPOOL
        {
            juce::initialiseJuce_GUI();
            juce::MessageManager::getInstance()->setCurrentThreadAsMessageThread();
            // juce::Desktop::create() is gone; initialiseJuce_GUI owns the
            // Desktop's lifetime in JUCE 8 and its constructor is private.
            onGUIInitialization();
            juce::LookAndFeel::setDefaultLookAndFeel(&Theme::singleton());
        }
    }
}

ReferenceCountedGUIInitializationGuard::~ReferenceCountedGUIInitializationGuard()
{
    LE_ASSERT(isThisTheGUIThread());

#if defined(__APPLE__) && !__LP64__
    /// \note See the end note in detachComponentFromHostWindow() in gui.mm.
    ///                                       (23.12.2011.) (Domagoj Saric)
    /// \note Creation of another SW GUI might already be in the message queue
    /// (e.g. when switching between multiple instances of SW in Reaper) so we
    /// have to 'empty' the message queue before the reference count check to
    /// avoid destroying JUCE in after another SW GUI has been constructed (i.e.
    /// avoid the following scenario: reference-check...pump-message-queue-where
    /// -a-new-GUI-is-constructed...destroy-JUCE).
    ///                                       (20.02.2013.) (Domagoj Saric)
    /// \note Original JUCE note from detachComponentFromHostWindow():
    /// The event loop needs to be run between closing the window and deleting
    /// the plugin, presumably to let the cocoa objects get tidied up. Leaving
    /// out this line causes crashes in Live and Reaper when you delete the
    /// plugin with its window open. (Doing it this way rather than using a
    /// single longer timout means that we can guarantee how many messages will
    /// be dispatched, which seems to be vital in Reaper).
    ///                                           (13.09.2013.) (Domagoj Saric)
    for (unsigned int i(20); i != 0; --i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(1);
#endif // Apple

    if (--guiInitializationReferenceCount == 0)
    {
        JUCE_AUTORELEASEPOOL
        {
            onGUIShutdown();
            // Implementation note:
            //   We must manually reset the animator otherwise its timer becomes
            // orphaned when the juce::InternalTimerThread singleton is
            // destroyed (so it thinks it is still running even though its
            // parent juce::InternalTimerThread has been destroyed).
            //                                (15.12.2011.) (Domagoj Saric)
            // \note The stopTimer() that followed is unreachable in JUCE 8 --
            // ComponentAnimator inherits Timer privately -- and the
            // InternalTimerThread it was defending against no longer exists.
            //                                (28.07.2026.) (SW port)
            juce::Desktop::getInstance().getAnimator().cancelAllAnimations(false);
#if defined(_WIN32)
            LE_ASSERT(juce::Process::getCurrentModuleInstanceHandle() == &__ImageBase);
#endif // _WIN32
            // Desktop::destroy() and MessageManager::destroySingleton() are
            // both gone; shutdownJuce_GUI() does both.
            juce::shutdownJuce_GUI();

            LE_ASSERT(guiInitializationReferenceCount == 0);
        }
    }
}

bool ReferenceCountedGUIInitializationGuard::isGUIInitialised()
{
    return guiInitializationReferenceCount != 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// warningMessageBox()
// -------------------
//
//    A thread safe nothrow implementation that can be safely called whenever
// and wherever from.
//
////////////////////////////////////////////////////////////////////////////////

/// \note Both of these were synchronous. JUCE 8 defaults JUCE_MODAL_LOOPS_PERMITTED
/// to 0, and a plugin has no business spinning a modal loop inside a host's
/// message thread anyway, so neither blocks now.
///
///   showNativeDialogBox is gone from JUCE 8 outright, and the
/// isGUIInitialised() / isThisTheGUIThread() dance that chose between it and the
/// JUCE box went with it: showMessageBoxAsync is safe to call from anywhere and
/// simply posts.
///                                       (28.07.2026.) (SW port)

void warningMessageBox(std::string_view const title, std::string_view const message,
                       bool const /*canBlock*/)
{
    //...mrmlj...canBlock no longer means anything and should come off the ~15
    //...mrmlj...call sites once they are ported.
    JUCE_AUTORELEASEPOOL
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                               juce::String(title.begin(), title.size()),
                                               juce::String(message.begin(), message.size()));
    }
}

void warningOkCancelBox(TCHAR const *const title, TCHAR const *const question,
                        std::function<void(bool)> onResult)
{
    JUCE_AUTORELEASEPOOL
    {
        juce::AlertWindow::showOkCancelBox(
            juce::MessageBoxIconType::WarningIcon, juce::String(title), juce::String(question), {},
            {}, nullptr,
            juce::ModalCallbackFunction::create([onResult = std::move(onResult)](int const result) {
                if (onResult)
                    onResult(result == 1);
            }));
    }
}

static juce::File pluginRootPath;
static juce::File mruPresetsFolder;

namespace
{
unsigned int const maxPathLength =
#if defined(_WIN32)
    // https://visualstudio.uservoice.com/forums/121579-visual-studio/suggestions/2156195-fix-260-character-file-name-length-limitation
    // http://blogs.msdn.com/b/bclteam/archive/2007/02/13/long-paths-in-net-part-1-of-3-kim-hamilton.aspx
    MAX_PATH;
#elif defined(__APPLE__)
    PATH_MAX + 1;
#endif

typedef TCHAR path_t[maxPathLength];

unsigned int getBinaryPath(path_t &path)
{
#if defined(_WIN32)

    DWORD const fullPathLength(
        ::GetModuleFileName(reinterpret_cast<HMODULE>(&__ImageBase), path, _countof(path)));
    LE_ASSERT((fullPathLength > 0) && (fullPathLength < _countof(path)));
    LE_ASSERT(path[fullPathLength] == '\0');
    static TCHAR const extension[] = _T( ".dll" );

#elif defined(__APPLE__)

    // http://developer.apple.com/library/mac/#documentation/DeveloperTools/Reference/MachOReference/Reference/reference.html
    // http://lists.apple.com/archives/xcode-users/2004/Feb/msg00428.html

    //unsigned long fullPathLength( PATH_MAX );
    //NSGetExecutablePath( path, &fullPathLength );
    Dl_info exeInfo;
    LE_VERIFY(dladdr(&pluginRootPath, &exeInfo) != 0);
    ::swDLLAddress = exeInfo.dli_fbase;
    unsigned long fullPathLength(std::strlen(exeInfo.dli_fname));
    LE_ASSERT(fullPathLength <= _countof(path) + _countof(".paths"));
    std::memcpy(path, exeInfo.dli_fname, fullPathLength);
    //static TCHAR const extension[] = _T( ".dylib" );...mrmlj...does not see through symlink...
    static TCHAR const extension[] = _T( "." );
    path[fullPathLength++] = _T('.');
    path[fullPathLength] = _T('\0');

#endif // OS

    unsigned int const dotIndex(fullPathLength - sizeof('\0') -
                                (_countof(extension) - sizeof('\0') - sizeof('.')));
    unsigned int const insertionIndex(dotIndex + sizeof('\0'));

    LE_ASSERT(path[dotIndex] == _T('.'));
    LE_ASSERT(std::_tcscmp(&path[dotIndex], extension) == 0);
    return insertionIndex;
}
} // anonymous namespace

/// \note What used to live here: the plugin found its skin, its presets and its
/// documentation by mmapping a `SpectrumWorx.paths` file that the 2016 installer
/// wrote next to the binary. The skin is compiled into the binary now
/// (resources.hpp), the installer is gone, and boost::mmap went with stage 2, so
/// the file, the two mapPathsFile() overloads that read and rewrote it, and the
/// on-disk resourceBitmap() that used it are all deleted.
///
///   rootPath() and presetsFolder() survive because the editor still wants a
/// place for the user's guide and the preset browser still wants a folder to
/// open in. Both now answer from ordinary locations rather than from an
/// installer artefact. **Stage 8 owns where presets actually live** -- this is a
/// placeholder that keeps the callers honest, not a decision.
///                                       (28.07.2026.) (SW port)

bool initializePaths()
{
    if (!havePathsBeenInitialised())
    {
        pluginRootPath = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                             .getChildFile("SpectrumWorx");
        mruPresetsFolder = pluginRootPath.getChildFile("Presets");
    }
    return true;
}

bool havePathsBeenInitialised() { return pluginRootPath != juce::File(); }

juce::File const &rootPath()
{
    LE_ASSERT_MSG((pluginRootPath != juce::File()), "Not initialized.");
    return pluginRootPath;
}

juce::File &presetsFolder()
{
    LE_ASSERT_MSG((mruPresetsFolder != juce::File()), "Not initialized.");
    return mruPresetsFolder;
}

void paintImage(juce::Graphics &graphics, juce::Image const &image)
{
    paintImage(graphics, image, 0, 0);
}

void paintImage(juce::Graphics &graphics, juce::Image const &image, int const x, int const y)
{
    graphics.drawImage(image, x, y, image.getWidth(), image.getHeight(), 0, 0, image.getWidth(),
                       image.getHeight());
}

void setSizeFromImage(juce::Component &component, juce::Image const &image)
{
    component.setSize(image.getWidth(), image.getHeight());
}

void addToParentAndShow(juce::Component &parent, juce::Component &childToBe)
{
    parent.addAndMakeVisible(&childToBe);
}

void fadeOutComponent(juce::Component &component, float const finalAlpha,
                      unsigned int const duration, bool const useProxyComponent)
{
    try
    {
        juce::Point<int> const centre(component.getBounds().getCentre());
        juce::Desktop::getInstance().getAnimator().animateComponent(
            &component, juce::Rectangle<int>(centre.getX(), centre.getY(), 0, 0), finalAlpha,
            duration, useProxyComponent, 0, 0);
    }
    catch (...)
    {
    }
}

LE_NOINLINE bool LE_COLD isThisTheGUIThread()
{
#ifndef NDEBUG
    if (!GUI::
            isGUIInitialised()) //...mrmlj...to avoid an LE_ASSUME/assertion failure in juce::MessageManager::getInstance()...
        return false;
#endif // NDEBUG
#ifdef __APPLE__
    if (!juce::MessageManager::getInstanceWithoutCreating())
        return false; //...mrmlj...quick-hack to fix crashes on OSX when this function is called before the GUI is initialised...
#endif                // __APPLE__
    return juce::MessageManager::getInstance()->isThisTheMessageThread();
}

bool LE_COLD isGUIInitialised()
{
    return ReferenceCountedGUIInitializationGuard::isGUIInitialised();
}

float LE_COLD displayScale()
{
    auto const &desktop(juce::Desktop::getInstance());
    auto const scale(desktop.getGlobalScaleFactor());
#ifndef NDEBUG
    for (auto const &display : desktop.getDisplays().displays)
        LE_ASSERT(display.scale == scale);
#endif // NDEBUG
    return scale;
}

namespace Detail
{
LE_COLD void setName(juce::Component &widget, juce::String const &newName)
{
    widget.juce::Component::setName(newName);
}
LE_NOINLINE LE_COLD void setName(juce::Component &widget, char const *const newName)
{
    setName(widget, juce::String(newName));
}

bool hasDirectFocus(juce::Component const &widget)
{
    bool const result(&widget == widget.getCurrentlyFocusedComponent());
    LE_ASSERT(result == widget.hasKeyboardFocus(false));
    return result;
}

bool hasFocus(juce::Component const &widget)
{
    bool const result(hasDirectFocus(widget) ||
                      isParentOf(widget, widget.getCurrentlyFocusedComponent()));
    LE_ASSERT(result == widget.hasKeyboardFocus(true));
    return result;
}

bool isParentOf(juce::Component const &parent, juce::Component const &possibleChild)
{
    juce::Component *pParent(possibleChild.getParentComponent());
    while (pParent)
    {
        if (pParent == &parent)
            return true;
        pParent = pParent->getParentComponent();
    }
    return false;
}

bool isParentOf(juce::Component const &parent, juce::Component const *pPossibleChild)
{
    return pPossibleChild && isParentOf(parent, *pPossibleChild);
}
} // namespace Detail

#ifdef _WIN32
HHOOK OwnedWindowBase::wndProcHook(0);
#endif // _WIN32

void OwnedWindowBase::attach(SpectrumWorxEditor &parent, juce::Component &window)
{
    LE_ASSERT(!window.isOpaque());

    //...mrmlj...check these links for mac keystroke handling:
    //http://lists.steinberg.net:8100/Lists/vst-plugins/Message/12066.html
    juce::ComponentPeer &owner(*parent.getPeer());
#ifdef _WIN32
    // Implementation note:
    //   FL Studio 9 uses a GUI engine that has a 'nonstandard' way of handling/
    // implementing owned-windows, that is, all windows in the FL Studio are
    // real child windows (have the WS_CHILD style set) of the main window.
    // Because of this we cannot simply call GetAncestor() with GA_ROOT to get
    // our 'real' parent (the wrapping window) because this would always return
    // the handle to the main application window (in FL Studio 9). As a
    // workaround we traverse up the window hierarchy searching for windows
    // that have "SpectrumWorx" in their names assuming these would be our
    // wrapping windows. Because of hosts like Audition 3.0 [that have several
    // wrapper windows that have the plugin name in their title and that, in
    // addition, are not 'sequential'/direct ancestors/descendants (e.g.
    // Audition wraps SpectrumWorx in a window called " - SpectrumWorx" that is
    // in turn a child of a window called "EffectView" that is in turn a child
    // of the final wrapper window called "VST Plugin - SpectrumWorx")] we must
    // traverse the entire hierarchy and take the last/highest 'appropriate'
    // window.
    //                                        (25.05.2010.) (Domagoj Saric)
    /// \todo The above approach works for so far tested hosts but will fail to
    /// work for hosts that put the name of the (active) plugin in the title
    /// of the main window. Consider a smarter solution when it becomes
    /// necessary.
    ///                                       (25.05.2010.) (Domagoj Saric)
    /// http://www.codeproject.com/Tips/222075/All-about-owned-windows
    /// http://blogs.msdn.com/b/oldnewthing/archive/2010/03/15/9978691.aspx
    HWND const editorHandle(reinterpret_cast<HWND>(owner.getNativeHandle()));
    HWND editorRootParentNativeHandle(editorHandle);
    HWND lastPossibleHandle(0);
    std::string_view const spectrumWorxTitle("SpectrumWorx");
    char windowTitleBuffer[256];
    do
    {
        editorRootParentNativeHandle = ::GetAncestor(editorRootParentNativeHandle, GA_PARENT);
        std::string_view const windowTitle(
            windowTitleBuffer, ::GetWindowTextA(editorRootParentNativeHandle, windowTitleBuffer,
                                                _countof(windowTitleBuffer)));
        if (windowTitle.find(spectrumWorxTitle) != std::string_view::npos)
            lastPossibleHandle = editorRootParentNativeHandle;
    } while (editorRootParentNativeHandle);
    editorRootParentNativeHandle = lastPossibleHandle;
    if (!editorRootParentNativeHandle)
    {
        editorRootParentNativeHandle = ::GetAncestor(editorHandle, GA_ROOT);
        // Implementation note:
        //   Check if we seem to be dealing with a host like Audio Mulch that
        // uses Qt or something similar and uses only child windows (so
        // GetParent() always returns the main application window) that
        // additionally have no window text set (so we cannot use text search).
        //   If so we have no other option but to search for the first parent
        // whose client area encompasses our window.
        //                                    (02.06.2010.) (Domagoj Saric)
        // Implementation note:
        //   To detect if we reached the main application window we must use the
        // GetParent() function (that also 'returns' owner windows) and not the
        // GetAncestor() function with the GA_PARENT parameter because the main
        // application window can 'just' own our parent/wrapper window. In this
        // case GetAncestor( *, GA_PARENT ) would return the desktop window and
        // we would mistake it for the main application window.
        //                                    (04.06.2010.) (Domagoj Saric)
        HWND const masterParent(::GetParent(editorRootParentNativeHandle));
        if (!masterParent || (masterParent == ::GetDesktopWindow()))
        {
            RECT editorRect;
            LE_VERIFY(::GetWindowRect(editorHandle, &editorRect));
            editorRootParentNativeHandle = editorHandle;
            do
            {
                editorRootParentNativeHandle =
                    ::GetAncestor(editorRootParentNativeHandle, GA_PARENT);
                RECT editorParentRect;
                LE_VERIFY(::GetWindowRect(editorRootParentNativeHandle, &editorParentRect));
                if ((editorParentRect.left < editorRect.left) &&
                    (editorParentRect.top < editorRect.top))
                    break;
            } while (editorRootParentNativeHandle);
        }
    }

    if (wndProcHook == 0)
        wndProcHook =
            ::SetWindowsHookEx(WH_CALLWNDPROC, &callWndHookProc, 0, ::GetCurrentThreadId());
    LE_ASSERT(wndProcHook); //...mrmlj...better error handling desired...

    window.juce::Component::addToDesktop(juce::ComponentPeer::windowIsSemiTransparent,
                                         owner.getNativeHandle());
#endif // _WIN32

#ifdef __APPLE__
    // http://www.cocoadev.com/index.pl?MagneticWindows
    // http://www.cocoadev.com/index.pl?WindowFollowingWindow
    // http://web.mac.com/mabi99/marcocoa/blog/Entries/2007/5/30_Watching_a_window%E2%80%99s_frame.html
    window.juce::Component::addToDesktop(juce::ComponentPeer::windowIsSemiTransparent);
    juce::ComponentPeer &ownedWindowPeer(*window.getPeer());
    makeEditorChild(owner, ownedWindowPeer);
#endif // __APPLE__

    window.juce::Component::setVisible(true);
}

void OwnedWindowBase::detach(SpectrumWorxEditor &editor, juce::Component &ownee)
{
#ifdef _WIN32
    LE_ASSERT(wndProcHook != 0);
    if (juce::ComponentPeer::getNumPeers() < 3)
    {
        LE_ASSERT(juce::ComponentPeer::getNumPeers() == 2);
        LE_ASSERT(juce::ComponentPeer::getPeer(0) == editor.getPeer());
        LE_ASSERT(juce::ComponentPeer::getPeer(1) == ownee.getPeer());
        LE_VERIFY(::UnhookWindowsHookEx(wndProcHook));
        wndProcHook = 0;
    }
    LE::Utility::ignoreUnused(editor);
    LE::Utility::ignoreUnused(ownee);
#else
    detachFromEditor(*editor.getPeer(), *ownee.getPeer());
#endif // _WIN32
}

#ifdef _WIN32
juce::ComponentPeer *peerWithParentHandle(HWND const parentWindowHandle)
{
    for (unsigned int i(0); i < static_cast<unsigned int>(juce::ComponentPeer::getNumPeers()); ++i)
    {
        juce::ComponentPeer *const pPeer(juce::ComponentPeer::getPeer(i));
        LE_ASSERT(pPeer);
        if (::IsChild(parentWindowHandle, static_cast<HWND>(pPeer->getNativeHandle())))
            return pPeer;
    }
    return nullptr;
}

LRESULT CALLBACK OwnedWindowBase::callWndHookProc(int const nCode, WPARAM const wParam,
                                                  LPARAM const lParam)
{
    LE_ASSERT(lParam);
    CWPSTRUCT const &info(*reinterpret_cast<CWPSTRUCT const *>(lParam));
    if (info.message == WM_WINDOWPOSCHANGED)
    {
        LE_ASSERT(info.lParam);
        WINDOWPOS const &wp(*reinterpret_cast<WINDOWPOS const *>(info.lParam));
        LE_ASSERT(wp.hwnd == info.hwnd);
        // Implementation note:
        //   We have to update on all parent window adjustments (not just on
        // movement) because some hosts do some non-movement adjustments to the
        // main editor's parent window (that nonetheless affect the relative
        // position of our editor window within the parent) right after creation
        // and that breaks the positioning of the owned popup windows.
        //                                    (20.04.2010.) (Domagoj Saric)
        //if ( !( wp.flags & SWP_NOMOVE ) )
        {
            juce::ComponentPeer *pEditorPeer(peerWithParentHandle(info.hwnd));
            if (pEditorPeer)
            {
                SpectrumWorxEditor &editor(*LE::Utility::polymorphicDowncast<SpectrumWorxEditor *>(
                    &pEditorPeer->getComponent()));
                RECT parentRect;
                LE_VERIFY(::GetWindowRect(info.hwnd, &parentRect));
                RECT editorRect;
                LE_VERIFY(::GetWindowRect(reinterpret_cast<HWND>(pEditorPeer->getNativeHandle()),
                                          &editorRect));
                unsigned int const parentHorizontalMargin(editorRect.left - parentRect.left);
                unsigned int const parentVerticalMargin(editorRect.top - parentRect.top);
                POINT newEditorLocation = {wp.x + parentHorizontalMargin,
                                           wp.y + parentVerticalMargin};
                // Implementation note:
                //   Because adjustPositions()/juce::ComponentPeer::setPosition()
                // requires screen coordinates we must first call
                // ClientToScreen() because of hosts like FL Studio where editor
                // wrapper windows are also child windows of the main window and
                // thus WINDOWPOS contains client coordinates. For other hosts
                // (whose wrapper windows are merely owned/non-child windows)
                // this will simply 'do nothing' so there is no need for
                // separate logic.
                //                            (25.05.2010.) (Domagoj Saric)
                LE_VERIFY(
                    ::ClientToScreen(::GetAncestor(info.hwnd, GA_PARENT), &newEditorLocation));
                /// \note Account for display scaling/"high DPI display".
                /// http://msdn.microsoft.com/en-us/library/windows/desktop/ms633533(v=vs.85).aspx
                /// http://blogs.msdn.com/b/fontblog/archive/2005/11/08/490490.aspx
                /// http://blogs.msdn.com/b/b8/archive/2012/03/21/scaling-to-different-screens.aspx
                /// http://msdn.microsoft.com/en-us/magazine/dn574798.aspx
                /// http://stackoverflow.com/questions/8060280/getting-an-dpi-aware-correct-rect-from-getwindowrect-from-a-external-window
                /// http://www.juce.com/forum/topic/scaling-factor
                /// http://www.juce.com/forum/topic/density-independent-pixels
                /// http://www.juce.com/forum/topic/another-bad-update
                /// http://www.juce.com/forum/topic/windows-81-double-scaling
                ///                           (25.09.2014.) (Domagoj Saric)
                float const scale(1 / displayScale());
                newEditorLocation.x =
                    Math::round(Math::convert<float>(newEditorLocation.x) * scale);
                newEditorLocation.y =
                    Math::round(Math::convert<float>(newEditorLocation.y) * scale);
                LE_ASSERT(Math::abs(newEditorLocation.x - editor.getScreenX()) <=
                          1); //...mrmlj...rounding error...
                LE_ASSERT(Math::abs(newEditorLocation.y - editor.getScreenY()) <= 1);
                adjustPositions(editor.presetBrowser_.operator->(), editor.settings_.operator->(),
                                newEditorLocation.x, newEditorLocation.y, wp.flags);
            }
        }
    }

    return ::CallNextHookEx(wndProcHook, nCode, wParam, lParam);
}
#endif // _WIN32

namespace
{
void adjustOwnedWindow(juce::Component *const pWindow, unsigned int &x, unsigned int const y,
                       unsigned int const doHide, unsigned int const doShow)
{
    unsigned int const freeWindowDistance(6);

    if (pWindow)
    {
        x -= freeWindowDistance;
        x -= pWindow->getWidth();

        pWindow->setTopLeftPosition(x, y);

#ifndef _WIN32
        LE_ASSUME(!doShow);
        LE_ASSUME(!doHide);
#endif // _WIN32
        if (doHide)
            pWindow->juce::Component::setVisible(false);
        else if (doShow)
            pWindow->juce::Component::setVisible(true);
    }
}
} // anonymous namespace

void OwnedWindowBase::adjustPositions(juce::Component *pFirstWindow, juce::Component *pSecondWindow,
                                      unsigned int const editorX, unsigned int const editorY,
                                      unsigned int const flags)
{
    unsigned int const mainEditorMargin(7);

    unsigned int x(editorX);
    unsigned int const y(editorY + mainEditorMargin);

#ifdef _WIN32
    BOOL const doHide(flags & SWP_HIDEWINDOW);
    BOOL const doShow(flags & SWP_SHOWWINDOW);
#else
    //...mrmlj...clean this up...
    LE_ASSERT(!flags);
    bool const doHide(false);
    bool const doShow(false);
#endif // _WIN32
    LE_ASSERT((!doHide && !doShow) || (!!doHide != !!doShow));

    adjustOwnedWindow(pFirstWindow, x, y, doHide, doShow);
    adjustOwnedWindow(pSecondWindow, x, y, doHide, doShow);
}

void OwnedWindowBase::adjustPositions(SpectrumWorxEditor &parent,
                                      juce::Component *const pFirstWindow,
                                      juce::Component *const pSecondWindow)
{
    juce::Point<int> const parentPosition(parent.getScreenPosition());
    adjustPositions(pFirstWindow, pSecondWindow, parentPosition.getX(), parentPosition.getY(), 0);
}

void OwnedWindowBase::adjustPositionsForPresetBrowser(SpectrumWorxEditor &parent,
                                                      juce::Component *const pCurrentWindowState)
{
    adjustPositions(parent, pCurrentWindowState, parent.settings_.operator->());
}

void OwnedWindowBase::adjustPositionsForSettings(SpectrumWorxEditor &parent,
                                                 juce::Component *const pCurrentWindowState)
{
#ifdef LE_NO_PRESETS
    adjustPositions(parent, nullptr, pCurrentWindowState);
#else
    adjustPositions(parent, parent.presetBrowser_.operator->(), pCurrentWindowState);
#endif // LE_NO_PRESETS
}

#if 0  //...mrmlj...does not work with the latest juce...cleanup...
void AsyncRepainter::repaint( juce::Component & component, int const x, int const y, int const w, int const h )
{
    if ( isThisTheGUIThread() )
    {
        static_cast<AsyncRepainter &>( component ).juce::Component::internalRepaint( x, y, w, h );
    }
    else
    {
        class AsyncRepaintCallback : public juce::CallbackMessage
        {
        public:
            AsyncRepaintCallback( juce::Component & component, int const x, int const y, int const w, int const h )
                : pComponent_( &component ), x_( x ), y_( y ), w_( w ), h_( h )
            {
                juce::CallbackMessage::post();
            }

        private:
            void messageCallback() override
            {
                juce::Component * const pComponent( pComponent_.getComponent() );
                if ( pComponent )
                    static_cast<AsyncRepainter &>( *pComponent ).juce::Component::internalRepaint( x_, y_, w_, h_ );
            }

            juce::Component::SafePointer<juce::Component> pComponent_;
            int const x_;
            int const y_;
            int const w_;
            int const h_;
        };

        new (std::nothrow) AsyncRepaintCallback( component, x, y, w, h );
    }
}
#endif // 0

DrawableText::DrawableText(char const *const text, unsigned int const x, unsigned int const y,
                           unsigned int const width, unsigned int const height,
                           juce::Justification const justification, juce::Font const &font)
{
    glyphs_.addFittedText(font, text, Math::convert<float>(x), Math::convert<float>(y),
                          Math::convert<float>(width), Math::convert<float>(height), justification,
                          1);
}

juce::Font DrawableText::defaultFont()
{
    juce::Font font(Theme::singleton().Theme::getPopupMenuFont());
    font.setHeight(11);
    return font;
}

void BackgroundImage::paint(juce::Graphics &graphics)
{
    graphics.setOpacity(Theme::singleton().settings().globalOpacity);
    paintImage(graphics, image());
}

juce::Image const &BackgroundImage::image() const
{
    LE_ASSERT(pImage_);
    return *pImage_;
}

void BackgroundImage::setImage(juce::Image const &image) { pImage_ = &image; }

BitmapButton::BitmapButton(juce::Component &parent, juce::Image const &on, juce::Image const &off,
                           juce::Colour const &overlayColourWhenOver, bool const toggled)
{
    //...mrmlj...the settings bitmaps are currently broken...
    LE_ASSERT((on.getHeight() == off.getHeight()) || (&on == &resourceBitmap<SettingsOn>()));
    LE_ASSERT(on.getWidth() == off.getWidth());

    /// \note The 2013 comment here explained that juce::Button registered itself
    /// as a listener of its own Value, so a value set through automation or an
    /// LFO came back asynchronously and generated a bogus "value changed"; the
    /// fix was to cut the Button->Value->Button loop with
    /// getToggleStateValue().removeListener(this).
    ///
    ///   That is neither possible nor needed against JUCE 8. Button is no longer
    /// a Value::Listener -- it holds a private helper object (juce_Button.cpp:40)
    /// which this class cannot reach -- and that helper calls setToggleState
    /// with dontSendNotification for the click notification (juce_Button.cpp:58),
    /// on top of an early-out when the state has not actually changed
    /// (juce_Button.cpp:174). JUCE cuts the loop itself now.
    ///
    ///   Read from JUCE's sources rather than observed, so it is worth watching
    /// for doubled automation writes the first time this runs under a host.
    ///                                       (28.07.2026.) (SW port)

    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);

    setImages(true,                  // resizeButtonNowToFitThisImage,
              false,                 // rescaleImagesWhenButtonSizeChanges,
              true,                  // preserveImageProportions,
              off,                   // normalImage
              normalOpacity(),       // imageOpacityWhenNormal,
              normalOverlay(),       // overlayColourWhenNormal,
              juce::Image(),         // overImage,
              overOpacity(),         // imageOpacityWhenOver,
              overlayColourWhenOver, // overlayColourWhenOver,
              on,                    // downImage,
              downOpacity(),         // imageOpacityWhenDown,
              downOverlay(),         // overlayColourWhenDown,
              0.0f                   // hitTestAlphaThreshold
    );

    setClickingTogglesState(toggled);

    addToParentAndShow(parent, *this);
}

//...mrmlj...copy pasted from the juce::ImageButton class because it hides it...
juce::Image BitmapButton::getCurrentImage() const
{
    if (isDown() || getToggleState())
        return getDownImage();

    if (isOver())
        return getOverImage();

    return getNormalImage();
}

juce::Colour const &BitmapButton::downOverlay() { return juce::Colours::transparentWhite; }

juce::Colour const &BitmapButton::defaultOverOverlay() { return juce::Colours::transparentWhite; }

juce::Colour const &BitmapButton::normalOverlay() { return juce::Colours::transparentWhite; }

// Implementation note:
//   The built in JUCE ComboBox does not allow enough customization so we had to
// make our own. Just like the original ComboBox we use the PopupMenu class for
// the implementation. Because the juce::PopupMenu class is limited and/or too
// encapsulated we use here extremely dirty trickery to get to its internal
// details so as to be able to modify it according to our needs in manner that
// is easier and more efficient than that of the original juce::ComboBox (e.g.
// recreating the whole menu when the selection changes, holding duplicates of
// all items etc...) or to workaround bugs (e.g. the menu displaying in wrong
// places when in lower and/or right half of the screen)...
//                                            (17.03.2010.) (Domagoj Saric)
bool PopupMenu::menuActive_(false);

PopupMenu::PopupMenu() : menuHeight_(0), menuWidth_(0) {}

void PopupMenu::addItem(ItemID const newItemId, char const *const newItemText,
                        juce::Image const &icon, bool const enabled)
{
    juce::String text(newItemText);
    updateDimensionsForNewItem(text);
    items_.push_back({newItemId, std::move(text), icon, enabled, false, nullptr});
}

void PopupMenu::addSubMenu(PopupMenu &subMenu, char const *const name)
{
    LE_ASSERT(name);
    juce::String text(name);
    updateDimensionsForNewItem(text);
    items_.push_back({0, std::move(text), juce::Image(), true, false, &subMenu});
}

void PopupMenu::addSectionHeader(char const *const title)
{
    LE_ASSERT(title);
    juce::String text(title);
    updateDimensionsForNewItem(text);
    items_.push_back({0, std::move(text), juce::Image(), false, true, nullptr});
}

void PopupMenu::updateDimensionsForNewItem(juce::String const &itemText)
{
    int idealWidth, idealHeight;
    Theme::singleton().Theme::getIdealPopupMenuItemSize(itemText, false, -1, idealWidth,
                                                        idealHeight);
    menuWidth_ = std::max<unsigned int>(menuWidth_, idealWidth + 4);
    menuHeight_ += idealHeight;
}

/// \note juce::PopupMenu reserves 0 for "the user dismissed the menu", so the
/// IDs handed to it are ours plus one. The 2016 code masked the top byte
/// instead, which cost it the top byte of the ID space.
namespace
{
constexpr int toJuceID(PopupMenu::ItemID const id) { return static_cast<int>(id) + 1; }
constexpr PopupMenu::ItemID fromJuceID(int const id)
{
    return static_cast<PopupMenu::ItemID>(id - 1);
}
} // anonymous namespace

juce::PopupMenu PopupMenu::build(int const tickedIndex) const
{
    juce::PopupMenu menu;
    for (int index(0); index < static_cast<int>(items_.size()); ++index)
    {
        auto const &item(items_[static_cast<std::size_t>(index)]);
        if (item.isSectionHeader)
            menu.addSectionHeader(item.text);
        else if (item.pSubMenu)
            menu.addSubMenu(item.text, item.pSubMenu->build(item.pSubMenu->tickedIndex_), true);
        else
            menu.addItem(toJuceID(item.id), item.text, item.enabled, index == tickedIndex,
                         item.icon);
    }
    return menu;
}

void PopupMenu::showCenteredAtRight(juce::Component const &owner, OnChosen onChosen) const
{
    juce::Point<int> const ownerPosition(owner.getScreenPosition());
    unsigned int const ownerRight(ownerPosition.getX() + owner.getWidth());
    unsigned int const ownerVerticalMiddle(ownerPosition.getY() + (owner.getHeight() / 2));
    showAt(ownerRight + 6, ownerVerticalMiddle - (menuHeight_ / 2), 1,
           1 //...mrmlj...required with latest juce to actually get the menu on the right side...
           ,
           std::move(onChosen));
}

void PopupMenu::showCenteredBelow(juce::Component const &owner, OnChosen onChosen) const
{
    juce::Point<int> point(owner.localPointToGlobal(juce::Point<int>()));

    unsigned int const width(owner.getWidth());
    if (menuWidth_ > static_cast<unsigned int>(width))
    {
        point.setX(point.getX() - ((menuWidth_ - width) / 2));
    }

    showAt(point.getX(), point.getY(), width, owner.getHeight(), std::move(onChosen));
}

void PopupMenu::showAt(unsigned int const x, unsigned int const y, unsigned int const width,
                       unsigned int const height, OnChosen onChosen) const
{
    menuActive_ = true;
    build(tickedIndex_)
        .showMenuAsync(juce::PopupMenu::Options()
                           .withTargetScreenArea(juce::Rectangle<int>(x, y, width, height))
                           .withMinimumWidth(width),
                       [onChosen = std::move(onChosen)](int const chosenID) {
                           menuActive_ = false;
                           if (onChosen)
                               onChosen(chosenID ? OptionalID(fromJuceID(chosenID)) : std::nullopt);
                       });
}

void PopupMenu::clear()
{
    items_.clear();
    tickedIndex_ = -1;
    menuHeight_ = 0;
    menuWidth_ = 0;
}

unsigned int PopupMenu::numberOfItems() const { return static_cast<unsigned int>(items_.size()); }

PopupMenuWithSelection::PopupMenuWithSelection() : currentSelection_(0), currentSelectionID_(0) {}

unsigned int PopupMenuWithSelection::getSelectedIndex() const
{
    LE_ASSERT(hasValidSelection());
    return static_cast<unsigned int>(currentSelection_);
}

unsigned int PopupMenuWithSelection::indexForID(unsigned int const id) const
{
    for (unsigned int index(0); index < numberOfItems(); ++index)
        if (items()[index].id == id)
            return index;
    LE_UNREACHABLE_CODE();
}

void PopupMenuWithSelection::setSelectedIndex(unsigned int const newSelectionIndex)
{
    updateSelection(newSelectionIndex);
    currentSelectionID_ = items()[newSelectionIndex].id + 1;
}

unsigned int PopupMenuWithSelection::getSelectedID() const
{
    LE_ASSERT(hasValidSelection());
    return currentSelectionID_ - 1;
}

void PopupMenuWithSelection::setSelectedID(unsigned int const newSelectionID)
{
    currentSelectionID_ = newSelectionID + 1;
    updateSelection(indexForID(newSelectionID));
}

juce::String const &PopupMenuWithSelection::getSelectedItemText() const
{
    return getItemText(static_cast<unsigned int>(currentSelection_));
}

juce::Image const &PopupMenuWithSelection::getSelectedItemIcon() const
{
    /// \note A reference into our own storage now, so it stays valid for as
    /// long as the item does. It used to point into juce::PopupMenu's internals.
    return items()[static_cast<std::size_t>(currentSelection_)].icon;
}

void PopupMenuWithSelection::updateSelection(unsigned int const newSelectionIndex)
{
    currentSelection_ = static_cast<int>(newSelectionIndex);
    tickedIndex_ = currentSelection_;
}

void PopupMenuWithSelection::clear()
{
    PopupMenu::clear();
    currentSelection_ = 0;
    currentSelectionID_ = 0;
}

bool PopupMenuWithSelection::hasValidSelection() const
{
    return (currentSelectionID_ != 0) &&
           (static_cast<unsigned int>(currentSelection_) < numberOfItems());
}

bool PopupMenuWithSelection::handleNewSelection(OptionalID const &chosenMenuEntryID)
{
    if (chosenMenuEntryID.has_value())
    {
        currentSelectionID_ = *chosenMenuEntryID + 1;
        updateSelection(indexForID(*chosenMenuEntryID));
        return true;
    }
    return false;
}

void PopupMenuWithSelection::showCenteredAtRight(juce::Component const &owner,
                                                 OnSelection onSelection)
{
    PopupMenu::showCenteredAtRight(
        owner, [this, onSelection = std::move(onSelection)](OptionalID const &chosen) {
            onSelection(handleNewSelection(chosen));
        });
}

void PopupMenuWithSelection::showCenteredBelow(juce::Component const &owner,
                                               OnSelection onSelection)
{
    PopupMenu::showCenteredBelow(
        owner, [this, onSelection = std::move(onSelection)](OptionalID const &chosen) {
            onSelection(handleNewSelection(chosen));
        });
}

juce::String const &PopupMenuWithSelection::getItemText(unsigned int const itemIndex) const
{
    return items()[itemIndex].text;
}

ComboBox::ComboBox(juce::Component &parent, juce::Image const &normalBackground,
                   juce::Image const &selectedBackground)
    : normalBackground_(normalBackground), selectedBackground_(selectedBackground)
{
    LE_ASSERT(normalBackground.getWidth() == selectedBackground.getWidth());
    LE_ASSERT(normalBackground.getHeight() == selectedBackground.getHeight());

    setSizeFromImage(*this, normalBackground);
    addToParentAndShow(parent, *this);
}

void ComboBox::paint(juce::Graphics &graphics)
{
    paintImage(graphics, hasDirectFocus() ? selectedBackground_ : normalBackground_);

    graphics.setColour(juce::Colours::white);
    graphics.setFont(Theme::singleton().whiteFont());
    graphics.drawFittedText(getSelectedItemText(), 4, 2, getWidth() - 8,
                            normalBackground_.getHeight() - 3, juce::Justification::centred, 1,
                            0.1f);
}

/// \note \p onValueChanged runs later, on the message thread, and only if the
/// menu actually opened. The SafePointer is the point: a menu can outlive the
/// widget that opened it -- the host can close the editor while it is down --
/// and the 2016 code could not have this problem because the call blocked.
void ComboBox::showMenu(std::function<void(bool)> onValueChanged)
{
    //...mrmlj...temporary workaround for the temporary zero padding workaround...
    if (!isEnabled())
        return;

    if (menuActive())
        return;

    showCenteredBelow(*this, [self = juce::Component::SafePointer<ComboBox>(this),
                              onValueChanged = std::move(onValueChanged)](bool const valueChanged) {
        if (!self)
            return;
        if (valueChanged)
        {
            self->grabKeyboardFocus();
            self->repaint();
        }
        if (onValueChanged)
            onValueChanged(valueChanged);
    });
}

LE_NOINLINE void ComboBox::setSelectedID(unsigned int const newSelectionID)
{
    PopupMenuWithSelection::setSelectedID(newSelectionID);
    repaint();
}

void ComboBox::setSelectedIndex(unsigned int const newSelectionIndex)
{
    PopupMenuWithSelection::setSelectedIndex(newSelectionIndex);
    repaint();
}

void Detail::paintTextButton(BitmapButton const &button, juce::Graphics &g,
                             unsigned int const textX, unsigned int const textY,
                             unsigned int const imageX, unsigned int const imageY,
                             bool const isMouseOverButton, bool const isButtonDown)
{
    g.setColour(juce::Colours::white);
    g.setFont(DrawableText::defaultFont());
    juce::Image const &currentImage(button.getCurrentImage());
    g.drawFittedText(button.getName(), textX, textY, button.getWidth() - textX, 11,
                     juce::Justification::horizontallyCentred, 1);

    Theme::singleton().Theme::drawImageButton(
        g, const_cast<juce::Image *>(&currentImage), imageX, imageY, currentImage.getWidth(),
        currentImage.getHeight(),
        isButtonDown ? BitmapButton::downOverlay()
                     : (isMouseOverButton ? BitmapButton::defaultOverOverlay()
                                          : BitmapButton::normalOverlay()),
        isButtonDown
            ? BitmapButton::downOpacity()
            : (isMouseOverButton ? BitmapButton::overOpacity() : BitmapButton::normalOpacity()),
        const_cast<BitmapButton &>(button));
}

LEDTextButton::LEDTextButton(juce::Component &parent, unsigned int const x, unsigned int const y,
                             char const *const text)
    : BitmapButton(parent, resourceBitmap<LEDOn>(), resourceBitmap<LEDOff>())
{
    setName(text);

    setBounds(x, y, getWidth() + DrawableText::defaultFont().getStringWidth(getName()), 14);
}

void LEDTextButton::paintButton(juce::Graphics &g, bool const isMouseOverButton,
                                bool const isButtonDown)
{
    std::size_t const imageWidth(25);
    LE_ASSERT(getCurrentImage().getWidth() == imageWidth);
    Detail::paintTextButton(*this, g, imageWidth, 3, 0, 0, isMouseOverButton, isButtonDown);
}

TextButton::TextButton(juce::Component &parent, unsigned int const x, unsigned int const y,
                       char const *const text)
{
    setName(text);

    juce::Font font(Theme::singleton().whiteFont());
    font.setHeight(static_cast<float>(height));

    setBounds(x, y, font.getStringWidth(getName()), height);

    setClickingTogglesState(true);

    addToParentAndShow(parent, *this);
}

void TextButton::paintButton(juce::Graphics &g, bool const isMouseOverButton, bool /*isButtonDown*/)
{
#define INTEGER_ALPHA(alpha) static_cast<unsigned char>(alpha * 255)
    static unsigned char const alphas[2]
                                     [2] = /* [not toggled, toggled] [not mouse over, mouse over] */
        {{INTEGER_ALPHA(0.3), INTEGER_ALPHA(0.6)}, {INTEGER_ALPHA(1.0), INTEGER_ALPHA(0.8)}};
#undef INTEGER_ALPHA

    unsigned char const alpha(alphas[getToggleState()][isMouseOverButton]);

    juce::Font font(Theme::singleton().whiteFont());
    font.setHeight(static_cast<float>(height));

    g.setColour(juce::Colour((Theme::blueColour().getARGB() & 0x00FFFFFF) | (alpha << 24)));
    g.setFont(font);
    g.drawSingleLineText(getName(), 0, height);
}

Knob::Knob(juce::Component &parent, unsigned int const x, unsigned int const y,
           unsigned int const xMargin, unsigned int const yMargin)
{
    /// \note The Slider half of the same 2013 fix, and it went the same way.
    /// Slider::valueListener() never existed in stock JUCE -- it was an addition
    /// in the patched fork -- and JUCE 8's own Value listener already calls
    /// setValue with dontSendNotification (juce_Slider.cpp:433), which is what
    /// unhooking it was for. See the note in the BitmapButton constructor.
    ///                                       (28.07.2026.) (SW port)

    setBounds(x, y, xMargin, yMargin);
    //setTooltip             ( title                 );
    setSliderStyle(RotaryVerticalDrag);
    setTextBoxStyle(NoTextBox, true, 0, 0);
    //setPopupDisplayEnabled ( true, 0               ); //...mrmlj...for testing...
    setPopupMenuEnabled(true);
    setMouseDragSensitivity(800);
    addToParentAndShow(parent, *this);
}

void Knob::setupForParameter(char const *const title, juce::Image const &filmStripToSizeFor,
                             param_type const defaultValue)
{
    setName(title);

    unsigned int const imageWidth(filmStripToSizeFor.getWidth());
    unsigned int const imageHeight(filmStripToSizeFor.getHeight() / numberOfKnobSubbitmaps);
    LE_ASSERT((filmStripToSizeFor.getHeight() % numberOfKnobSubbitmaps == 0));
    LE_ASSUME(imageWidth == imageHeight);

    unsigned int const xMargin(getWidth());
    unsigned int const yMargin(getHeight());

    unsigned int const width(imageWidth + xMargin);
    unsigned int const height(imageHeight + yMargin);
    setSize(width, height);

    setDoubleClickReturnValue(true, defaultValue);
}

void Knob::startedDragging() noexcept
{
    if (!Theme::singleton().settings().hideCursorOnKnobDrag)
        return;

    LE_ASSERT(juce::Desktop::getInstance().getNumMouseSources() == 1);
    // \note By value: getMainMouseSource() returns a prvalue in JUCE 8, and
    // enableUnboundedMouseMovement() is const, so a copy does the same work.
    auto mouseSource(juce::Desktop::getInstance().getMainMouseSource());

    /// \note Compared by value, not by address. In 2016 getMainMouseSource()
    /// returned a reference into Desktop's own list, so taking its address and
    /// comparing it with getDraggingMouseSource()'s pointer identified the
    /// source. JUCE 8 returns a prvalue -- MouseInputSource is a handle around a
    /// pimpl -- so `&mouseSource` is the address of the local copy above and
    /// never equals anything Desktop owns. The assertion could then only pass
    /// while nothing was dragging, i.e. it failed on every real knob drag.
    /// operator== compares the pimpl, which is the identity that was meant.
    ///                                       (29.07.2026.) (SW port)
    auto const *const pDraggingSource(juce::Desktop::getInstance().getDraggingMouseSource(0));
    LE_ASSERT(!pDraggingSource || //...mrmlj...double click...
              (*pDraggingSource == mouseSource));

    /// \note setMouseCursor( juce::MouseCursor::NoCursor ) and
    /// enableUnboundedMouseMovement() result in a black box under VMWare.
    ///                                       (10.07.2012.) (Domagoj Saric)
    mouseSource.enableUnboundedMouseMovement(true, false);
    LE_ASSERT(mouseSource.canDoUnboundedMovement());
}

void Knob::stoppedDragging() noexcept
{
    LE_ASSERT(juce::Desktop::getInstance().getNumMouseSources() == 1);
    //juce::MouseInputSource & mouseSource( juce::Desktop::getInstance().getMainMouseSource() );
    // http://www.rawmaterialsoftware.com/viewtopic.php?f=2&t=5628&hilit=enableUnboundedMouseMovement
    //mouseSource.enableUnboundedMouseMovement( false, !Theme::singleton().settings().hideCursorOnKnobDrag );

    //...mrmlj...neither of these works/helps because
    //...mrmlj...enableUnboundedMouseMovement() seems to handle it
    //...mrmlj...automatically (but imprecisely)...
    //juce::Desktop::setMousePosition( juce::Desktop::getLastMouseDownPosition() );
    //juce::Desktop::setMousePosition( this->localPointToGlobal( this->getBounds().getCentre() ) );
}

void LE_NOINLINE Knob::setValue(param_type const newValue)
{
#ifndef NDEBUG
    {
        // Implementation note:
        //   A simple
        // LE_ASSERT( Math::isValueInRange( static_cast<value_type>( newValue ), getMinimum(), getMaximum() ) );
        // assertion would sometimes falsely fail for knobs with
        // quantization-adjusted ranges.
        //                                    (05.05.2011.) (Domagoj Saric)
        Engine::Setup const &engineSetup(SpectrumWorxEditor::fromChild(*this).engineSetup());
        Knob::value_type const maxQuantizationAdjustment(std::max(
            engineSetup.frequencyRangePerBin<Knob::param_type>(), engineSetup.stepTime() * 1000));
        auto const minimum(getMinimum());
        auto const maximum(getMaximum());
        LE_ASSERT_MSG(Math::isValueInRange(static_cast<value_type>(newValue),
                                           minimum - maxQuantizationAdjustment,
                                           maximum + maxQuantizationAdjustment),
                      "Knob value out of range");
    }
#endif // NDEBUG
    juce::Slider::setValue(static_cast<value_type>(newValue), juce::dontSendNotification);
}

void Knob::paint(juce::Image const &filmStrip, unsigned int const xMargin,
                 unsigned int const yMargin, juce::Graphics &graphics)
{
    LE_ASSERT(filmStrip.getWidth() == filmStrip.getHeight() / signed(numberOfKnobSubbitmaps));

    unsigned int const imageWidth(filmStrip.getWidth());
    unsigned int const imageHeight(imageWidth);
    unsigned int const pictureIndex(Math::convert<unsigned int>(
        (numberOfKnobSubbitmaps - 1) * juce::Slider::valueToProportionOfLength(getValue())));
    unsigned int const pictureOffset(pictureIndex * imageHeight);
    LE_ASSERT(pictureIndex < numberOfKnobSubbitmaps);
    LE_ASSERT(pictureOffset < static_cast<unsigned int>(filmStrip.getHeight()));

    graphics.drawImage(filmStrip, xMargin, yMargin, imageWidth, imageHeight, 0, pictureOffset,
                       imageWidth, imageHeight);
}

Knob::param_type Knob::getNormalisedValue() const
{
    Knob::param_type const fullRangeValue(static_cast<param_type>(getValue()));
    Knob::param_type const minimumValue(static_cast<param_type>(getMinimum()));
    Knob::param_type const maximumValue(static_cast<param_type>(getMaximum()));

    return Math::convertLinearRange<Knob::param_type, 0, 1, 1, Knob::param_type>(
        fullRangeValue, minimumValue, maximumValue);
}

EditorKnob::EditorKnob(SpectrumWorxEditor &parent, unsigned int const x, unsigned int const y)
    : Knob(parent, x, y, 0, 0), parameterIndex_(0)
{
    setScrollWheelEnabled(true);
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
}

void EditorKnob::setupForParameter(std::uint8_t const parameterIndex, param_type const minimumValue,
                                   param_type const maximumValue, param_type const defaultValue)
{
    Knob::setupForParameter(nullptr, resourceBitmap<EditorKnobStrip>(), defaultValue);

    parameterIndex_ = parameterIndex;

    setRange(minimumValue, maximumValue, 0);
}

namespace
{
#pragma warning(push)
#pragma warning(disable : 4510) // Default constructor could not be generated.
#pragma warning(disable                                                                            \
                : 4610) // Class can never be instantiated - user-defined constructor required.
struct ParameterPrinter
{
    typedef char const *result_type;
    template <class Parameter> result_type operator()() const
    {
        return LE::Parameters::print<Parameter>(value, engineSetup, buffer);
    }
    Engine::Setup const &engineSetup;
    float const value;
    LE::Parameters::PrintBuffer const buffer;
}; // struct ParameterPrinter
#pragma warning(pop)
} // namespace

void EditorKnob::paint(juce::Graphics &graphics)
{
    Knob::paint(resourceBitmap<EditorKnobStrip>(), 0, 0, graphics);

    // For main knobs we display the value within the knob itself.
    LE_ASSERT(resourceBitmap<EditorKnobStrip>().getWidth() == 55);
    graphics.setColour(juce::Colours::white);
    {
        juce::Font font(Theme::singleton().whiteFont());
        font.setHeight(11);
        graphics.setFont(font);
    }

    //...mrmlj...ugh...
    std::array<char, 20> valueString;
    ParameterPrinter const printer = {editor().engineSetup(), static_cast<float>(getValue()),
                                      LE::Utility::makeSpan(&valueString[0], valueString.size())};
    using LE::Parameters::IndexOf;
    using namespace GlobalParameters;
    typedef GlobalParameters::Parameters GlobalParams;
    switch (parameterIndex_)
    {
    case IndexOf<GlobalParams, InputGain>::value:
        printer.operator()<InputGain>();
        break;
    case IndexOf<GlobalParams, OutputGain>::value:
        printer.operator()<OutputGain>();
        break;
    case IndexOf<GlobalParams, MixPercentage>::value:
        printer.operator()<MixPercentage>();
        break;
        LE_DEFAULT_CASE_UNREACHABLE();
    }
    //...mrmlj...assumes global parameters are static...
    ParameterID::Global parameterID;
    parameterID.index = parameterIndex_;
    char const *const pUnit(
        Plugin2HostPassiveInteropController::ParameterLabelGetter()(parameterID, nullptr));

    graphics.drawFittedText(juce::String(&valueString[0]) + pUnit, 14, 16, 28, 24,
                            juce::Justification::centred, 1, 0.1f);
}

/// \note EditorKnob::valueChanged() lives in spectrumWorxEditor.cpp. It is the
/// only thing in this file that instantiates
/// SpectrumWorxEditor::globalParameterChanged<>, which reaches host() and so
/// needs the complete SpectrumWorx -- and that is what used to drag the whole
/// 2016 VST2 plugin class, and the deleted VST 2.4 SDK behind it, into the
/// widget layer. Everything else here needs the editor declared, not defined.
///                                       (28.07.2026.) (SW port)

void EditorKnob::startedDragging() noexcept
{
    Knob::startedDragging();
    editor().mainKnobDragStarted(parameterIndex_);
}

void EditorKnob::stoppedDragging() noexcept
{
    editor().mainKnobDragStopped(parameterIndex_);
    Knob::stoppedDragging();
}

SpectrumWorxEditor &EditorKnob::editor()
{
    return *LE::Utility::polymorphicDowncast<SpectrumWorxEditor *>(this->getParentComponent());
}

TitledComboBox::TitledComboBox(juce::Component &parent, unsigned int const x, unsigned int const y,
                               char const *const title)
    : ComboBox(parent, resourceBitmap<SettingsCombo>(), resourceBitmap<SettingsComboOn>()),
      title_(title, 4, 0, getWidth() - 8, 13, juce::Justification::left)
{
    TitledComboBox::setBounds(x, y, getWidth(), getHeight() + 15);
}

void TitledComboBox::paint(juce::Graphics &graphics)
{
    if (!hasValidSelection())
        return;
    graphics.setOrigin(0, +12);
    ComboBox::paint(graphics);
    graphics.setOrigin(0, -12);
    title_.draw(graphics);
}

void TitledComboBox::mouseDown(juce::MouseEvent const &)
{
    ComboBox::showMenu(
        [self = juce::Component::SafePointer<TitledComboBox>(this)](bool const valueChanged) {
            if (self && valueChanged)
                //...mrmlj...move...editor/settings specific...
                SpectrumWorxEditor::Settings::comboBoxValueChanged(*self);
        });
}

namespace Detail
{
void addPowerOfTwoValueStringsToComboBox(unsigned int const firstValue,
                                         unsigned int const lastValue, ComboBox &comboBox)
{
    LE_ASSERT_MSG(comboBox.numberOfItems() == 0, "ComboBox already filled.");
    std::array<char, 20> buffer;
    unsigned int value(firstValue);
    while (value <= lastValue)
    {
        Utility::lexical_cast(value, &buffer[0]);
        comboBox.addItem(value, &buffer[0]);
        value *= 2;
    }
    comboBox.setValue(firstValue);
}

void addEnumeratedParameterValueStringsToComboBox(
    LE::Utility::Span<char const *LE_RESTRICT const> strings, ComboBox &comboBox)
{
    LE_ASSERT_MSG(comboBox.numberOfItems() == 0, "ComboBox already filled.");
    ComboBox::value_type parameterValue(0);
    while (strings)
    {
        comboBox.addItem(parameterValue, strings.front());
        ++parameterValue;
        strings.advance_begin(1);
    }
    comboBox.setValue(0);
}
} // namespace Detail

////////////////////////////////////////////////////////////////////////////////
// Theme
////////////////////////////////////////////////////////////////////////////////

/// \note Theme itself now lives in theme.hpp/theme.cpp -- it is a
/// LookAndFeel_V2 there, not a LookAndFeel, and it loads its fonts out of the
/// binary instead of registering them with the operating system. What stays
/// here are the two LFO-update policy queries, which were static members of
/// Theme only because they read Theme::settings(): they ask about a
/// ModuleControlBase and a ModuleUI, neither of which a LookAndFeel should
/// know exist, and their presence is what stopped Theme being separable.
///                                       (28.07.2026.) (SW port)

bool shouldUpdateLFOControl(ModuleControlBase const &control)
{
    Theme::LFOUpdateBehaviour const lfoUpdateBehaviour(Theme::settings().lfoUpdateBehaviour);
    return (lfoUpdateBehaviour == Theme::Always) ||
           (lfoUpdateBehaviour == Theme::WhenControlActive && control.isActive()) ||
           (lfoUpdateBehaviour == Theme::WhenControlSelected &&
            Detail::hasDirectFocus(control.widget()));
}

namespace
{
void onGUIInitialization() { Theme::createSingleton(); }

void onGUIShutdown() { Theme::destroySingleton(); }
} // namespace

//------------------------------------------------------------------------------
} // namespace GUI
//------------------------------------------------------------------------------
} // namespace SW
//------------------------------------------------------------------------------
} // namespace LE
//------------------------------------------------------------------------------

/// \note OSX 10.6 does not provide a std::strnlen implementation so we have to
/// provide one on our own.
///                                           (27.09.2013.) (Domagoj Saric)
#if defined(__APPLE__) /*&& !defined( __LP64__ )*/
extern "C"
{
    size_t __attribute__((weak)) __cdecl strnlen(char const *str, size_t const maxsize_param)
    {
        LE_ASSERT(str);
        unsigned int const maxsize(static_cast<unsigned int>(maxsize_param));
        LE_ASSERT(maxsize == maxsize_param);
        unsigned int n;
        for (n = 0; n < maxsize && *str; n++, str++)
        {
        }
        return n;
    }

    size_t __attribute__((weak)) __cdecl wcsnlen(wchar_t const *wcs, size_t const maxsize_param)
    {
        LE_ASSERT(wcs);
        unsigned int const maxsize(static_cast<unsigned int>(maxsize_param));
        LE_ASSERT(maxsize == maxsize_param);
        unsigned int n;
        for (n = 0; n < maxsize && *wcs; n++, wcs++)
        {
        }
        return n;
    }
};
#endif // __APPLE__ /*&& !__LP64__*/
