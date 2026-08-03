#pragma once

#include <JuceHeader.h>
#include "KnobStyle.h"

// "Display Options" tab (2026-08-03) - purely visual/UI preferences, no MIDI or RackController
// involvement at all (unlike every other tab), so it takes no constructor arguments. Currently
// holds just the knob style picker (see KnobStyle.h); a natural home for any future purely-visual
// setting (e.g. a later "Copper" knob style, if one's ever added).
//
// This component owns the picker UI only - it doesn't apply the style itself. MainComponent wires
// onKnobStyleChanged to SignalChainComponent::setKnobStyle(), which forwards to
// SlotParamsPanel::setKnobStyle() - the actual consumer.
class DisplayOptionsComponent : public juce::Component
{
public:
    DisplayOptionsComponent();

    void resized() override;

    std::function<void (KnobStyle)> onKnobStyleChanged;

private:
    juce::Label knobStyleLabel { {}, "Knob Style" };
    juce::ComboBox knobStyleSelector;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DisplayOptionsComponent)
};
