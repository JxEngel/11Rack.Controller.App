#pragma once

#include <JuceHeader.h>
#include "Rack/MidiTransport.h"
#include "Rack/RackController.h"

#include <functional>
#include <vector>

// Starting skeleton: lists MIDI devices, connects via RackController, and exercises its API
// (queries, the one validated write, and a generic raw-send escape hatch for diagnostics like the
// Universal SysEx Identity Request). This is the seed of the eventual editor UI (Milestone 5),
// not the UI itself - see docs/implementation-plan.md.
class MainComponent : public juce::Component,
                       private Rack::RackController::Listener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;

private:
    // A named RackController action, for the "known command" picker - replaces the earlier
    // approach of building raw SysEx bytes directly in this file (now RackController's job).
    struct KnownAction
    {
        juce::String name;
        std::function<void()> action;
    };

    std::vector<KnownAction> makeKnownActions();

    void refreshDeviceLists();
    void updateConnection();
    void sendIdentityRequest();
    void sendSelectedKnownAction();
    void sendSelectRig();
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

    juce::ComboBox midiInputSelector;
    juce::ComboBox midiOutputSelector;
    juce::TextButton refreshButton         { "Refresh Devices" };
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

    juce::TextEditor logBox;

    std::vector<Rack::MidiTransport::DeviceInfo> availableInputs;
    std::vector<Rack::MidiTransport::DeviceInfo> availableOutputs;
    std::vector<KnownAction> knownActions;

    Rack::RackController controller;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
