#include "RigGlobalsComponent.h"

#include <cmath>

using Rack::RackController;

namespace
{
    constexpr double kDisplayCenter = 5.0;
    constexpr double kDisplayHalfRange = 5.0;
    constexpr double kRawHalfRange = 127.0;

    // Fixed CCs, per the official MIDI CC chart (docs/protocol-spec.md) - not part of any
    // per-effect "Setting N" positional scheme, since there's only one Tap Tempo and one FX Loop.
    constexpr uint8_t kTapTempoCc = 64; // "Tap Tempo" - 64-127 = a tap
    constexpr uint8_t kFxLoopBypassCc = 107; // "FX Loop On/Off"
    constexpr uint8_t kFxLoopSendCc = 19;    // "FX Loop Send"
    constexpr uint8_t kFxLoopReturnCc = 108; // "FX Loop Return"
    constexpr uint8_t kFxLoopMixCc = 88;     // "FX Loop Mix"

    void setupKnob (juce::Slider& slider)
    {
        slider.setRange (0, 127, 1);
        slider.setValue (64, juce::dontSendNotification);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 24);
    }
}

int8_t RigGlobalsComponent::displayToRaw (double display)
{
    double raw = (display - kDisplayCenter) * (kRawHalfRange / kDisplayHalfRange);
    raw = juce::jlimit (-127.0, 127.0, raw);
    return static_cast<int8_t> (std::lround (raw));
}

double RigGlobalsComponent::rawToDisplay (int raw)
{
    return kDisplayCenter + (static_cast<double> (raw) * (kDisplayHalfRange / kRawHalfRange));
}

RigGlobalsComponent::RigGlobalsComponent (Rack::RackController& controllerToUse)
    : controller (controllerToUse)
{
    controller.addListener (this);

    addAndMakeVisible (volumeLabel);
    addAndMakeVisible (volumeSlider);
    addAndMakeVisible (tunerLabel);
    addAndMakeVisible (tunerOnButton);
    addAndMakeVisible (tunerOffButton);
    addAndMakeVisible (tunerStatusLabel);
    addAndMakeVisible (tapTempoLabel);
    addAndMakeVisible (tapTempoButton);
    addAndMakeVisible (fxLoopLabel);
    addAndMakeVisible (fxLoopBypassToggle);
    addAndMakeVisible (fxLoopSendLabel);
    addAndMakeVisible (fxLoopSendSlider);
    addAndMakeVisible (fxLoopReturnLabel);
    addAndMakeVisible (fxLoopReturnSlider);
    addAndMakeVisible (fxLoopMixLabel);
    addAndMakeVisible (fxLoopMixSlider);

    // Shows the unit's own 0.0-10.0 display scale, not the raw wire value - see displayToRaw().
    volumeSlider.setRange (0.0, 10.0, 0.1);
    volumeSlider.setValue (kDisplayCenter, juce::dontSendNotification);
    volumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 24);

    // Live two-way sync: dragging sends on every change. Receiving a device-confirmed value
    // (onMainVolumeReceived, below) moves the slider with dontSendNotification so it doesn't loop
    // back into another send.
    volumeSlider.onValueChange = [this]
    {
        controller.setMainVolume (displayToRaw (volumeSlider.getValue()));
    };

    tunerStatusLabel.setText ("Tuner state: unknown (no query exists - reflects the last "
                               "device-confirmed change only)",
                               juce::dontSendNotification);

    tunerOnButton.onClick = [this] { controller.setTunerOn (true); };
    tunerOffButton.onClick = [this] { controller.setTunerOn (false); };

    // Momentary - one click, one tap. Nothing to read back or sync.
    tapTempoButton.onClick = [this] { controller.sendMidiCc (kTapTempoCc, 127); };

    fxLoopBypassToggle.onClick = [this]
    {
        bool bypassed = fxLoopBypassToggle.getToggleState();
        // 0-63 = Off, 64-127 = On, per the official CC chart.
        controller.sendMidiCc (kFxLoopBypassCc, bypassed ? 0 : 127);
    };

    setupKnob (fxLoopSendSlider);
    fxLoopSendSlider.onValueChange = [this]
    {
        controller.sendMidiCc (kFxLoopSendCc, static_cast<uint8_t> (static_cast<int> (fxLoopSendSlider.getValue())));
    };

    setupKnob (fxLoopReturnSlider);
    fxLoopReturnSlider.onValueChange = [this]
    {
        controller.sendMidiCc (kFxLoopReturnCc, static_cast<uint8_t> (static_cast<int> (fxLoopReturnSlider.getValue())));
    };

    setupKnob (fxLoopMixSlider);
    fxLoopMixSlider.onValueChange = [this]
    {
        controller.sendMidiCc (kFxLoopMixCc, static_cast<uint8_t> (static_cast<int> (fxLoopMixSlider.getValue())));
    };
}

RigGlobalsComponent::~RigGlobalsComponent()
{
    controller.removeListener (this);
}

void RigGlobalsComponent::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto volumeRow = area.removeFromTop (30);
    volumeLabel.setBounds (volumeRow.removeFromLeft (90).reduced (2));
    volumeSlider.setBounds (volumeRow.removeFromLeft (350).reduced (2));

    area.removeFromTop (18);
    auto tunerRow = area.removeFromTop (30);
    tunerLabel.setBounds (tunerRow.removeFromLeft (90).reduced (2));
    tunerOnButton.setBounds (tunerRow.removeFromLeft (120).reduced (2));
    tunerOffButton.setBounds (tunerRow.removeFromLeft (120).reduced (2));

    area.removeFromTop (6);
    tunerStatusLabel.setBounds (area.removeFromTop (24));

    area.removeFromTop (18);
    auto tapRow = area.removeFromTop (30);
    tapTempoLabel.setBounds (tapRow.removeFromLeft (90).reduced (2));
    tapTempoButton.setBounds (tapRow.removeFromLeft (80).reduced (2));

    area.removeFromTop (18);
    auto fxLoopHeaderRow = area.removeFromTop (30);
    fxLoopLabel.setBounds (fxLoopHeaderRow.removeFromLeft (90).reduced (2));
    fxLoopBypassToggle.setBounds (fxLoopHeaderRow.removeFromLeft (100).reduced (2));

    area.removeFromTop (6);
    auto fxLoopSendRow = area.removeFromTop (30);
    fxLoopSendLabel.setBounds (fxLoopSendRow.removeFromLeft (90).reduced (2));
    fxLoopSendSlider.setBounds (fxLoopSendRow.removeFromLeft (300).reduced (2));

    area.removeFromTop (6);
    auto fxLoopReturnRow = area.removeFromTop (30);
    fxLoopReturnLabel.setBounds (fxLoopReturnRow.removeFromLeft (90).reduced (2));
    fxLoopReturnSlider.setBounds (fxLoopReturnRow.removeFromLeft (300).reduced (2));

    area.removeFromTop (6);
    auto fxLoopMixRow = area.removeFromTop (30);
    fxLoopMixLabel.setBounds (fxLoopMixRow.removeFromLeft (90).reduced (2));
    fxLoopMixSlider.setBounds (fxLoopMixRow.removeFromLeft (300).reduced (2));
}

void RigGlobalsComponent::onMainVolumeReceived (int volume)
{
    // dontSendNotification - this is us reflecting a device-confirmed value, not a user drag, so
    // it must not re-trigger onValueChange (which would just send the same value right back).
    volumeSlider.setValue (rawToDisplay (volume), juce::dontSendNotification);
}

void RigGlobalsComponent::onTunerStateReceived (bool isOn)
{
    tunerStatusLabel.setText (juce::String ("Tuner state (device-confirmed): ") + (isOn ? "On" : "Off"),
                               juce::dontSendNotification);
}
