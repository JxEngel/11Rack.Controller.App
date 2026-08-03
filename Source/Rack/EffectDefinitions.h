#pragma once

#include <optional>
#include <string>
#include <vector>

namespace Rack::EffectDefinitions
{
    // Strongly-typed C++ model of the Eleven Rack's effect/parameter roster - what knobs,
    // switches, and selectors each effect model has, and their human-readable names.
    //
    // Ported from ElevenHack's Effect.java / EffectAmpCab.java (https://gitlab.com/schmidg/
    // elevenhack, Apache-2.0, Guillaume Schmid) - not official AVID documentation, and not a
    // line-by-line Java port. See docs/protocol-spec.md.
    //
    // IMPORTANT: this reflects ElevenHack's OWN level of completeness, not full hardware-verified
    // knowledge. Many effect types were only identified by name (plus a Bypass switch) in
    // ElevenHack - their individual knob/selector parameters were never decoded. Those entries
    // have `isFullyKnown = false` here; treat their (near-empty) `params` list as "not yet mapped,"
    // not "this effect genuinely has no parameters."

    enum class ParamKind
    {
        knob,     // a continuous value
        toggle,   // an on/off switch
        selector, // an enumerated choice
    };

    struct SelectOption
    {
        int value = 0;
        std::string name;
    };

    struct ParamDefinition
    {
        std::string key;   // ElevenHack's own internal field name, e.g. "Driv", "bypa" - useful
                            // for cross-referencing back to the source, not a wire protocol value
        std::string label; // human-readable, e.g. "Sustain", "Bypass"
        ParamKind kind = ParamKind::knob;
        int minValue = 0;  // only meaningful for `knob`; ElevenHack defaults to a plain 0-127
                            // range unless a specific effect overrides it (e.g. Tempo, EQ bands)
        int maxValue = 127;
        int step = 1;
        std::vector<SelectOption> options; // only meaningful for `selector`

        // Only meaningful for `toggle`, and only set for a genuine two-named-option switch (e.g.
        // "Chorus"/"Vibrato", "Tri"/"Sine") as opposed to a true on/off control (Bypass, "Vibrato
        // On/Off") - both empty means "plain on/off", rendered as a rocker switch (see ToggleStyle.h);
        // both non-empty means a two-option switch showing these real words instead of ON/OFF (see
        // TwoOptionSwitchStyle.h). optionOffLabel is the false/0-63 state, optionOnLabel is
        // true/64-127 - only Chorus/Vibrato's split is hardware-confirmed (see
        // docs/master-control-map.md); the others follow the same "first word in the label = off"
        // convention but are unconfirmed.
        std::string optionOffLabel;
        std::string optionOnLabel;
    };

    // The 16 signal-chain slot categories a rig position can hold (ElevenRack's
    // `m_effectTypes`/Effect's `EF_TYPE_*` constants) - distinct from the specific effect instance
    // IDs below. Directly relevant to the still-open Rig Description tuple-structure question in
    // docs/protocol-spec.md.
    enum class EffectClass
    {
        ampCab = 0,
        fxLoop = 1,
        vol = 2,
        wah = 3,
        mod = 4,
        reverb = 5,
        delay = 6,
        disto = 7,
        fx1 = 8,
        fx2 = 9,
        input = 10,
        type0B = 11,
        type0C = 12,
        type0D = 13,
        type0E = 14,
        type0F = 15,
        rigParams = 16,
    };

    std::string effectClassName (EffectClass effectClass);

    struct EffectDefinition
    {
        int effectId = 0;   // the specific instance ID (e.g. 55 = WAH_BLACK), or -1 for rig-level
                             // globals (ElevenHack's `RIG_PARAMS`)
        std::string name;   // human-readable, e.g. "Black Wah"
        std::vector<ParamDefinition> params; // in declaration order; "Bypass" first where present
        bool isFullyKnown = true; // false = only the name (and Bypass) is known - see note above
    };

    // Look up a full effect definition by its specific instance ID. Returns std::nullopt for an
    // unrecognized ID (matches ElevenHack's "Unknown code N" fallback behavior).
    std::optional<EffectDefinition> lookup (int effectId);

    // The 16 known Amp/Cab models (the AMP_CAB effect's "sld6" selector), by their canonical small
    // index (0-15) - equivalent to calling lookup(12 /* AMP_CAB */) and reading its selector's
    // options, exposed separately for convenience.
    //
    // KNOWN GAP: ElevenHack's source also lists a second set of large, evenly-spaced 32-bit values
    // mapping to these same 16 names (seemingly an alternate on-the-wire encoding of the same
    // selector) - not modeled here yet. Needs a real hardware capture of an Amp/Cab parameter value
    // to confirm which representation actually appears before it's worth encoding permanently.
    const std::vector<SelectOption>& ampModelOptions();
}
