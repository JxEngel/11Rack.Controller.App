#pragma once

#include <JuceHeader.h>
#include "Rack/MidiTransport.h"
#include "Rack/RackController.h"
#include "DiagnosticsComponent.h"
#include "SignalChainComponent.h"

// The app's top-level shell: owns the single RackController instance and the shared device
// connection controls, and hosts the actual feature components as tabs. See
// docs/implementation-plan.md Milestone 5.
//
// Connection is automatic (2026-07-28), not a manual device picker: on launch, and whenever
// "Reconnect" is clicked, connectToRack() looks for a MIDI input AND output whose name contains
// "Eleven Rack" and connects directly to that pair. Confirmed with the user that in this app's own
// device lists (unlike an earlier, separate observation of the OS's raw MIDI port count - see
// docs/protocol-spec.md's hardware validation log), "Eleven Rack" only ever appears once per
// direction, so a name match is unambiguous - no picker needed for the common case. "Reconnect"
// stays as the manual retry path for when the unit wasn't plugged in / powered on yet at launch.
class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;

private:
    void connectToRack();

    juce::TextButton reconnectButton { "Reconnect" };
    juce::Label connectionStatusLabel;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    // One instance covers tooltips for every tab (e.g. SignalChainComponent's chain block
    // sub-labels) - juce::SettableTooltipClient components need a TooltipWindow somewhere in the
    // hierarchy to actually render, and there wasn't one anywhere in the app yet.
    juce::TooltipWindow tooltipWindow { this };

    Rack::RackController controller;
    DiagnosticsComponent diagnosticsComponent { controller };
    SignalChainComponent signalChainComponent { controller };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
