#pragma once

#include <JuceHeader.h>
#include <vector>

// Starting skeleton: lists MIDI devices, connects to one, logs raw traffic in hex,
// and can send a Universal SysEx Identity Request as a first connectivity test.
// This is the seed of the Milestone 3 interface layer described in
// docs/implementation-plan.md - not the final editor UI.
class MainComponent : public juce::Component,
                       private juce::MidiInputCallback
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;

private:
    // A read-only (query-only) ElevenHack-derived SysEx message, for testing the reverse-engineered
    // protocol against real hardware without risking a write to the unit. See docs/protocol-spec.md.
    struct KnownCommand
    {
        juce::String name;
        std::vector<uint8_t> bytes;
    };

    static std::vector<KnownCommand> makeKnownCommands();

    void refreshDeviceLists();
    void openSelectedInput();
    void openSelectedOutput();
    void sendIdentityRequest();
    void sendSelectedKnownCommand();
    void sendSelectRig();
    void sendSysEx (const std::vector<uint8_t>& bytes, const juce::String& description);
    void logMessage (const juce::String& message);

    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;

    juce::ComboBox midiInputSelector;
    juce::ComboBox midiOutputSelector;
    juce::TextButton refreshButton         { "Refresh Devices" };
    juce::TextButton identityRequestButton { "Send Identity Request" };
    juce::ComboBox knownCommandSelector;
    juce::TextButton sendKnownCommandButton { "Send Known Command" };

    // The one WRITE command exposed so far - selecting the active rig is safe/reversible (it's
    // exactly what the front-panel selector does, no stored data is overwritten). Deliberately
    // separate from the read-only commands above. See docs/protocol-spec.md.
    juce::Label rigBankLabel  { {}, "Bank" };
    juce::Slider rigBankSelector;
    juce::Label rigNumberLabel { {}, "Rig #" };
    juce::Slider rigNumberSelector;
    juce::TextButton selectRigButton { "Select Rig (writes to device)" };

    juce::TextEditor logBox;

    juce::Array<juce::MidiDeviceInfo> availableInputs;
    juce::Array<juce::MidiDeviceInfo> availableOutputs;
    std::vector<KnownCommand> knownCommands;

    std::unique_ptr<juce::MidiInput> midiInput;
    std::unique_ptr<juce::MidiOutput> midiOutput;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
