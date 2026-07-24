#pragma once

#include <JuceHeader.h>
#include "Rack/RackController.h"
#include "Rack/EffectDefinitions.h"

#include <memory>
#include <vector>

// Per-effect parameter editing screen (Milestone 5), generalized across the slots where we have
// both a CC mapping and real per-knob data in EffectDefinitions: Distortion, Wah, Mod, and Reverb.
// The CC "Setting N" positional mapping is hardware-confirmed for knobs (Distortion, 2026-07-24 -
// see docs/protocol-spec.md "seventh round": CC 27 moved the Overdrive knob of a loaded "Green JRC
// Disto", exactly matching EffectDefinitions' knob order). Mod and Reverb's real parameter data
// (including the Chorus/Vibrato and Vibe Phaser param order/CCs, and Reverb's Mix/Decay/Tone/
// Pre-Delay) comes from the official Eleven Rack User Guide, Chapter 9 - not yet confirmed against
// real hardware, called out in each slot's note text rather than presented as confirmed.
//
// Delay and the Amp's tone knobs are deliberately NOT included here yet. The official manual does
// have real per-knob data for both (BBD/Dyn/Tape Delay; 16 named amp models each with their own
// tone-knob labels) - see docs/protocol-spec.md - but Delay's CC-to-Setting-N order needs careful
// reconstruction (the manual's print order isn't the real Setting-N order), and Amp's data is
// shaped differently (one effect ID with 16 selectable models, not 16 separate effect IDs), which
// doesn't fit this component's current "pick an effect ID from a list" pattern. FX1/FX2 are also
// omitted - EffectDefinitions doesn't record which effect families a given rig actually assigns to
// those slots. See docs/implementation-plan.md Milestone 5 and docs/protocol-spec.md "Open Items".
//
// Two real limitations, both deliberate and documented rather than silently glossed over:
//  1. There's no way to auto-detect which effect is actually loaded in a slot right now - that
//     depends on the Rig Description tuple structure, still unresolved (see
//     docs/protocol-spec.md open items). You have to tell this screen which model is loaded by
//     picking it from the dropdown yourself.
//  2. There's no query mechanism for CC-controlled values - only a way to set them, never read
//     them back. Controls start at a neutral default and only reflect what YOU set from here, not
//     the unit's actual current state, until you move them.
class EffectEditorComponent : public juce::Component
{
public:
    explicit EffectEditorComponent (Rack::RackController& controllerToUse);
    ~EffectEditorComponent() override;

    void resized() override;

private:
    void rebuildEffectList();
    void rebuildForSelectedEffect();

    // Holds exactly one of slider/toggle/combo, matching `kind` - a plain tagged union would be
    // more compact, but this is clearer for a handful of controls that all need addAndMakeVisible.
    struct ParamControl
    {
        Rack::EffectDefinitions::ParamKind kind = Rack::EffectDefinitions::ParamKind::knob;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Slider> slider;       // kind == knob
        std::unique_ptr<juce::ToggleButton> toggle; // kind == toggle
        std::unique_ptr<juce::ComboBox> combo;      // kind == selector
        uint8_t ccNumber = 0;
    };

    juce::Label slotChooserLabel { {}, "Slot" };
    juce::ComboBox slotSelector;
    juce::Label effectChooserLabel { {}, "Effect" };
    juce::ComboBox effectSelector;
    juce::Label noteLabel;

    std::unique_ptr<juce::ToggleButton> bypassToggle;
    std::vector<ParamControl> paramControls;

    Rack::RackController& controller;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectEditorComponent)
};
