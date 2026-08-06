////////////////////////////////////////////////////////////////////////////////
///
/// \file spectrumWorxCLAP.hpp
/// -------------------------
///
/// The CLAP plugin. Stage 1 walking skeleton: no DSP, no real GUI. It declares
/// the audio ports and the parameter topology the finished plugin will have, so
/// that the risky host interactions - a 287 entry list whose names, ranges and
/// module paths change under the host when a slot's effect is swapped - can be
/// exercised in every DAW before any SpectrumWorx code is ported.
///
/// See doc/tech/parameter_system.md for what is being modelled.
///
/// Copyright (c) 2026 the SpectrumWorx contributors.
/// SPDX-License-Identifier: GPL-3.0-or-later
///
////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
#ifndef spectrumWorxCLAP_hpp__1F3B9A44_6C52_4D08_9E17_2AB5C7E0D391
#define spectrumWorxCLAP_hpp__1F3B9A44_6C52_4D08_9E17_2AB5C7E0D391
//------------------------------------------------------------------------------
#include "core/automatedModuleChain.hpp"
#include "core/host_interop/host2PluginImpl.hpp"
#include "core/host_interop/plugin2HostImpl.hpp"
#include "core/modules/moduleDSPAndGUI.hpp"
#include "core/spectrumWorxCore.hpp"
#include "core/threading/messages.hpp"
#include "core/threading/valueMailbox.hpp"
#include "external_audio/sample.hpp"
#include "gui/editor/editorHost.hpp"

#include "le/plugins/clap/tag.hpp"

#include <clap/helpers/plugin.hh>
#include <sst/clap_juce_shim/clap_juce_shim.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <vector>
//------------------------------------------------------------------------------
namespace LE::SW
{
//------------------------------------------------------------------------------

namespace GUI
{
class ModuleUI;
class SpectrumWorxEditor;
} // namespace GUI

struct DawExtraState;

clap_plugin_descriptor const *descriptor();
clap_plugin const *createPlugin(clap_host const *);

static constexpr auto misbehaviourLevel = clap::helpers::MisbehaviourHandler::Ignore;
static constexpr auto checkingLevel = clap::helpers::CheckingLevel::Maximal;

using PluginHelper = clap::helpers::Plugin<misbehaviourLevel, checkingLevel>;

/// \note SpectrumWorxCore's constructor is protected and Engine::Processor
/// downcasts to it, so the engine cannot exist except as a base of something.
/// SpectrumWorxSharedImpl is what does that in the finished design; until the
/// protocol template layer of 5.2/5.3 lands, this class is that something.
///
///   The two interop bases are what make the parameter model real: the passive
/// one reads a parameter's value, properties and display string out of the
/// current Program, and the active one writes one back, including the module
/// chain's "which effect is in this slot" selector.
class SpectrumWorxCLAP final
    : public PluginHelper,
      public sst::clap_juce_shim::EditorProvider,
      public SpectrumWorxCore,
      public Plugin2HostPassiveInteropImpl<SpectrumWorxCLAP, Plugins::Protocol::CLAP>,
      public Plugin2HostActiveInteropImpl<SpectrumWorxCLAP, Plugins::Protocol::CLAP>,
      public Host2PluginInteropImpl<SpectrumWorxCLAP, Plugins::Protocol::CLAP>,
      public GUI::EditorHost
{
  public:
    using Protocol = Plugins::Protocol::CLAP;
    using PassiveInterop = Plugin2HostPassiveInteropImpl<SpectrumWorxCLAP, Protocol>;
    /// \note What makes this a Plugin2HostInteropControler, which is what the
    /// editor talks to when the user moves something.
    using Notifications = Plugin2HostActiveInteropImpl<SpectrumWorxCLAP, Protocol>;
    using ActiveInterop = Host2PluginInteropImpl<SpectrumWorxCLAP, Protocol>;

    /// \note Both bases and SpectrumWorxCore declare these; say which.
    using ActiveInterop::setParameter;
    using PassiveInterop::getParameter;
    using PassiveInterop::getParameterDisplay;

    friend class Host2PluginInteropImpl<SpectrumWorxCLAP, Protocol>;

    ////////////////////////////////////////////////////////////////////////////
    // What the interop templates ask of an Impl.
    ////////////////////////////////////////////////////////////////////////////

    /// \note ParameterID rather than ParameterIndex -- the AU choice, not the
    /// VST 2.4 one, because SW::ParameterID::binaryValue *is* a clap_id. That
    /// equivalence is the reason this port targets CLAP at all.
    using ParameterSelector = Plugins::ParameterID;

    /// \note The engine's module class, not Engine::ModuleParameters -- the
    /// interop downcasts to this to read a module's parameters, and this is the
    /// one that carries the UI.
    using Module = SW::Module;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \class UIEdits
    ///
    /// \brief What the editor did, on its way to the host.
    ///
    /// \note A host only accepts parameter changes through the output event
    /// list it hands to process() or flush(), and the editor runs on neither of
    /// those threads. So the edits queue here and drain there. Single producer
    /// (the UI), single consumer (audio), and a full queue drops rather than
    /// blocks -- the host re-reads values on the next rescan anyway, and a
    /// priority inversion on the audio thread would be the worse trade.
    ///
    ////////////////////////////////////////////////////////////////////////////

    /// \note The implementation moved to `Threading::SPSCQueue`, which was
    /// generalised from it -- this was already the right ring and is now one
    /// instantiation of it, sharing its tests with the two the redesign adds.
    struct UIEdit
    {
        enum class Kind : std::uint8_t
        {
            Value,
            GestureBegin,
            GestureEnd
        };

        clap_id id;
        double value;
        Kind kind;
    }; // struct UIEdit

    using UIEdits = Threading::SPSCQueue<UIEdit, 1024>;

    /// \note The 2016 host proxy answered a dozen questions about a VST or AU
    /// host. These are the ones the parameter path reaches.
    class HostProxy
    {
      public:
        explicit HostProxy(SpectrumWorxCLAP const &plugin) : plugin_(plugin) {}

        /// \note Flatly no for CLAP: a host learns that setting one parameter
        /// moved another from the CLAP_EVENT_PARAM_VALUE the plugin queues, not
        /// by being asked to go and look.
        static bool wantsManualDependentParameterNotifications() { return false; }

        /// One parameter moving another -- an LFO bound dragging its partner --
        /// and every parameter the editor itself moves.
        void automatedParameterChanged(ParameterSelector, Plugins::AutomatedParameterValue) const;

        /// A knob drag, which a host records as one undoable automation gesture.
        void automatedParameterBeginEdit(ParameterSelector) const;
        void automatedParameterEndEdit(ParameterSelector) const;

        /// \note The engine's other notion of a gesture: a named block of edits
        /// ("Add module"), for a host that can label an undo step. CLAP has no
        /// call for it -- its gestures are per parameter, above.
        static void gestureBegin(char const * /*description*/) {}
        static void gestureEnd() {}

        /// \note True, meaning "do not push me every parameter of a module that
        /// just changed". The list itself is fixed (see rebuildParameterIDs);
        /// what a slot's effect change alters is names and values, and the
        /// CLAP_PARAM_RESCAN_INFO | TEXT | VALUES sent for it makes the host
        /// re-read all of them.
        static bool parameterListChanged() { return true; }

        void presetChangeBegin() const;
        void presetChangeEnd() const;

        bool reportNewLatencyInSamples(unsigned int) const;

      private:
        SpectrumWorxCLAP const &plugin_;
    }; // class HostProxy

    HostProxy host() const { return HostProxy{*this}; }

    /// The editor, while one is open, else nullptr.
    GUI::SpectrumWorxEditor *gui() const { return pEditor_; }

    /// \note An `EditorModuleInitialiser` shadowed `SpectrumWorxCore`'s here, so
    /// that filling a slot built the module's strip as well as its buffers. It
    /// cannot: the two callers that reached it -- a host parameter event in
    /// `process()` and a preset load -- are on the audio thread and the main
    /// thread respectively, and building a JUCE component on the first is the
    /// whole of symptom 3. The strip follows the chain now (`ToUI::ChainChanged`),
    /// so the initialiser is the DSP half again and there is nothing to shadow.
    ///                                       (02.08.2026.) (SW port)

    /// \note The VST program model prefixed a modified program's name with '*'.
    /// CLAP has a host call for it instead, and it is the host's business
    /// whether that means anything.
    void markCurrentProgramAsModified() const;

  public:
    explicit SpectrumWorxCLAP(clap_host const *);
    ~SpectrumWorxCLAP() override;

    /// Called by the editor, on the UI thread, to drive a slot swap by hand.
    void cycleModuleFromUI(std::uint8_t moduleIndex);
    /// Fires a bare rescan with no state change - the "does the host cope with
    /// an unsolicited rescan" probe.
    void requestRescanFromUI();

    /// Which effect is in \p slot, or noModule.
    std::int8_t effectIn(std::uint8_t slot) const;
    /// \note Fixed for the plugin's lifetime -- see rebuildParameterIDs().
    std::uint16_t parameterCount() const
    {
        return static_cast<std::uint16_t>(parameterIDs_.size());
    }

  protected: // GUI::EditorHost
    /// \note All four are trivial: the engine and the notification layer are
    /// both bases of this class. The interface exists because sw-impl links
    /// sw-gui, so the editor cannot name this type.
    SpectrumWorxCore &core() override { return *this; }
    Plugin2HostInteropControler &automation() override { return *this; }

    void editorOpened(GUI::SpectrumWorxEditor &) override;
    void editorClosed() override;

    bool requestEditorSize(int width, int height) override;

    ////////////////////////////////////////////////////////////////////////////
    // The external audio file the side channel can be fed from.
    //
    // \note Loading is synchronous, on the calling thread, which is the message
    // thread at all three call sites (the editor's menu, a preset that names a
    // sample, and activate() re-reading one at a new sample rate). The 2016
    // build gave it a raw-pthread BackgroundThread; that thread is deleted, and
    // what a deferred load would need instead is on the definition of
    // setNewSample.
    ////////////////////////////////////////////////////////////////////////////

    juce::File currentSampleFile() const override { return sampleFile_; }
    void setNewSample(juce::File const &) override;
    /// \note Always false while the load above is synchronous: by the time
    /// anything can ask, it has finished. See the note on the interface.
    bool isSampleLoadInProgress() const override { return false; }
    void registerSampleLoadedListener(GUI::SpectrumWorxEditor &) override {}
    void deregisterSampleLoadedListener(GUI::SpectrumWorxEditor const &) override {}

    /// \note Audio Units negotiate their channel layout with the host, and
    /// clap-wrapper does present this plugin as one. It is not reachable from
    /// here, though, and the CLAP itself declares its ports outright -- so the
    /// honest answer at this layer is no, and 5.7 revisits it with the input
    /// mode parameter.
    bool completelyDisableIOChanges() const override { return false; }

    /// \note There is no settings file to persist this to yet: the 2016 one
    /// went with the plugin class that owned it, and the session state a host
    /// hands back through clap_plugin_state is a better home for it anyway.
    /// Held in memory so the checkbox at least tracks itself.
    bool shouldLoadLastSessionOnStartup() const override { return loadLastSession_; }
    void shouldLoadLastSessionOnStartup(bool const load) override { loadLastSession_ = load; }

  protected:
    bool init() noexcept override;
    bool activate(double sampleRate, std::uint32_t minFrames,
                  std::uint32_t maxFrames) noexcept override;
    void deactivate() noexcept override;
    clap_process_status process(clap_process const *) noexcept override;
    void reset() noexcept override;
    void onMainThread() noexcept override;

    // clap_plugin_audio_ports
    bool implementsAudioPorts() const noexcept override { return true; }
    std::uint32_t audioPortsCount(bool isInput) const noexcept override;
    bool audioPortsInfo(std::uint32_t index, bool isInput,
                        clap_audio_port_info *) const noexcept override;

    // clap_plugin_params
    bool implementsParams() const noexcept override { return true; }
    bool isValidParamId(clap_id) const noexcept override;
    std::uint32_t paramsCount() const noexcept override;
    bool paramsInfo(std::uint32_t index, clap_param_info *) const noexcept override;
    bool paramsValue(clap_id, double *) noexcept override;
    bool paramsValueToText(clap_id, double, char *display, std::uint32_t size) noexcept override;
    bool paramsTextToValue(clap_id, char const *display, double *) noexcept override;
    void paramsFlush(clap_input_events const *, clap_output_events const *) noexcept override;

    // clap_plugin_state. The preset serialisation plus a <dawExtraState> block;
    // see the note above the definitions and doc/tech/streaming_format.md.
    bool implementsState() const noexcept override { return true; }
    bool stateSave(clap_ostream const *) noexcept override;
    bool stateLoad(clap_istream const *) noexcept override;

    /// \brief The session's non-parameter state, as a pair of hooks over the
    /// `<dawExtraState>` block. Empty for now, and the note on the definition
    /// says what goes in it first.
    DawExtraState sessionState() const;

    // clap_plugin_latency. Cached at activate(): engineSetup() asserts that the
    // setup is current, and the host may ask at any time.
    bool implementsLatency() const noexcept override { return true; }
    std::uint32_t latencyGet() const noexcept override { return latencyInSamples_; }

    // clap_plugin_gui, entirely by way of the shim
    bool implementsGui() const noexcept override { return clapJuceShim_ != nullptr; }
    ADD_SHIM_IMPLEMENTATION(clapJuceShim_)
    ADD_SHIM_LINUX_TIMER(clapJuceShim_)

    // sst::clap_juce_shim::EditorProvider
    std::unique_ptr<juce::Component> createEditor() override;
    bool registerOrUnregisterTimer(clap_id &, int milliseconds, bool registering) override;
    bool registerOrUnregisterPosixFd(int fd, clap_posix_fd_flags_t, bool registering) override;

  protected: // GUI::EditorHost -- the protocol, as the editor sees it
    Threading::ToEngineQueue &toEngine() const override { return toEngine_; }
    Threading::ValueMailbox const &modulatedValues() const override { return values_; }

  private:
    /// \brief Applies everything the main thread has asked for, at the top of
    /// process(). `[audio-thread]`
    void drainCommands();

    /// \brief Applies everything the audio thread has reported, on the main
    /// thread. `[main-thread]`
    void drainEngineEvents();

    /// \brief Hands \p pObject back for the main thread to destroy.
    /// `[audio-thread]` \see the definition.
    void retire(Threading::ToUI::Retired, void *pObject);

    /// \brief Says the chain changed shape, and asks the host to re-read the
    /// parameters that describe it. `[audio-thread]`
    void chainChanged();

    /// \brief Installs \p pNewSample (owned, null clears) as the side channel's
    /// source. `[main-thread]` \see the definition.
    void publishSample(Sample *pNewSample);

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \brief Writes what the LFOs did this block into the mailbox.
    ///
    /// \note Here rather than in the engine, which is the whole point: a module
    /// used to push each LFO value into a `juce::Slider` itself, from inside
    /// `preProcess()`. This is the same information, published where anything
    /// that draws can read it and nothing has to be on this thread to do so.
    ///
    /// \note Only LFO-driven parameters. Not gated on an editor being open; see
    /// the note on the definition.
    ///
    ////////////////////////////////////////////////////////////////////////////
    void publishModulatedValues();

    /// Applies a parameter event. Returns true if it changed a slot's effect,
    /// i.e. if the host's view of the parameter list is now stale.
    bool handleEvent(clap_event_header const *);
    void requestRescan(clap_param_rescan_flags);
    /// `clap_host_params::request_flush`, if the host has one. \see the note.
    void requestParameterFlush() const;
    /// Emits param value events for slot selectors the editor moved.
    void flushUIEdits(clap_output_events const *);

    /// CLAP's module path, which is how a host groups a parameter in its
    /// generic panel -- and these group naturally, by module slot.
    void modulePathFor(ParameterID, char (&path)[CLAP_PATH_SIZE]) const noexcept;

    /// \brief The range a parameter has *right now*, in the effect's own units.
    ///
    /// \note What CLAPEdge normalises against, and deliberately not what
    /// paramsInfo() advertises -- that has to stay put for the plugin's lifetime.
    /// The caller owns the description because the callers are on three different
    /// threads; see the definition.
    ///
    /// \return whether the slot's effect actually owns this parameter. When it
    /// does not, \p ranges is filled with the maximal description instead, so
    /// there is always a usable scale.
    bool liveRanges(ParameterID, Plugins::ParameterInformation<Protocol> &) const;

    /// Advances the engine's LFO timer for this block: from the host's transport
    /// while it is playing, from the block length otherwise. See the definition.
    void updateLFOTiming(clap_process const *) noexcept;

    /// Feeds the engine the sidechain port when the host has one connected, and
    /// the main input otherwise -- the engine reads a side channel whenever the
    /// current input mode calls for one and does not check that it is real.
    void runEngine(clap_process const *) noexcept;

    /// \brief index -> ParameterID, which is what CLAP's paramsInfo(index) needs
    /// and the model does not offer directly.
    ///
    /// \note Rebuilt whenever the parameter list changes, which is what a rescan
    /// means. Held rather than recomputed because the host walks it by index and
    /// getParameterIDs is O(modules x parameters) each time.
    void rebuildParameterIDs();
    std::vector<Plugins::ParameterID> parameterIDs_;

    /// The engine's own; SpectrumWorxCore only holds a pointer.
    Program program_;

    std::unique_ptr<sst::clap_juce_shim::ClapJuceShim> clapJuceShim_;

    std::atomic<std::uint32_t> pendingRescan_{0};

    /// \note `mark_dirty` is main-thread-only and the interop layer marks the
    /// program modified for any automated change, including one that arrived as a
    /// parameter event in process(). Mutable because marking is a const
    /// operation on the plugin -- it tells the host something, it changes nothing.
    mutable std::atomic<bool> pendingMarkDirty_{false};

    /// What the editor moved, waiting for a process() or flush() to carry it to
    /// the host.
    ///
    /// \note Mutable because `HostProxy` holds the plugin by const reference and
    /// its members are const: telling the host something is a const operation on
    /// the plugin -- it changes nothing about what the plugin *is*. The previous
    /// implementation said the same thing by marking the ring's own storage
    /// mutable and its `push` const; saying it once here is the same claim with
    /// one fewer place to get it wrong.
    mutable UIEdits uiEdits_;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \brief The two rings and the mailbox: everything that crosses between the
    /// main thread and the audio thread.
    ///
    /// \note Owned here rather than by the editor, because `paramsValue`,
    /// `paramsValueToText` and `stateSave` are `[main-thread]` calls a host makes
    /// with the window shut. The editor is handed references, and the mailbox as
    /// a `const &` -- it only ever reads it.
    ///
    /// \see core/threading/messages.hpp, doc/tech/threading_model.md §3.
    ///
    ////////////////////////////////////////////////////////////////////////////

    mutable Threading::ToEngineQueue toEngine_;
    Threading::ToUIQueue toUI_;
    Threading::ValueMailbox values_;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// \brief The external audio file feeding the side channel.
    ///
    /// \note Two halves, and the split is the point. `pSample_` belongs to
    /// whichever thread owns the engine and is only ever *swapped*: the message
    /// thread decodes a new one, publishes the pointer, and the old one comes
    /// back through `ToUI::Retire` to be freed off the callback. `sampleFile_`
    /// and `sampleRate_` are the main thread's own record of what it published,
    /// so that `currentSampleFile()` and `activate()`'s re-read at a new rate
    /// answer without touching the audio thread's copy.
    ///
    ///   One `Sample` stood here, written by `setNewSample()` under the
    /// processing lock and read by `runEngine()` under a `try_lock` -- which is
    /// why a load, a preset or an FFT-size change made a block play the host's
    /// side chain port instead of the sample.
    ///                                       (02.08.2026.) (SW port)
    ///
    ////////////////////////////////////////////////////////////////////////////

    Sample *pSample_{nullptr};
    juce::File sampleFile_;
    unsigned int decodedSampleRate_{0};

    /// \note Owned by the shim, which destroys it before this. Cleared in the
    /// editor's own destructor path so a queued notification cannot reach a
    /// dead component.
    GUI::SpectrumWorxEditor *pEditor_{nullptr};

    double sampleRate_{0};
    std::uint32_t latencyInSamples_{0};
    bool engineRunning_{false};
    bool loadLastSession_{false};

    /// \note One outstanding `request_restart` at a time. A preset that moves
    /// the FFT size and the overlap factor is two parameter changes and one
    /// restart, and a host that has not got round to it yet does not need to be
    /// asked again. Cleared in deactivate(), which is where the restart lands.
    bool restartRequested_{false};

    /// \brief A `ToUI::ChainChanged` waiting to be acted on. \see drainEngineEvents().
    bool chainChangedPending_{false};
}; // class SpectrumWorxCLAP

//------------------------------------------------------------------------------
} // namespace LE::SW
//------------------------------------------------------------------------------
#endif // spectrumWorxCLAP_hpp
