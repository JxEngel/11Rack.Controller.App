#pragma once

#include <JuceHeader.h>

#include <cstdint>
#include <optional>
#include <vector>

// One entry per editable slot, consumed by SignalChainComponent (click-a-block-in-the-chain) via
// SlotParamsPanel. CC numbers are from the official MIDI CC chart (docs/protocol-spec.md);
// `settingCc` is the slot's "Setting N" list in order, positionally mapped to
// `EffectDefinition::params[1..]` (params[0] is always Bypass, handled separately via `bypassCc`).
// Where a slot has fewer settingCc entries than a given effect has params (e.g. Wah), the extra
// params are simply not rendered - not guessed.
//
// Originally extracted (2026-07-27) from a separate EffectEditorComponent.cpp tab so
// SignalChainComponent could reuse the exact same hardware-validated CC data via SlotParamsPanel,
// instead of a second, driftable copy. EffectEditorComponent was later removed entirely
// (2026-08-03) - see SlotParamsPanel.h.
struct SlotConfig
{
    juce::String name;
    uint8_t bypassCc = 0;
    std::vector<uint8_t> settingCc;
    std::vector<int> effectIds; // EffectDefinitions effect IDs to offer in the dropdown
    int defaultEffectId = 0;
    juce::String note;
};

// The 7 slots with both a CC mapping and real per-knob data in EffectDefinitions: Distortion, Wah,
// Mod, Reverb, Delay, FX1, FX2. See each entry's `note` for what is/isn't hardware-confirmed.
const std::vector<SlotConfig>& slotConfigs();

// Finds a slot by its display name (e.g. "Distortion") - returns nullptr if none matches.
const SlotConfig* findSlotByName (const juce::String& name);

// Reconciles a live Bulk Rig decode's raw field tag (see Rack::BulkRigParser::EffectSlot::params)
// against a live-CC EffectDefinitions::ParamDefinition::key, for a given effect - e.g.
// confirmedRawTagForKey("Tape Echo", "Hiss") -> "Hiss". Returns nullopt for any pair that hasn't
// been directly confirmed via a real decoded sample - this is deliberately a small table of
// verified facts, not a positional or name-similarity heuristic, so it never silently guesses.
// See docs/protocol-spec.md for the full cross-reference this is drawn from, including the pairs
// that were investigated and are NOT included here because they didn't exactly match (e.g. Multi
// Chorus's "PreD"/CC key almost certainly corresponds to bulk tag "PDly", but that's a plausible
// name-similarity guess, not a confirmed match, so it's intentionally left out).
std::optional<juce::String> confirmedRawTagForKey (const juce::String& effectName, const juce::String& ccKey);

// Same idea as confirmedRawTagForKey(), but ALSO covers pairs that are only a plausible name-
// similarity/positional match against real decoded data - not a directly confirmed exact string
// match (e.g. Flanger's "Rate" almost certainly corresponds to bulk tag "Sped", but the strings
// differ, so it never showed up in confirmedRawTagForKey()). Falls back to confirmedRawTagForKey()
// first, so anything hard-confirmed is still returned unchanged from there. Kept as a SEPARATE
// function (not merged into the table above) so confirmedRawTagForKey() keeps meaning "hardware-
// proven, no ambiguity" for any caller that needs that guarantee. See docs/protocol-spec.md for how
// each extrapolated pair was derived, and which effects (e.g. Tape Echo's remaining unmatched keys)
// have NO recorded correspondence at all yet and so are still absent even from this wider table.
std::optional<juce::String> bestEffortRawTagForKey (const juce::String& effectName, const juce::String& ccKey);

// Converts a raw Bulk Rig knob value to the 0-127 CC scale SlotParamsPanel's sliders use. Confirmed
// via a real hardware sweep of Sine Wah's Filt knob (2026-07-28): fully down = -2147483648
// (INT32_MIN), exact centre = 0, fully up = 2147483647 (INT32_MAX) - the standard Q31 fixed-point
// pattern (full 32-bit signed range, linear, zero-centred). Applied to every OTHER knob-kind param
// confirmedRawTagForKey() has a raw tag for too, on the assumption this is the unit's one general
// knob encoding rather than something bespoke per effect - not individually re-confirmed for each of
// those, so treat it as a well-supported hypothesis rather than a hardware-proven fact for anything
// other than Wah's Filt. See docs/protocol-spec.md for the full writeup.
int knobRawToCcValue (int32_t raw);
