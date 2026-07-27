#pragma once

#include <JuceHeader.h>
#include "Rack/MidiTransport.h"
#include "Rack/RackController.h"
#include "DiagnosticsComponent.h"
#include "RigBrowserComponent.h"
#include "RigGlobalsComponent.h"
#include "EffectEditorComponent.h"
#include "SignalChainComponent.h"

#include <vector>

// The app's top-level shell: owns the single RackController instance and the shared device
// connection controls, and hosts the actual feature components as tabs. See
// docs/implementation-plan.md Milestone 5.
class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;

private:
    void refreshDeviceLists();
    void updateConnection();

    juce::ComboBox midiInputSelector;
    juce::ComboBox midiOutputSelector;
    juce::TextButton refreshButton { "Refresh Devices" };
    juce::Label connectionStatusLabel;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    std::vector<Rack::MidiTransport::DeviceInfo> availableInputs;
    std::vector<Rack::MidiTransport::DeviceInfo> availableOutputs;

    Rack::RackController controller;
    DiagnosticsComponent diagnosticsComponent { controller };
    RigBrowserComponent rigBrowserComponent { controller };
    RigGlobalsComponent rigGlobalsComponent { controller };
    EffectEditorComponent effectEditorComponent { controller };
    SignalChainComponent signalChainComponent { controller };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
