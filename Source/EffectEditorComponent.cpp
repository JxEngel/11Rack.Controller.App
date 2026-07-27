#include "EffectEditorComponent.h"
#include "SlotConfig.h"

EffectEditorComponent::EffectEditorComponent (Rack::RackController& controllerToUse)
    : paramsPanel (controllerToUse)
{
    addAndMakeVisible (slotChooserLabel);
    addAndMakeVisible (slotSelector);
    addAndMakeVisible (paramsPanel);

    const auto& slots = slotConfigs();
    for (int i = 0; i < (int) slots.size(); ++i)
        slotSelector.addItem (slots[(size_t) i].name, i + 1);

    slotSelector.onChange = [this]
    {
        auto index = slotSelector.getSelectedId() - 1;
        const auto& slots = slotConfigs();
        if (index >= 0 && index < (int) slots.size())
            paramsPanel.setSlot (slots[(size_t) index]);
    };

    slotSelector.setSelectedId (1, juce::dontSendNotification); // defaults to Distortion
    paramsPanel.setSlot (slots[0]);
}

EffectEditorComponent::~EffectEditorComponent() = default;

void EffectEditorComponent::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto topRow = area.removeFromTop (30);
    slotChooserLabel.setBounds (topRow.removeFromLeft (40).reduced (2));
    slotSelector.setBounds (topRow.removeFromLeft (150).reduced (2));

    area.removeFromTop (6);
    paramsPanel.setBounds (area);
}
