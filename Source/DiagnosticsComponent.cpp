#include "DiagnosticsComponent.h"

using Rack::RackController;

std::vector<DiagnosticsComponent::KnownAction> DiagnosticsComponent::makeKnownActions()
{
    // Named RackController actions - all read-only queries, confirmed working against real
    // hardware - see docs/protocol-spec.md.
    return {
        { "Request Effect Count",                 [this] { controller.requestEffectCount(); } },
        { "Request Main Volume",                  [this] { controller.requestMainVolume(); } },
        { "Request Current Rig Number",           [this] { controller.requestCurrentRig(); } },
        { "Request Rig Name (Bank 0, Rig 0)",     [this] { controller.requestRigName ({ 0, 0 }); } },
        { "Request Rig Description",              [this] { controller.requestRigDescription(); } },
        { "Request Bulk Rig",                     [this] { controller.requestBulkRig(); } },
    };
}

DiagnosticsComponent::DiagnosticsComponent (Rack::RackController& controllerToUse)
    : controller (controllerToUse)
{
    controller.addListener (this);

    knownActions = makeKnownActions();

    addAndMakeVisible (identityRequestButton);
    addAndMakeVisible (knownActionSelector);
    addAndMakeVisible (sendKnownActionButton);
    addAndMakeVisible (rigBankLabel);
    addAndMakeVisible (rigBankSelector);
    addAndMakeVisible (rigNumberLabel);
    addAndMakeVisible (rigNumberSelector);
    addAndMakeVisible (selectRigButton);
    addAndMakeVisible (effectIndexLabel);
    addAndMakeVisible (effectIndexSelector);
    addAndMakeVisible (requestEffectDescriptionButton);
    addAndMakeVisible (ccNumberLabel);
    addAndMakeVisible (ccNumberSelector);
    addAndMakeVisible (ccValueLabel);
    addAndMakeVisible (ccValueSelector);
    addAndMakeVisible (sendCcButton);
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

    // 0-64, matching the confirmed real Effect Count = 65. Browse freely to see how the index
    // space maps to real effect models - index 0 itself is a mystery ("Eleven"/"DigiElvnELVu",
    // not a real effect name), which is exactly what prompted adding this. See docs/protocol-spec.md.
    effectIndexLabel.attachToComponent (&effectIndexSelector, true);
    effectIndexSelector.setSliderStyle (juce::Slider::IncDecButtons);
    effectIndexSelector.setRange (0, 64, 1);
    effectIndexSelector.setValue (0, juce::dontSendNotification);
    effectIndexSelector.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 24);
    requestEffectDescriptionButton.onClick = [this] { requestSelectedEffectDescription(); };

    // Defaults to CC 69 (Tuner On/Off) / value 127 (On) - see the header comment for why that's a
    // good first test of the CC "Setting N" hypothesis.
    ccNumberLabel.attachToComponent (&ccNumberSelector, true);
    ccNumberSelector.setSliderStyle (juce::Slider::IncDecButtons);
    ccNumberSelector.setRange (0, 127, 1);
    ccNumberSelector.setValue (69, juce::dontSendNotification);
    ccNumberSelector.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 24);

    ccValueLabel.attachToComponent (&ccValueSelector, true);
    ccValueSelector.setSliderStyle (juce::Slider::IncDecButtons);
    ccValueSelector.setRange (0, 127, 1);
    ccValueSelector.setValue (127, juce::dontSendNotification);
    ccValueSelector.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 24);

    sendCcButton.onClick = [this] { sendMidiCc(); };

    logBox.setMultiLine (true);
    logBox.setReadOnly (true);
    logBox.setScrollbarsShown (true);
    logBox.setCaretVisible (false);

    for (int i = 0; i < (int) knownActions.size(); ++i)
        knownActionSelector.addItem (knownActions[(size_t) i].name, i + 1);
    knownActionSelector.setSelectedId (1, juce::dontSendNotification);

    identityRequestButton.onClick = [this] { sendIdentityRequest(); };
    sendKnownActionButton.onClick = [this] { sendSelectedKnownAction(); };
}

DiagnosticsComponent::~DiagnosticsComponent()
{
    controller.removeListener (this);
}

void DiagnosticsComponent::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto buttonRow = area.removeFromTop (30);
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
    auto effectRow = area.removeFromTop (30);
    requestEffectDescriptionButton.setBounds (effectRow.removeFromRight (200).reduced (2));
    effectRow.removeFromLeft (90); // room for the auto-positioned "Effect Index" label
    effectIndexSelector.setBounds (effectRow.removeFromLeft (80).reduced (2));

    area.removeFromTop (6);
    auto ccRow = area.removeFromTop (30);
    sendCcButton.setBounds (ccRow.removeFromRight (200).reduced (2));
    ccRow.removeFromLeft (50); // room for the auto-positioned "CC#" label
    ccNumberSelector.setBounds (ccRow.removeFromLeft (80).reduced (2));
    ccRow.removeFromLeft (60); // room for the auto-positioned "Value" label
    ccValueSelector.setBounds (ccRow.removeFromLeft (80).reduced (2));

    area.removeFromTop (6);
    logBox.setBounds (area);
}

void DiagnosticsComponent::sendIdentityRequest()
{
    // Universal SysEx Identity Request: F0 7E <channel> 06 01 F7 - generic MIDI, not an
    // Eleven-Rack-specific command, so this goes through RackController::sendRaw() rather than a
    // named method. The unit's reply won't parse as an Eleven Rack frame (different vendor ID),
    // so it'll show up via onUnhandledMessage's raw hex dump below.
    std::vector<uint8_t> bytes { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };
    logMessage ("Sent Universal SysEx Identity Request: " + juce::String::toHexString (bytes.data(), (int) bytes.size()));
    controller.sendRaw (bytes);
}

void DiagnosticsComponent::sendMidiCc()
{
    // Plain 3-byte MIDI Control Change on channel 1 (0xB0): B0 <CC#> <value>. Not SysEx, so this
    // goes through RackController::sendRaw() - see docs/protocol-spec.md Open Items for the
    // "CC Setting N -> positional param order" hypothesis this is testing. Any reply (e.g. an
    // ASYNCSET this triggers) will show up via the usual RackController::Listener callbacks if
    // recognized, or onUnhandledMessage's raw hex dump if not.
    auto ccNumber = static_cast<uint8_t> (static_cast<int> (ccNumberSelector.getValue()));
    auto value = static_cast<uint8_t> (static_cast<int> (ccValueSelector.getValue()));

    std::vector<uint8_t> bytes { 0xB0, ccNumber, value };
    logMessage ("Sent MIDI CC " + juce::String (ccNumber) + " = " + juce::String (value) + ": "
                + juce::String::toHexString (bytes.data(), (int) bytes.size()));
    controller.sendRaw (bytes);
}

void DiagnosticsComponent::requestSelectedEffectDescription()
{
    auto index = static_cast<int> (effectIndexSelector.getValue());
    logMessage ("Sent: Request Effect Description (Effect " + juce::String (index) + ")");
    controller.requestEffectDescription (index);
}

void DiagnosticsComponent::sendSelectedKnownAction()
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

void DiagnosticsComponent::sendSelectRig()
{
    // Confirmed working against real hardware - see docs/protocol-spec.md.
    auto bank = (uint8_t) (int) rigBankSelector.getValue();
    auto rig  = (uint8_t) (int) rigNumberSelector.getValue();

    logMessage ("Sent: Select Rig (Bank " + juce::String (bank) + ", Rig " + juce::String (rig) + ")");
    controller.selectRig ({ bank, rig });
}

void DiagnosticsComponent::logMessage (const juce::String& message)
{
    // RackController::Listener callbacks already arrive on the message thread (MidiTransport hops
    // there before calling back), but this is also called directly from button click handlers, so
    // keep the SafePointer + callAsync pattern for uniform safety regardless of caller.
    juce::Component::SafePointer<DiagnosticsComponent> safeThis (this);
    juce::MessageManager::callAsync ([safeThis, message]
    {
        if (safeThis != nullptr)
        {
            safeThis->logBox.moveCaretToEnd();
            safeThis->logBox.insertTextAtCaret (message + "\n");
        }
    });
}

void DiagnosticsComponent::onEffectCountReceived (int count)
{
    logMessage ("Effect Count: " + juce::String (count));
}

void DiagnosticsComponent::onMainVolumeReceived (int volume)
{
    logMessage ("Main Volume: " + juce::String (volume));
}

void DiagnosticsComponent::onCurrentRigReceived (RackController::RigId rig)
{
    logMessage ("Current Rig: Bank " + juce::String (rig.bank) + ", Rig " + juce::String (rig.rig));
}

void DiagnosticsComponent::onRigNameReceived (RackController::RigId rig, const std::string& name)
{
    logMessage ("Rig Name (Bank " + juce::String (rig.bank) + ", Rig " + juce::String (rig.rig)
                + "): " + juce::String (name));
}

void DiagnosticsComponent::onEffectDescriptionReceived (int effectIndex, const std::string& strId, const std::string& name)
{
    logMessage ("Effect Description #" + juce::String (effectIndex) + ": strId=" + juce::String (strId)
                + " name=" + juce::String (name));
}

void DiagnosticsComponent::onTunerStateReceived (bool isOn)
{
    logMessage (juce::String ("Tuner: ") + (isOn ? "On" : "Off"));
}

void DiagnosticsComponent::onRigDescriptionReceived (const std::vector<uint8_t>& rawPayload)
{
    logMessage ("Rig Description [" + juce::String ((int) rawPayload.size()) + " bytes, structure not yet decoded]: "
                + juce::String::toHexString (rawPayload.data(), (int) rawPayload.size()));
}

void DiagnosticsComponent::onBulkRigReceived (const std::vector<uint8_t>& decodedTfxBytes)
{
    logMessage ("Bulk Rig: " + juce::String ((int) decodedTfxBytes.size())
                + " decoded bytes (not yet parsed as .tfx - see docs/implementation-plan.md)");
}

void DiagnosticsComponent::onUnhandledMessage (const std::vector<uint8_t>& rawBytes)
{
    logMessage ("UNHANDLED [" + juce::String ((int) rawBytes.size()) + " bytes]: "
                + juce::String::toHexString (rawBytes.data(), (int) rawBytes.size()));
}
