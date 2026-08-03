#pragma once

#include <JuceHeader.h>
#include "KnobStyle.h"
#include "ToggleStyle.h"

// "Display Options" tab (2026-08-03) - purely visual/UI preferences, no MIDI or RackController
// involvement at all (unlike every other tab), so it takes no constructor arguments. Holds the
// knob style picker (see KnobStyle.h) and the toggle/switch style picker (see ToggleStyle.h) - a
// natural home for any future purely-visual setting (e.g. a later second knob or switch style).
//
// This component owns the picker UI only - it doesn't apply either style itself. MainComponent
// wires onKnobStyleChanged/onToggleStyleChanged to SignalChainComponent::setKnobStyle()/
// setToggleStyle(), which forward to SlotParamsPanel's own setters - the actual consumers.
class DisplayOptionsComponent : public juce::Component
{
public:
    DisplayOptionsComponent();

    void resized() override;

    std::function<void (KnobStyle)> onKnobStyleChanged;
    std::function<void (ToggleStyle)> onToggleStyleChanged;

private:
    juce::Label knobStyleLabel { {}, "Knob Style" };
    juce::ComboBox knobStyleSelector;

    juce::Label toggleStyleLabel { {}, "Switch Style" };
    juce::ComboBox toggleStyleSelector;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DisplayOptionsComponent)
};
