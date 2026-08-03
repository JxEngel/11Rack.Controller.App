#include "DisplayOptionsComponent.h"

DisplayOptionsComponent::DisplayOptionsComponent()
{
    addAndMakeVisible (knobStyleLabel);
    addAndMakeVisible (knobStyleSelector);

    // Item IDs are (KnobStyle enum value + 1) - see KnobStyle.h for how to add a new style. Only
    // one exists today, but the selector stays in place so a future second style is just one more
    // addItem() call, not a redesign.
    knobStyleSelector.addItem ("Gold Metallic", (int) KnobStyle::goldMetallic + 1);
    knobStyleSelector.setSelectedId ((int) KnobStyle::goldMetallic + 1, juce::dontSendNotification);
    knobStyleSelector.onChange = [this]
    {
        if (onKnobStyleChanged)
            onKnobStyleChanged ((KnobStyle) (knobStyleSelector.getSelectedId() - 1));
    };
}

void DisplayOptionsComponent::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto row = area.removeFromTop (30);
    knobStyleLabel.setBounds (row.removeFromLeft (80).reduced (2));
    knobStyleSelector.setBounds (row.removeFromLeft (160).reduced (2));
}
