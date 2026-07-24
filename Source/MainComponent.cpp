#include "MainComponent.h"

using Rack::RackController;

std::vector<MainComponent::KnownAction> MainComponent::makeKnownActions()
{
    // Named RackController actions - replaces the earlier raw-SysEx-byte "known command" list
    // now that RackController has a dedicated method for each of these. All read-only queries,
    // confirmed working against real hardware - see docs/protocol-spec.md.
    return {
        { "Request Effect Count",                 [this] { controller.requestEffectCount(); } },
        { "Request Main Volume",                  [this] { controller.requestMainVolume(); } },
        { "Request Current Rig Number",           [this] { controller.requestCurrentRig(); } },
        { "Request Rig Name (Bank 0, Rig 0)",     [this] { controller.requestRigName ({ 0, 0 }); } },
        { "Request Rig Description",              [this] { controller.requestRigDescription(); } },
        { "Request Effect Description (Effect 0)",[this] { controller.requestEffectDescription (0); } },
        // Learned (2026-07-24) that this arrives as a single reassembled JUCE message, not
        // multiple fragments - label updated accordingly. See docs/protocol-spec.md.
        { "Request Bulk Rig",                     [this] { controller.requestBulkRig(); } },
    };
}

MainComponent::MainComponent()
{
    controller.addListener (this);

    knownActions = makeKnownActions();

    addAndMakeVisible (midiInputSelector);
    addAndMakeVisible (midiOutputSelector);
    addAndMakeVisible (refreshButton);
    addAndMakeVisible (identityRequestButton);
    addAndMakeVisible (knownActionSelector);
    addAndMakeVisible (sendKnownActionButton);
    addAndMakeVisible (rigBankLabel);
    addAndMakeVisible (rigBankSelector);
    addAndMakeVisible (rigNumberLabel);
    addAndMakeVisible (rigNumberSelector);
    addAndMakeVisible (selectRigButton);
    addAndMakeVisible (logBox);

    // Bank 0-1, rig 0-103 (matches ElevenRack::MAX_RIG_BANK). Initial values reflect the last
    // known current rig at time of writing (Bank 0, Rig 7 = "B4") so up/down by one is one click.
    rigBankLabel.attachToComponent (&rigBankSelector, true);
    rigBankSelector.setSliderStyle (juce::Slider::IncDecButtons);
    rigBankSelector.setRange (0, 1, 1);
    rigBankSelector.setValue (0, juce::dontSendNotification);
    rigBankSelector.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 24);

    rigNumberLabel.attachToComponent (&rigNumberSelector, true);
    rigNumberSelector.setSliderStyle (juce::Slider::IncDecButtons);
    rigNumberSelector.setRange (0, 103, 1);
    rigNumberSelector.setValue (7, juce::dontSendNotification);
    rigNumberSelector.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 24);

    selectRigButton.onClick = [this] { sendSelectRig(); };

    logBox.setMultiLine (true);
    logBox.setReadOnly (true);
    logBox.setScrollbarsShown (true);
    logBox.setCaretVisible (false);

    for (int i = 0; i < (int) knownActions.size(); ++i)
        knownActionSelector.addItem (knownActions[(size_t) i].name, i + 1);
    knownActionSelector.setSelectedId (1, juce::dontSendNotification);

    refreshButton.onClick = [this] { refreshDeviceLists(); };
    identityRequestButton.onClick = [this] { sendIdentityRequest(); };
    sendKnownActionButton.onClick = [this] { sendSelectedKnownAction(); };

    midiInputSelector.onChange = [this] { updateConnection(); };
    midiOutputSelector.onChange = [this] { updateConnection(); };

    refreshDeviceLists();

    setSize (700, 580);
}

MainComponent::~MainComponent()
{
    controller.removeListener (this);
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
    sendKnownActionButton.setBounds (commandRow.removeFromRight (180).reduced (2));
    knownActionSelector.setBounds (commandRow.reduced (2));

    area.removeFromTop (6);
    auto rigSelectRow = area.removeFromTop (30);
    selectRigButton.setBounds (rigSelectRow.removeFromRight (200).reduced (2));
    rigSelectRow.removeFromLeft (50); // room for the auto-positioned "Bank" label
    rigBankSelector.setBounds (rigSelectRow.removeFromLeft (80).reduced (2));
    rigSelectRow.removeFromLeft (50); // room for the auto-positioned "Rig #" label
    rigNumberSelector.setBounds (rigSelectRow.removeFromLeft (80).reduced (2));

    area.removeFromTop (6);
    logBox.setBounds (area);
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

    logMessage ("Found " + juce::String ((int) availableInputs.size()) + " MIDI input(s), "
                + juce::String ((int) availableOutputs.size()) + " MIDI output(s).");
}

void MainComponent::updateConnection()
{
    // RackController::connect() takes both directions together, so changing either dropdown
    // reconnects both - a small, deliberate simplification versus the two independent open/close
    // paths this used to have. Harmless for a one-device test harness like this.
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
    logMessage (connected ? juce::String ("Connected.") : juce::String ("Failed to connect - check the selected devices."));
}

void MainComponent::sendIdentityRequest()
{
    // Universal SysEx Identity Request: F0 7E <channel> 06 01 F7 - generic MIDI, not an
    // Eleven-Rack-specific command, so this goes through RackController::sendRaw() rather than a
    // named method. The unit's reply won't parse as an Eleven Rack frame (different vendor ID),
    // so it'll show up via onUnhandledMessage's raw hex dump below.
    std::vector<uint8_t> bytes { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };
    logMessage ("Sent Universal SysEx Identity Request: " + juce::String::toHexString (bytes.data(), (int) bytes.size()));
    controller.sendRaw (bytes);
}

void MainComponent::sendSelectedKnownAction()
{
    auto id = knownActionSelector.getSelectedId();
    if (id < 1 || id > (int) knownActions.size())
    {
        logMessage ("No known command selected.");
        return;
    }

    const auto& known = knownActions[(size_t) (id - 1)];
    logMessage ("Sent: " + known.name);
    known.action();
}

void MainComponent::sendSelectRig()
{
    // Confirmed working against real hardware - see docs/protocol-spec.md.
    auto bank = (uint8_t) (int) rigBankSelector.getValue();
    auto rig  = (uint8_t) (int) rigNumberSelector.getValue();

    logMessage ("Sent: Select Rig (Bank " + juce::String (bank) + ", Rig " + juce::String (rig) + ")");
    controller.selectRig ({ bank, rig });
}

void MainComponent::logMessage (const juce::String& message)
{
    // RackController::Listener callbacks already arrive on the message thread (MidiTransport hops
    // there before calling back), but this is also called directly from button click handlers, so
    // keep the SafePointer + callAsync pattern for uniform safety regardless of caller.
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

void MainComponent::onEffectCountReceived (int count)
{
    logMessage ("Effect Count: " + juce::String (count));
}

void MainComponent::onMainVolumeReceived (int volume)
{
    logMessage ("Main Volume: " + juce::String (volume));
}

void MainComponent::onCurrentRigReceived (RackController::RigId rig)
{
    logMessage ("Current Rig: Bank " + juce::String (rig.bank) + ", Rig " + juce::String (rig.rig));
}

void MainComponent::onRigNameReceived (RackController::RigId rig, const std::string& name)
{
    logMessage ("Rig Name (Bank " + juce::String (rig.bank) + ", Rig " + juce::String (rig.rig)
                + "): " + juce::String (name));
}

void MainComponent::onEffectDescriptionReceived (int effectIndex, const std::string& strId, const std::string& name)
{
    logMessage ("Effect Description #" + juce::String (effectIndex) + ": strId=" + juce::String (strId)
                + " name=" + juce::String (name));
}

void MainComponent::onTunerStateReceived (bool isOn)
{
    logMessage (juce::String ("Tuner: ") + (isOn ? "On" : "Off"));
}

void MainComponent::onRigDescriptionReceived (const std::vector<uint8_t>& rawPayload)
{
    logMessage ("Rig Description [" + juce::String ((int) rawPayload.size()) + " bytes, structure not yet decoded]: "
                + juce::String::toHexString (rawPayload.data(), (int) rawPayload.size()));
}

void MainComponent::onBulkRigReceived (const std::vector<uint8_t>& decodedTfxBytes)
{
    logMessage ("Bulk Rig: " + juce::String ((int) decodedTfxBytes.size())
                + " decoded bytes (not yet parsed as .tfx - see docs/implementation-plan.md)");
}

void MainComponent::onUnhandledMessage (const std::vector<uint8_t>& rawBytes)
{
    logMessage ("UNHANDLED [" + juce::String ((int) rawBytes.size()) + " bytes]: "
                + juce::String::toHexString (rawBytes.data(), (int) rawBytes.size()));
}
