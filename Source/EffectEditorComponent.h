#pragma once

#include <JuceHeader.h>
#include "Rack/RackController.h"
#include "SlotParamsPanel.h"

// Per-slot picker (Milestone 5) over the slots where we have both a CC mapping and real per-knob
// data in EffectDefinitions: Distortion, Wah, Mod, Reverb, Delay, FX1, and FX2 - see SlotConfig.cpp
// for the full per-slot CC data and hardware-confirmation notes. The actual "pick a model, edit its
// bypass + params" UI is `SlotParamsPanel` (extracted 2026-07-27 so SignalChainComponent's
// click-a-chain-block flow can reuse the identical, hardware-validated widget instead of a second,
// driftable copy) - this component is now just the slot dropdown on top of it.
//
// The Amp's tone knobs are still NOT included. The official manual has real per-knob data (16
// named amp models each with their own tone-knob labels) - see docs/protocol-spec.md - but Amp's
// data is shaped differently (one effect ID with 16 selectable models, not 16 separate effect
// IDs), which doesn't fit this component's current "pick an effect ID from a list" pattern. See
// docs/implementation-plan.md Milestone 5 and docs/protocol-spec.md "Open Items".
class EffectEditorComponent : public juce::Component
{
public:
    explicit EffectEditorComponent (Rack::RackController& controllerToUse);
    ~EffectEditorComponent() override;

    void resized() override;

private:
    juce::Label slotChooserLabel { {}, "Slot" };
    juce::ComboBox slotSelector;
    SlotParamsPanel paramsPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectEditorComponent)
};
