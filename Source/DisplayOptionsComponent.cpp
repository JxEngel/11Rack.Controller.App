#include "DisplayOptionsComponent.h"

DisplayOptionsComponent::DisplayOptionsComponent()
{
    addAndMakeVisible (knobStyleLabel);
    addAndMakeVisible (knobStyleSelector);
    addAndMakeVisible (toggleStyleLabel);
    addAndMakeVisible (toggleStyleSelector);

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

    // Same pattern as knobStyleSelector above - see ToggleStyle.h for how to add a new style.
    toggleStyleSelector.addItem ("Rocker Switch", (int) ToggleStyle::rockerSwitch + 1);
    toggleStyleSelector.setSelectedId ((int) ToggleStyle::rockerSwitch + 1, juce::dontSendNotification);
    toggleStyleSelector.onChange = [this]
    {
        if (onToggleStyleChanged)
            onToggleStyleChanged ((ToggleStyle) (toggleStyleSelector.getSelectedId() - 1));
    };
}

void DisplayOptionsComponent::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto knobRow = area.removeFromTop (30);
    knobStyleLabel.setBounds (knobRow.removeFromLeft (80).reduced (2));
    knobStyleSelector.setBounds (knobRow.removeFromLeft (160).reduced (2));

    area.removeFromTop (6);
    auto toggleRow = area.removeFromTop (30);
    toggleStyleLabel.setBounds (toggleRow.removeFromLeft (80).reduced (2));
    toggleStyleSelector.setBounds (toggleRow.removeFromLeft (160).reduced (2));
}
