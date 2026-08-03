#include "MainComponent.h"

#include <algorithm>

namespace
{
    // "Eleven Rack" is confirmed (2026-07-28, checked directly against this app's own device
    // lists, not just the earlier raw-OS-port-count observation in docs/protocol-spec.md) to
    // appear exactly once per direction - a name match is unambiguous, no picker needed for the
    // common case. Case-insensitive substring match in case the OS ever appends a port suffix.
    bool isElevenRackDevice (const Rack::MidiTransport::DeviceInfo& device)
    {
        return device.name.containsIgnoreCase ("Eleven Rack");
    }
}

MainComponent::MainComponent()
{
    addAndMakeVisible (reconnectButton);
    addAndMakeVisible (connectionStatusLabel);
    addAndMakeVisible (tabs);

    connectionStatusLabel.setJustificationType (juce::Justification::centredLeft);

    reconnectButton.onClick = [this] { connectToRack(); };

    // Owned as members, not heap-allocated - don't let TabbedComponent delete them.
    tabs.addTab ("Diagnostics", juce::Colours::transparentBlack, &diagnosticsComponent, false);
    tabs.addTab ("Signal Chain", juce::Colours::transparentBlack, &signalChainComponent, false);
    tabs.addTab ("Display Options", juce::Colours::transparentBlack, &displayOptionsComponent, false);

    displayOptionsComponent.onKnobStyleChanged = [this] (KnobStyle style)
    {
        signalChainComponent.setKnobStyle (style);
    };
    displayOptionsComponent.onToggleStyleChanged = [this] (ToggleStyle style)
    {
        signalChainComponent.setToggleStyle (style);
    };
    displayOptionsComponent.onTwoOptionSwitchStyleChanged = [this] (TwoOptionSwitchStyle style)
    {
        signalChainComponent.setTwoOptionSwitchStyle (style);
    };

    connectToRack();

    setSize (760, 640);
}

MainComponent::~MainComponent() = default;

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::parentHierarchyChanged()
{
    // JUCE 8 defaults to the Direct2D renderer on Windows, which has been reported to throw
    // COM/WARP exceptions ("No such interface supported") on some systems/GPU drivers. Force the
    // software renderer instead (getPeer() is only non-null once we're actually on screen).
    // Engine 0 is always "Software Renderer" - see juce_Windowing_windows.cpp.
    if (auto* peer = getPeer())
        peer->setCurrentRenderingEngine (0);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto topRow = area.removeFromTop (30);
    reconnectButton.setBounds (topRow.removeFromLeft (130).reduced (2));
    connectionStatusLabel.setBounds (topRow.reduced (2));

    area.removeFromTop (6);
    tabs.setBounds (area);
}

void MainComponent::connectToRack()
{
    controller.disconnect();

    auto inputs = Rack::MidiTransport::getAvailableInputs();
    auto outputs = Rack::MidiTransport::getAvailableOutputs();

    auto inputIt = std::find_if (inputs.begin(), inputs.end(), isElevenRackDevice);
    auto outputIt = std::find_if (outputs.begin(), outputs.end(), isElevenRackDevice);

    if (inputIt == inputs.end() || outputIt == outputs.end())
    {
        connectionStatusLabel.setText ("Eleven Rack not found - click Reconnect to retry.",
                                        juce::dontSendNotification);
        return;
    }

    bool connected = controller.connect (inputIt->identifier, outputIt->identifier);
    connectionStatusLabel.setText (connected
                                        ? "Connected to Eleven Rack."
                                        : "Found Eleven Rack but failed to connect - click Reconnect to retry.",
                                    juce::dontSendNotification);
}
