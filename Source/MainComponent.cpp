#include "MainComponent.h"

std::vector<MainComponent::KnownCommand> MainComponent::makeKnownCommands()
{
    // All REQU (0x01) query commands from ElevenHack's SysEx.java - read-only, safe to send.
    // Frame: F0 13 0B 0F <msg-type> <command> [params...] F7. See docs/protocol-spec.md.
    return {
        { "Request Effect Count",                  { 0xF0, 0x13, 0x0B, 0x0F, 0x01, 0x22, 0xF7 } },
        { "Request Main Volume",                   { 0xF0, 0x13, 0x0B, 0x0F, 0x01, 0x36, 0x00, 0xF7 } },
        { "Request Current Rig Number",             { 0xF0, 0x13, 0x0B, 0x0F, 0x01, 0x02, 0xF7 } },
        { "Request Rig Name (Bank 0, Rig 0)",        { 0xF0, 0x13, 0x0B, 0x0F, 0x01, 0x04, 0x00, 0x00, 0xF7 } },
        { "Request Rig Description",                { 0xF0, 0x13, 0x0B, 0x0F, 0x01, 0x21, 0xF7 } },
        { "Request Effect Description (Effect 0)",   { 0xF0, 0x13, 0x0B, 0x0F, 0x01, 0x20, 0x00, 0xF7 } },
        { "Request Bulk Rig (raw - may span multiple messages)", { 0xF0, 0x13, 0x0B, 0x0F, 0x01, 0x01, 0xF7 } },
    };
}

MainComponent::MainComponent()
{
    knownCommands = makeKnownCommands();

    addAndMakeVisible (midiInputSelector);
    addAndMakeVisible (midiOutputSelector);
    addAndMakeVisible (refreshButton);
    addAndMakeVisible (identityRequestButton);
    addAndMakeVisible (knownCommandSelector);
    addAndMakeVisible (sendKnownCommandButton);
    addAndMakeVisible (logBox);

    logBox.setMultiLine (true);
    logBox.setReadOnly (true);
    logBox.setScrollbarsShown (true);
    logBox.setCaretVisible (false);

    for (int i = 0; i < (int) knownCommands.size(); ++i)
        knownCommandSelector.addItem (knownCommands[(size_t) i].name, i + 1);
    knownCommandSelector.setSelectedId (1, juce::dontSendNotification);

    refreshButton.onClick = [this] { refreshDeviceLists(); };
    identityRequestButton.onClick = [this] { sendIdentityRequest(); };
    sendKnownCommandButton.onClick = [this] { sendSelectedKnownCommand(); };

    midiInputSelector.onChange = [this] { openSelectedInput(); };
    midiOutputSelector.onChange = [this] { openSelectedOutput(); };

    refreshDeviceLists();

    setSize (700, 540);
}

MainComponent::~MainComponent()
{
    if (midiInput != nullptr)
        midiInput->stop();
}

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
    midiInputSelector.setBounds (topRow.removeFromLeft (topRow.getWidth() / 2).reduced (2));
    midiOutputSelector.setBounds (topRow.reduced (2));

    area.removeFromTop (6);
    auto buttonRow = area.removeFromTop (30);
    refreshButton.setBounds (buttonRow.removeFromLeft (150).reduced (2));
    identityRequestButton.setBounds (buttonRow.removeFromLeft (200).reduced (2));

    area.removeFromTop (6);
    auto commandRow = area.removeFromTop (30);
    sendKnownCommandButton.setBounds (commandRow.removeFromRight (180).reduced (2));
    knownCommandSelector.setBounds (commandRow.reduced (2));

    area.removeFromTop (6);
    logBox.setBounds (area);
}

void MainComponent::refreshDeviceLists()
{
    availableInputs = juce::MidiInput::getAvailableDevices();
    availableOutputs = juce::MidiOutput::getAvailableDevices();

    midiInputSelector.clear (juce::dontSendNotification);
    midiInputSelector.addItem ("(no input)", 1);
    for (int i = 0; i < availableInputs.size(); ++i)
        midiInputSelector.addItem (availableInputs[i].name, i + 2);

    midiOutputSelector.clear (juce::dontSendNotification);
    midiOutputSelector.addItem ("(no output)", 1);
    for (int i = 0; i < availableOutputs.size(); ++i)
        midiOutputSelector.addItem (availableOutputs[i].name, i + 2);

    midiInputSelector.setSelectedId (1, juce::dontSendNotification);
    midiOutputSelector.setSelectedId (1, juce::dontSendNotification);

    logMessage ("Found " + juce::String (availableInputs.size()) + " MIDI input(s), "
                + juce::String (availableOutputs.size()) + " MIDI output(s).");
}

void MainComponent::openSelectedInput()
{
    if (midiInput != nullptr)
    {
        midiInput->stop();
        midiInput = nullptr;
    }

    auto id = midiInputSelector.getSelectedId();
    if (id <= 1)
        return;

    auto device = availableInputs[id - 2];
    midiInput = juce::MidiInput::openDevice (device.identifier, this);

    if (midiInput != nullptr)
    {
        midiInput->start();
        logMessage ("Opened MIDI input: " + device.name);
    }
    else
    {
        logMessage ("Failed to open MIDI input: " + device.name);
    }
}

void MainComponent::openSelectedOutput()
{
    midiOutput = nullptr;

    auto id = midiOutputSelector.getSelectedId();
    if (id <= 1)
        return;

    auto device = availableOutputs[id - 2];
    midiOutput = juce::MidiOutput::openDevice (device.identifier);

    if (midiOutput != nullptr)
        logMessage ("Opened MIDI output: " + device.name);
    else
        logMessage ("Failed to open MIDI output: " + device.name);
}

void MainComponent::sendIdentityRequest()
{
    // Universal SysEx Identity Request: F0 7E <channel> 06 01 F7
    sendSysEx ({ 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 }, "Universal SysEx Identity Request");
}

void MainComponent::sendSelectedKnownCommand()
{
    auto id = knownCommandSelector.getSelectedId();
    if (id < 1 || id > (int) knownCommands.size())
    {
        logMessage ("No known command selected.");
        return;
    }

    const auto& command = knownCommands[(size_t) (id - 1)];
    sendSysEx (command.bytes, command.name);
}

void MainComponent::sendSysEx (const std::vector<uint8_t>& bytes, const juce::String& description)
{
    if (midiOutput == nullptr)
    {
        logMessage ("No MIDI output open - select one first.");
        return;
    }

    midiOutput->sendMessageNow (juce::MidiMessage (bytes.data(), (int) bytes.size()));
    logMessage ("Sent " + description + ": "
                + juce::String::toHexString (bytes.data(), (int) bytes.size()));
}

void MainComponent::logMessage (const juce::String& message)
{
    // May be called from the MIDI input's own thread (handleIncomingMidiMessage) as well as the
    // message thread - hop to the message thread before touching the TextEditor, and use a
    // SafePointer in case the component is destroyed while a hop is still in flight.
    juce::Component::SafePointer<MainComponent> safeThis (this);
    juce::MessageManager::callAsync ([safeThis, message]
    {
        if (safeThis != nullptr)
        {
            safeThis->logBox.moveCaretToEnd();
            safeThis->logBox.insertTextAtCaret (message + "\n");
        }
    });
}

void MainComponent::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    logMessage ("IN [" + juce::String (message.getRawDataSize()) + " bytes]: "
                + juce::String::toHexString (message.getRawData(), message.getRawDataSize())
                + "  (" + message.getDescription() + ")");
}
