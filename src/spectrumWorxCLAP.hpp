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
/// See doc/tech/parameter-system.md for what is being modelled.
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
#include "core/spectrumWorxCore.hpp"

#include "le/plugins/clap/tag.hpp"

#include <clap/helpers/plugin.hh>
#include <sst/clap_juce_shim/clap_juce_shim.h>

#include <atomic>
#include <memory>
#include <vector>
//------------------------------------------------------------------------------
namespace LE::SW
{
//------------------------------------------------------------------------------

namespace GUI
{
class ModuleUI;
}

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
      public Host2PluginInteropImpl<SpectrumWorxCLAP, Plugins::Protocol::CLAP>
{
  public:
    using Protocol = Plugins::Protocol::CLAP;
    using PassiveInterop = Plugin2HostPassiveInteropImpl<SpectrumWorxCLAP, Protocol>;
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

    /// \note The 2016 host proxy answered a dozen questions about a VST or AU
    /// host. Two are reached from the parameter path.
    class HostProxy
    {
      public:
        explicit HostProxy(SpectrumWorxCLAP const &plugin) : plugin_(plugin) {}

        /// \note Flatly no for CLAP: a host learns that setting one parameter
        /// moved another from the CLAP_EVENT_PARAM_VALUE the plugin queues, not
        /// by being asked to go and look.
        static bool wantsManualDependentParameterNotifications() { return false; }

        /// One parameter moving another -- an LFO bound dragging its partner.
        void automatedParameterChanged(ParameterSelector, Plugins::AutomatedParameterValue) const;

      private:
        SpectrumWorxCLAP const &plugin_;
    }; // class HostProxy

    HostProxy host() const { return HostProxy{*this}; }

    /// \note `void const *` rather than an editor pointer, which selects the
    /// interop's own guiless overload of updateGUIForChangedModule. Saying "no
    /// GUI" in the type is more honest than handing over a null editor, and it
    /// keeps the editor's definition out of this header. It becomes a real
    /// SpectrumWorxEditor * when there is one.
    void const *gui() const { return nullptr; }

    /// A slot's effect changed, so its parameters are different ones now.
    void moduleChanged(std::uint8_t moduleIndex, Engine::ModuleParameters const *) const;

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

    /// Which effect is in \p slot, or noModule. For the stub editor's labels.
    std::int8_t effectIn(std::uint8_t slot) const;
    std::uint16_t parameterCount() const { return numberOfParameters(&program()); }

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

    // clap_plugin_state
    bool implementsState() const noexcept override { return true; }
    bool stateSave(clap_ostream const *) noexcept override;
    bool stateLoad(clap_istream const *) noexcept override;

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

  private:
    /// Applies a parameter event. Returns true if it changed a slot's effect,
    /// i.e. if the host's view of the parameter list is now stale.
    bool handleEvent(clap_event_header const *);
    void requestRescan(clap_param_rescan_flags);
    /// Emits param value events for slot selectors the editor moved.
    void flushUIEdits(clap_output_events const *);

    /// CLAP's module path, which is how a host groups a parameter in its
    /// generic panel -- and these group naturally, by module slot.
    void modulePathFor(ParameterID, char (&path)[CLAP_PATH_SIZE]) const noexcept;

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
    std::atomic<std::uint32_t> uiEditedSlots_{0};

    double sampleRate_{0};
    std::uint32_t latencyInSamples_{0};
    bool engineRunning_{false};
}; // class SpectrumWorxCLAP

//------------------------------------------------------------------------------
} // namespace LE::SW
//------------------------------------------------------------------------------
#endif // spectrumWorxCLAP_hpp
