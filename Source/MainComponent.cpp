#include "MainComponent.h"

MainComponent::MainComponent()
{
    addAndMakeVisible (midiInputSelector);
    addAndMakeVisible (midiOutputSelector);
    addAndMakeVisible (refreshButton);
    addAndMakeVisible (connectionStatusLabel);
    addAndMakeVisible (tabs);

    connectionStatusLabel.setJustificationType (juce::Justification::centredLeft);

    refreshButton.onClick = [this] { refreshDeviceLists(); };
    midiInputSelector.onChange = [this] { updateConnection(); };
    midiOutputSelector.onChange = [this] { updateConnection(); };

    // Owned as members, not heap-allocated - don't let TabbedComponent delete them.
    tabs.addTab ("Diagnostics", juce::Colours::transparentBlack, &diagnosticsComponent, false);
    tabs.addTab ("Rig Browser", juce::Colours::transparentBlack, &rigBrowserComponent, false);

    refreshDeviceLists();

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
    midiInputSelector.setBounds (topRow.removeFromLeft (topRow.getWidth() / 3).reduced (2));
    midiOutputSelector.setBounds (topRow.removeFromLeft (topRow.getWidth() / 2).reduced (2));
    refreshButton.setBounds (topRow.removeFromLeft (150).reduced (2));
    connectionStatusLabel.setBounds (topRow.reduced (2));

    area.removeFromTop (6);
    tabs.setBounds (area);
}

void MainComponent::refreshDeviceLists()
{
    availableInputs = Rack::MidiTransport::getAvailableInputs();
    availableOutputs = Rack::MidiTransport::getAvailableOutputs();

    midiInputSelector.clear (juce::dontSendNotification);
    midiInputSelector.addItem ("(no input)", 1);
    for (int i = 0; i < (int) availableInputs.size(); ++i)
        midiInputSelector.addItem (availableInputs[(size_t) i].name, i + 2);

    midiOutputSelector.clear (juce::dontSendNotification);
    midiOutputSelector.addItem ("(no output)", 1);
    for (int i = 0; i < (int) availableOutputs.size(); ++i)
        midiOutputSelector.addItem (availableOutputs[(size_t) i].name, i + 2);

    midiInputSelector.setSelectedId (1, juce::dontSendNotification);
    midiOutputSelector.setSelectedId (1, juce::dontSendNotification);
    controller.disconnect();

    connectionStatusLabel.setText ("Found " + juce::String ((int) availableInputs.size()) + " input(s), "
                                        + juce::String ((int) availableOutputs.size()) + " output(s).",
                                    juce::dontSendNotification);
}

void MainComponent::updateConnection()
{
    // RackController::connect() takes both directions together, so changing either dropdown
    // reconnects both - a small, deliberate simplification versus fully independent open/close.
    controller.disconnect();

    auto inputId = midiInputSelector.getSelectedId();
    auto outputId = midiOutputSelector.getSelectedId();

    juce::String inputIdentifier = (inputId > 1 && inputId - 2 < (int) availableInputs.size())
                                        ? availableInputs[(size_t) (inputId - 2)].identifier
                                        : juce::String();
    juce::String outputIdentifier = (outputId > 1 && outputId - 2 < (int) availableOutputs.size())
                                         ? availableOutputs[(size_t) (outputId - 2)].identifier
                                         : juce::String();

    if (inputIdentifier.isEmpty() && outputIdentifier.isEmpty())
        return;

    bool connected = controller.connect (inputIdentifier, outputIdentifier);
    connectionStatusLabel.setText (connected ? "Connected." : "Failed to connect - check the selected devices.",
                                    juce::dontSendNotification);
}
