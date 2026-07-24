#include "RigGlobalsComponent.h"

#include <cmath>

using Rack::RackController;

namespace
{
    constexpr double kDisplayCenter = 5.0;
    constexpr double kDisplayHalfRange = 5.0;
    constexpr double kRawHalfRange = 127.0;
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
