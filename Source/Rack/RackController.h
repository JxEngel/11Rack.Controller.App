#pragma once

#include "MidiTransport.h"
#include "SysExFrame.h"

#include <juce_events/juce_events.h>

#include <cstdint>
#include <string>
#include <vector>

// Forward-declared at global scope (matching where RackControllerTests.cpp actually defines it,
// like every other Rack/*Tests.cpp file) so the qualified friend declaration below resolves to
// the real test class rather than accidentally declaring a distinct Rack::RackControllerTests.
class RackControllerTests;

namespace Rack
{
    // The service-layer facade the UI (or anything else) actually talks to. Owns a
    // MidiTransport, uses SysExFrame/SevenBitCodec internally to build outgoing messages and
    // decode incoming ones, and exposes a clean C++ API instead of raw SysEx bytes - see the
    // architecture discussion in docs/project-overview.md.
    //
    // Simplification vs. ElevenHack's ElevenReceiver: that class has near-duplicate switch blocks
    // for RESPOND (reply to our request) vs ASYNCSET (unit self-reporting a change), but both
    // blocks call the same handler methods for every command. This class dispatches identically
    // regardless of message type for the same reason - the distinction wasn't actually used
    // differently. Easy to add `frame.messageType` to the Listener callbacks later if a real need
    // for it turns up.
    class RackController
    {
    public:
        struct RigId
        {
            uint8_t bank = 0;
            uint8_t rig = 0;

            bool operator== (const RigId& other) const noexcept { return bank == other.bank && rig == other.rig; }
            bool operator!= (const RigId& other) const noexcept { return ! (*this == other); }
        };

        // From ElevenRack.java's MAX_RIG_BANK / rig bank layout.
        static constexpr int kRigsPerBank = 104;
        static constexpr int kNumBanks = 2;

        class Listener
        {
        public:
            virtual ~Listener() = default;

            virtual void onEffectCountReceived (int count) {}
            virtual void onMainVolumeReceived (int volume) {}
            virtual void onCurrentRigReceived (RigId rig) {}
            virtual void onRigNameReceived (RigId rig, const std::string& name) {}
            virtual void onEffectDescriptionReceived (int effectIndex, const std::string& strId, const std::string& name) {}
            virtual void onTunerStateReceived (bool isOn) {}

            // Fires once requestAllRigNames() has stepped through every rig slot (individual
            // names still arrive via onRigNameReceived as usual, one at a time).
            virtual void onRigNameFetchComplete() {}

            // Raw/decoded-but-unstructured payloads - see the open items in
            // docs/protocol-spec.md (Rig Description tuple structure, .tfx bulk format).
            virtual void onRigDescriptionReceived (const std::vector<uint8_t>& rawPayload) {}
            virtual void onBulkRigReceived (const std::vector<uint8_t>& decodedTfxBytes) {}

            // Anything that parses as a valid Eleven Rack SysEx frame but with a command byte
            // this class doesn't handle yet, or that doesn't parse as a valid frame at all -
            // useful for spotting gaps in what's been reverse-engineered so far.
            virtual void onUnhandledMessage (const std::vector<uint8_t>& rawBytes) {}
        };

        RackController();
        ~RackController();

        bool connect (const juce::String& inputIdentifier, const juce::String& outputIdentifier);
        void disconnect();
        bool isConnected() const;

        void addListener (Listener* listener);
        void removeListener (Listener* listener);

        // --- Queries - all confirmed working against real hardware, see docs/protocol-spec.md ---
        void requestEffectCount();
        void requestMainVolume();
        void requestCurrentRig();
        void requestRigName (RigId rig);
        void requestRigDescription();
        void requestEffectDescription (int effectIndex);
        void requestBulkRig();

        // Sequentially requests every rig name across both banks (kNumBanks * kRigsPerBank
        // total) - one request, wait for its reply, then the next, mirroring ElevenHack's
        // ElevenInit rather than bursting all requests at once (untested/risky on real hardware).
        // Each name still arrives via the usual onRigNameReceived; onRigNameFetchComplete() fires
        // once every slot has been requested. No timeout/retry if a reply never arrives - see
        // cancelRigNameFetch() as a manual escape hatch, and docs/implementation-plan.md for why
        // this simplification was chosen (matches ElevenHack, no observed unreliability so far).
        void requestAllRigNames();
        void cancelRigNameFetch();

        // Escape hatch for generic/diagnostic messages that aren't Eleven-Rack-specific commands -
        // e.g. a Universal MIDI Identity Request. Deliberately narrow: prefer a named method above
        // for anything that's actually part of this protocol.
        void sendRaw (const std::vector<uint8_t>& bytes);

        // --- Writes ---
        void selectRig (RigId rig); // Confirmed working against real hardware.

        // Confirmed working against real hardware (2026-07-24). `volume` is SIGNED - see
        // SevenBitCodec.h for why (a real hardware mismatch caught a bug where this was originally
        // unsigned, silently making half the real range unreachable). Roughly [-127, 127], with 0
        // at the unit's own displayed "5.0" (center of its 0.0-10.0 scale) - see
        // SignalChainComponent::displayToRaw()/rawToDisplay() for the display-value conversion.
        void setMainVolume (int8_t volume);
        void setTunerOn (bool isOn); // Confirmed working against real hardware (2026-07-24).

        // Plain 3-byte MIDI Control Change on channel 1: the live-tweak mechanism for per-effect
        // parameters (as opposed to the SysEx writes above, which are for whole-rig save/load, not
        // real-time knob turns) - confirmed working against real hardware (2026-07-24): CC 27
        // ("Distortion Setting 1") moved the Overdrive knob of a loaded "Green JRC Disto" exactly
        // as the CC chart's positional "Setting N" hypothesis predicted. See docs/protocol-spec.md
        // "sixth"/"seventh" rounds. No query mechanism exists for CC-controlled values - there is
        // no way to read back a parameter's current value this way, only to set it.
        void sendMidiCc (uint8_t ccNumber, uint8_t value);

        // NOT YET HARDWARE-VALIDATED - ported from ElevenHack's ElevenTransmitter for API
        // completeness only. `saveRig` in particular overwrites stored rig data. Do not wire any
        // of these up to a UI control without a deliberate, separate decision to test them first.
        void saveRig (int rigNumber);
        void setRigName (const std::string& name);

    private:
        void handleIncomingBytes (const std::vector<uint8_t>& bytes);
        void handleParsedFrame (const SysExFrame::ParsedFrame& frame, const std::vector<uint8_t>& rawBytes);
        void advanceRigNameFetch (RigId justReceived);

        MidiTransport transport;
        juce::ListenerList<Listener> listeners;

        bool fetchingAllRigNames = false;
        RigId nextRigNameFetchTarget {};

        // Global-scope qualifier matters here: an unqualified `friend class RackControllerTests;`
        // inside `namespace Rack` would declare/befriend `Rack::RackControllerTests` instead of
        // the actual (global-scope, like all the other Rack/*Tests.cpp files) test class below.
        friend class ::RackControllerTests; // lets the test file drive handleIncomingBytes()
                                             // directly with real captured byte sequences, without
                                             // needing actual hardware or a public test-only method
    };
}
