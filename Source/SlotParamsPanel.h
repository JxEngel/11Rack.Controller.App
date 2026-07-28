#pragma once

#include <JuceHeader.h>
#include "Rack/RackController.h"
#include "Rack/EffectDefinitions.h"
#include "SlotConfig.h"

#include <map>
#include <memory>
#include <optional>
#include <vector>

// Reusable "pick which effect model is loaded into a slot, then edit its bypass + per-param
// controls over MIDI CC" widget. Extracted (2026-07-27) from EffectEditorComponent so
// SignalChainComponent can show the identical, hardware-validated editing UI when a chain block is
// clicked, instead of a second copy that could drift out of sync with bugfixes made to one but not
// the other.
//
// Two real limitations, both deliberate and documented rather than silently glossed over:
//  1. There's no way to auto-detect which effect is actually loaded in a slot from MIDI CC alone -
//     that's why `setSlot()` takes an optional `preferredEffectId`: SignalChainComponent can pass
//     one in from a live Bulk Rig decode (see BulkRigParser.h) when connected to real hardware;
//     otherwise this falls back to the slot's own default.
//  2. There's no query mechanism for per-knob CC-controlled values - only a way to set them, never
//     read them back. Controls start at a neutral default and only reflect what YOU set from here,
//     not the unit's actual current state, until you move them. Bypass state is one exception - see
//     `knownBypass` below - it's a plain 0/1 in the Bulk Rig payload, no calibration needed. TOGGLE-
//     and KNOB-kind controls are the same story (see `knownToggleStates`/`knownKnobValues`) - but
//     only for the specific params SlotConfig.cpp's `bestEffortRawTagForKey()` has a raw tag for
//     (exact-confirmed, or a documented name-similarity match against real data), and for knobs,
//     converted via `knobRawToCcValue()`'s hardware-derived Q31 formula (confirmed 2026-07-28 via a
//     full-range sweep of Wah's Filt knob - see docs/protocol-spec.md "Round 2"). Selectors have no
//     confirmed encoding yet.
class SlotParamsPanel : public juce::Component
{
public:
    explicit SlotParamsPanel (Rack::RackController& controllerToUse);
    ~SlotParamsPanel() override;

    void resized() override;

    // Shows `slot`'s effect-selector + bypass + param controls. If `preferredEffectId` is present
    // in `slot.effectIds`, it's pre-selected instead of `slot.defaultEffectId` - use this when a
    // live Bulk Rig decode says which model is really loaded. If `knownBypass` is present, the
    // bypass toggle starts in that state instead of off - use this with the decoded `bypa` value.
    // `knownToggleStates` (keyed by ParamDefinition::key) seeds any OTHER toggle-kind control this
    // effect has a confirmed real value for - see SlotConfig.h's `confirmedRawTagForKey()`. Any key
    // not present here just keeps its default (unchecked) state, same as today. `knownKnobValues`
    // is the same idea for knob-kind controls, already converted to the 0-127 CC scale (see
    // SlotConfig.h's `knobRawToCcValue()`) - any key not present keeps the neutral mid-range default.
    void setSlot (const SlotConfig& slot, int preferredEffectId = -1, std::optional<bool> knownBypass = {},
                  const std::map<juce::String, bool>& knownToggleStates = {},
                  const std::map<juce::String, int>& knownKnobValues = {});

    // No slot selected - clears the panel to empty (used for chain blocks with no SlotConfig yet,
    // e.g. Volume Pedal/Amp/Cab/FX Loop - SignalChainComponent shows its own fallback label instead
    // of this panel in that case, but clearing keeps this panel's own state honest either way).
    void clear();

private:
    void rebuildEffectList();
    void rebuildForSelectedEffect (std::optional<bool> knownBypass = {},
                                   const std::map<juce::String, bool>& knownToggleStates = {},
                                   const std::map<juce::String, int>& knownKnobValues = {});

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

    juce::Label effectChooserLabel { {}, "Effect" };
    juce::ComboBox effectSelector;
    juce::Label noteLabel;

    // Scrollable, since some effects (e.g. Para EQ, FX1/FX2) have up to 14 real params - more rows
    // than reliably fit in the window regardless of size. `paramsContent` holds the actual bypass
    // toggle + param rows and is resized to whatever height they need; `paramsViewport` clips/
    // scrolls it within the space actually available.
    juce::Viewport paramsViewport;
    juce::Component paramsContent;

    std::unique_ptr<juce::ToggleButton> bypassToggle;
    std::vector<ParamControl> paramControls;

    const SlotConfig* currentSlot = nullptr;
    uint8_t currentBypassCc = 0;

    Rack::RackController& controller;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlotParamsPanel)
};
