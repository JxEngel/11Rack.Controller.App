#pragma once

#include <JuceHeader.h>

#include <cstdint>
#include <vector>

// One entry per editable slot, shared by EffectEditorComponent (per-slot dropdown) and
// SignalChainComponent (click-a-block-in-the-chain). CC numbers are from the official MIDI CC
// chart (docs/protocol-spec.md); `settingCc` is the slot's "Setting N" list in order, positionally
// mapped to `EffectDefinition::params[1..]` (params[0] is always Bypass, handled separately via
// `bypassCc`). Where a slot has fewer settingCc entries than a given effect has params (e.g. Wah),
// the extra params are simply not rendered - not guessed.
//
// Extracted (2026-07-27) from EffectEditorComponent.cpp so SignalChainComponent can reuse the exact
// same hardware-validated CC data via SlotParamsPanel, instead of a second, driftable copy.
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
