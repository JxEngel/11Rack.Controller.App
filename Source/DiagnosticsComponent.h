#pragma once

#include <JuceHeader.h>
#include "Rack/RackController.h"

#include <functional>
#include <vector>

// Protocol-testing harness: known-command picker, the one validated write (Select Rig), a raw-send
// escape hatch for generic diagnostics (Universal SysEx Identity Request), and a log of decoded
// incoming messages. Predates the real editor UI (Milestone 5) - kept around as a diagnostic tab
// since it's still useful for testing the protocol directly. See docs/implementation-plan.md.
class DiagnosticsComponent : public juce::Component,
                              private Rack::RackController::Listener
{
public:
    explicit DiagnosticsComponent (Rack::RackController& controllerToUse);
    ~DiagnosticsComponent() override;

    void resized() override;

private:
    // A named RackController action, for the "known command" picker.
    struct KnownAction
    {
        juce::String name;
        std::function<void()> action;
    };

    std::vector<KnownAction> makeKnownActions();

    void sendIdentityRequest();
    void sendSelectedKnownAction();
    void sendSelectRig();
    void sendMidiCc();
    void requestSelectedEffectDescription();
    void logMessage (const juce::String& message);

    // Rack::RackController::Listener
    void onEffectCountReceived (int count) override;
    void onMainVolumeReceived (int volume) override;
    void onCurrentRigReceived (Rack::RackController::RigId rig) override;
    void onRigNameReceived (Rack::RackController::RigId rig, const std::string& name) override;
    void onEffectDescriptionReceived (int effectIndex, const std::string& strId, const std::string& name) override;
    void onTunerStateReceived (bool isOn) override;
    void onRigDescriptionReceived (const std::vector<uint8_t>& rawPayload) override;
    void onBulkRigReceived (const std::vector<uint8_t>& decodedTfxBytes) override;
    void onUnhandledMessage (const std::vector<uint8_t>& rawBytes) override;

    juce::TextButton identityRequestButton { "Send Identity Request" };
    juce::ComboBox knownActionSelector;
    juce::TextButton sendKnownActionButton { "Send Known Command" };

    // The one WRITE command exposed so far - selecting the active rig is safe/reversible (it's
    // exactly what the front-panel selector does, no stored data is overwritten). Deliberately
    // separate from the read-only commands above. See docs/protocol-spec.md.
    juce::Label rigBankLabel  { {}, "Bank" };
    juce::Slider rigBankSelector;
    juce::Label rigNumberLabel { {}, "Rig #" };
    juce::Slider rigNumberSelector;
    juce::TextButton selectRigButton { "Select Rig (writes to device)" };

    // Browse effect descriptions by index (0-64, per the confirmed Effect Count = 65) - replaces
    // the old fixed "Effect 0" known-command entry, which wasn't enough to see how the index space
    // maps to real effect models (index 0 itself decodes to a mysterious "Eleven"/"DigiElvnELVu",
    // not any real effect name - see docs/protocol-spec.md).
    juce::Label effectIndexLabel { {}, "Effect Index" };
    juce::Slider effectIndexSelector;
    juce::TextButton requestEffectDescriptionButton { "Request Effect Description" };

    // Untested territory: sends a raw, plain 3-byte MIDI Control Change (not SysEx), to test the
    // official CC chart's "Setting N" hypothesis (docs/protocol-spec.md Open Items) - whether a CC
    // actually tweaks a live effect parameter, and how the positional mapping works in practice.
    // Defaults to CC 69 (Tuner On/Off, per the official chart) as a good first test: it's an
    // unambiguous on/off toggle, and RackController already listens for tuner state changes
    // (onTunerStateReceived, surfaced on the Globals tab) regardless of whether they were
    // triggered via SysEx or CC - giving an immediate, already-built way to confirm it worked.
    juce::Label ccNumberLabel { {}, "CC#" };
    juce::Slider ccNumberSelector;
    juce::Label ccValueLabel { {}, "Value" };
    juce::Slider ccValueSelector;
    juce::TextButton sendCcButton { "Send MIDI CC (test)" };

    juce::TextEditor logBox;

    std::vector<KnownAction> knownActions;

    Rack::RackController& controller;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiagnosticsComponent)
};
