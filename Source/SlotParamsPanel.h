#pragma once

#include <JuceHeader.h>
#include "GoldKnobLookAndFeel.h"
#include "KnobStyle.h"
#include "RockerSwitchLookAndFeel.h"
#include "ToggleStyle.h"
#include "Rack/RackController.h"
#include "Rack/EffectDefinitions.h"
#include "SlotConfig.h"

#include <map>
#include <memory>
#include <optional>
#include <vector>

// Reusable "pick which effect model is loaded into a slot, then edit its bypass + per-param
// controls over MIDI CC" widget. Originally extracted (2026-07-27) from a separate
// EffectEditorComponent tab so SignalChainComponent could show the identical, hardware-validated
// editing UI when a chain block is clicked, instead of a second copy that could drift out of sync
// with bugfixes made to one but not the other. EffectEditorComponent itself was later removed
// entirely (2026-08-03) once SignalChainComponent's click-a-chain-block flow covered everything it
// did (and more - Volume Pedal/Amp-Cab, which EffectEditorComponent never had) - this is now the
// only consumer.
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
//
// A param can also have NO live CC at all (e.g. Volume Pedal's Min Volume/Linear-Log - confirmed
// exact raw tags, but no documented CC anywhere, even after a real hardware CC scan - see
// docs/master-control-map.md §5) yet still have a confirmed decoded value. `rebuildForSelectedEffect()`
// shows these too, beyond `SlotConfig::settingCc`'s range, labelled "(not synced)" - the control is
// still interactive (so it CAN feed a future "Save to Unit" write once one exists), but has no
// `onValueChange`/`onClick` handler, so nothing is ever sent to the unit.
//
// Knob display style (2026-08-03, see KnobStyle.h) is set from outside via setKnobStyle() -
// DisplayOptionsComponent's "Display Options" tab owns the actual picker UI; this panel just
// applies whatever style it's told to every knob-kind control, current and future. Toggle display
// style (see ToggleStyle.h) works identically via setToggleStyle(), applied to bypassToggle and
// every toggle-kind param control.
class SlotParamsPanel : public juce::Component
{
public:
    explicit SlotParamsPanel (Rack::RackController& controllerToUse);
    ~SlotParamsPanel() override;

    void resized() override;

    // Applied to every knob-kind control, current and future - see rebuildPreservingCurrentValues()
    // for why switching styles doesn't discard whatever values are already showing.
    void setKnobStyle (KnobStyle style);

    // Same idea as setKnobStyle(), for every toggle-kind control (bypassToggle + toggle-kind
    // ParamControls) - see RockerSwitchLookAndFeel.h.
    void setToggleStyle (ToggleStyle style);

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

    // Re-shows the currently selected effect's controls exactly as rebuildForSelectedEffect()
    // does, but first reads bypass/toggle/knob values back out of the ABOUT-TO-BE-DESTROYED
    // paramControls/bypassToggle and feeds them back in as the "known" values - so a pure display
    // change (currently only setKnobStyle()) never silently discards whatever you'd already set,
    // the way just calling rebuildForSelectedEffect() with no arguments would (that resets
    // everything to defaults, which is correct when the EFFECT itself changes, but wrong here).
    void rebuildPreservingCurrentValues();

    // Applies currentKnobStyle to one knob-kind control's slider - the single place a new style
    // gets wired in, see the class doc comment.
    void applyKnobStyle (juce::Slider& slider);

    // Same idea, for currentToggleStyle - applied to bypassToggle and every toggle-kind control's
    // juce::ToggleButton.
    void applyToggleStyle (juce::ToggleButton& toggle);

    // Holds exactly one of slider/toggle/combo, matching `kind` - a plain tagged union would be
    // more compact, but this is clearer for a handful of controls that all need addAndMakeVisible.
    struct ParamControl
    {
        Rack::EffectDefinitions::ParamKind kind = Rack::EffectDefinitions::ParamKind::knob;
        juce::String paramKey; // EffectDefinitions::ParamDefinition::key - see rebuildPreservingCurrentValues()
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Slider> slider;       // kind == knob
        std::unique_ptr<juce::ToggleButton> toggle; // kind == toggle
        std::unique_ptr<juce::ComboBox> combo;      // kind == selector
        uint8_t ccNumber = 0;
    };

    // MUST be declared before any Component member whose slider/toggle might call setLookAndFeel()
    // on it (i.e. before bypassToggle/paramControls below) - C++ constructs members in declaration
    // order and destroys them in reverse, so this ordering guarantees both LookAndFeel objects
    // outlive every juce::Slider/juce::ToggleButton that could still be pointing at them, avoiding
    // a dangling-pointer-on-teardown bug.
    GoldKnobLookAndFeel goldKnobLookAndFeel;
    RockerSwitchLookAndFeel rockerSwitchLookAndFeel;
    KnobStyle currentKnobStyle = KnobStyle::goldMetallic;
    ToggleStyle currentToggleStyle = ToggleStyle::rockerSwitch;

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
