#include "SlotConfig.h"

#include <limits>
#include <map>

const std::vector<SlotConfig>& slotConfigs()
{
    static const std::vector<SlotConfig> configs = {
        {
            "Distortion",
            25, // "Distortion On/Off"
            { 27, 78, 79, 80, 81, 82, 83 }, // "Distortion Setting 1-7"
            { 29, 30, 31, 87, 91 },
            31, // Green JRC Disto - the model loaded during the 2026-07-24 hardware confirmation
            "No live readback for per-knob values - MIDI CC has no query mechanism, so knobs only "
            "reflect what YOU set here, not the unit's actual current values. (Bypass state and "
            "which model is loaded CAN now be read from a live Bulk Rig decode - see "
            "SignalChainComponent.) Pick the model that matches what's really loaded in the unit's "
            "Distortion slot before adjusting anything. Hardware-confirmed 2026-07-24 (Bypass + all "
            "3 knobs).",
        },
        {
            "Wah",
            43, // "Wah On/Off"
            { 4 }, // "Wah Pedal" - the chart has no "Setting N" scheme for Wah, just this one CC
            { 36, 55 },
            36, // Sine Wah
            "Only the Position knob (\"Wah Pedal\", CC 4) has a confirmed CC mapping - the "
            "second knob (VxCr) has no known CC in the official chart and is intentionally "
            "omitted rather than guessed. Not yet hardware-tested. No live readback for per-knob "
            "values.",
        },
        {
            "Mod",
            50, // "Modulation On/Off"
            // "Modulation Setting 1-7", plus 2 extra CCs (Lo Cut/Width) Multi Chorus alone uses
            // beyond the officially-named Setting-N list - see EffectDefinitions.cpp.
            { 61, 52, 53, 54, 57, 51, 56, 89, 90 },
            // Order matches the unit's own front-panel effect list exactly (confirmed
            // 2026-07-24): C1 Chor/Vib, Multi Chorus, Flanger, Vibe Phaser, Orange Phaser,
            // Roto Speaker.
            { 11, 39, 40, 88, 89, 90, 69, 70, 35, 46, 34, 71, 75, 76, 77 },
            11, // C1 Chor/Vib
            "All 6 Mod-slot effects the unit itself offers are listed here, in the unit's own "
            "on-screen order. CC data for Chorus/Vibrato, Vibe Phaser, Flanger, Multi Chorus, "
            "and Roto Speaker is sourced directly from the official Eleven Rack User Guide "
            "(Chapter 9); Orange Phaser's Rate knob is hardware-confirmed (see the Distortion "
            "slot for the positional mapping), but its Sync was recently corrected from a "
            "toggle to a tempo-sync list, same as the other Sync controls here - not yet "
            "re-tested. Roto Speaker's Type option list/order is hardware-confirmed, but the "
            "specific CC value per option is only a range midpoint guess, not independently "
            "verified (see EffectDefinitions.cpp). Confirmed 2026-07-24: Chorus/Vibrato's "
            "knobs, Sync, Bypass, and Mode toggle all work correctly. The rest of this slot is "
            "not yet hardware-tested. No live readback for per-knob values.",
        },
        {
            "Reverb",
            36, // "Reverb On/Off"
            { 18, 38, 40, 39, 76, 41 }, // "Reverb Setting 1-6"
            { 37, 47, 51, 52, 53 }, // Blackpanel Spring Reverb (37,47), Eleven SR (51,52,53)
            37, // Blackpanel Spring Reverb
            "Real parameters sourced from the official Eleven Rack User Guide (Chapter 9) - "
            "Mix/Decay/Tone for Blackpanel Spring Reverb, plus Pre-Delay and a 25-option Type "
            "list (CC 76) for Eleven SR. Type's option list/order is hardware-confirmed, but "
            "the specific CC value per option is only a range midpoint guess, not "
            "independently verified (see EffectDefinitions.cpp). Confirmed 2026-07-24: the "
            "unit only has these 2 real Reverb models (a dropdown dedup bug briefly made it "
            "look like 5). No live readback for per-knob values.",
        },
        {
            "Delay",
            28, // "Delay On/Off"
            { 62, 33, 35, 85, 87, 34, 48, 49, 55, 59, 72, 73 }, // "Delay Setting 1-12"
            { 27, 48, 80, 81, 82, 28, 49 }, // BBD Delay, Dyn Delay, Tape Echo
            27, // BBD Delay
            "Real parameters sourced from the official Eleven Rack User Guide (Chapter 9 CC "
            "table, cross-checked against Chapter 3's plain-English descriptions to get each "
            "param's real type right - e.g. BBD Delay's \"Mod\" and Tape Echo's \"Hiss\" are "
            "switches, not knobs). Dyn Delay's Mode selector (Mono/Stereo/Cross/Pong) is "
            "hardware-confirmed at 4 tested points (0/42/85/127) - the exact boundaries "
            "between them aren't independently verified. All three delay effects have a "
            "\"Fine\" on/off control with no known CC anywhere - not included; still "
            "unresolved (see EffectDefinitions.cpp). Confirmed 2026-07-24: Tape Echo's other "
            "controls (including the corrected Expanded Delay switch) all work correctly. No "
            "live readback for per-knob values.",
        },
        {
            "FX1",
            63, // "FX1 On/Off"
            { 20, 42, 60, 77, 116, 117, 118, 119, 5, 9, 12, 26, 29, 30 }, // "FX1 Setting 1-14"
            // Confirmed on real hardware (2026-07-24): FX1/FX2 can hold these effects, plus a
            // few already mapped elsewhere. Order matches the on-unit list the user confirmed:
            // C1 Chor/Vib, Multi Chorus, Flanger, Vibe Phaser, Orange Phaser, Roto Speaker,
            // Graphic EQ, Para EQ, Gray Comp, Dyn3 Comp.
            { 11, 39, 40, 88, 89, 90, 69, 70, 35, 46, 34, 71, 75, 76, 77, 33, 50, 78, 79, 32, 85, 86 },
            11, // C1 Chor/Vib
            "FX1/FX2 aren't free-form - they only host this fixed effect list (confirmed "
            "2026-07-24), sharing the same `EffectDefinitions` entries the Mod slot and "
            "Graphic EQ already use, just wired to a different CC range here. Gray Compressor, "
            "Dyn3 Compressor, and Para EQ have no separate native slot on the unit at all - "
            "FX1/FX2 is the only place they're controllable, and their real param order was "
            "reconstructed via CC-to-Setting-N lookup, same method as Delay. Para EQ has a "
            "genuine gap at Setting 3 - the \"(unused)\" control does nothing on real hardware, "
            "not a bug. Vibe Phaser's FX1/FX2 CCs are extrapolated, not directly documented in "
            "the manual (unlike Chorus/Vibrato, Flanger, and Orange Phaser, whose FX1/FX2 order "
            "exactly matches their Mod-slot order - confirmed on real hardware the same pattern "
            "holds for Vibe Phaser too, so its existing Mod-slot param order/CCs are reused "
            "as-is here). None of this is hardware-tested yet. No live readback for per-knob "
            "values.",
        },
        {
            "FX2",
            86, // "FX2 On/Off"
            { 113, 114, 115, 96, 97, 98, 99, 37, 46, 47, 58, 109, 110, 70 }, // "FX2 Setting 1-14"
            { 11, 39, 40, 88, 89, 90, 69, 70, 35, 46, 34, 71, 75, 76, 77, 33, 50, 78, 79, 32, 85, 86 },
            11, // C1 Chor/Vib
            "Same effect list and real param data as the FX1 slot - see its note for the full "
            "explanation (Gray Compressor/Dyn3 Compressor/Para EQ are FX1/FX2-only; Para EQ's "
            "\"(unused)\" control does nothing on real hardware; Vibe Phaser's CCs here are "
            "extrapolated from its Mod-slot data, not directly documented). None of this is "
            "hardware-tested yet. No live readback for per-knob values.",
        },
    };
    return configs;
}

const SlotConfig* findSlotByName (const juce::String& name)
{
    for (const auto& slot : slotConfigs())
        if (slot.name == name)
            return &slot;
    return nullptr;
}

namespace
{
    // effectName -> (ParamDefinition::key -> Bulk Rig raw field tag), confirmed via cross-referencing
    // 3 real decoded rigs (our own "JCM 800"/"SLO 100" captures, plus ElevenHack's "Metal Rythm 1"
    // reference fixture) against EffectDefinitions.cpp - see docs/protocol-spec.md for the full
    // cross-reference, including the near-miss pairs (e.g. "PreD"/"PDly") deliberately NOT included
    // here because they're a plausible name-similarity guess, not a confirmed exact match.
    const std::map<juce::String, std::map<juce::String, juce::String>>& confirmedTagTable()
    {
        static const std::map<juce::String, std::map<juce::String, juce::String>> table = {
            { "Sine Wah", { { "Filt", "Filt" }, { "VxCr", "VxCr" } } },
            { "Black Wah", { { "Filt", "Filt" }, { "VxCr", "VxCr" } } },
            // All 5 Distortion models share ElevenHack's one internal Distortion field set - only
            // DC_Disto was directly decoded, but the other 4 use the identical key strings for the
            // knobs they each expose, so the same table applies by strong analogy, not guesswork.
            { "Tri Knob Disto", { { "Driv", "Driv" }, { "Tone", "Tone" }, { "Levl", "Levl" } } },
            { "Black Op Disto", { { "Driv", "Driv" }, { "Tone", "Tone" }, { "Levl", "Levl" } } },
            { "Green JRC Disto", { { "Driv", "Driv" }, { "Tone", "Tone" }, { "Levl", "Levl" } } },
            { "White Boost Disto", { { "Driv", "Driv" }, { "Treb", "Treb" }, { "Bass", "Bass" }, { "Levl", "Levl" } } },
            { "DC_Disto", { { "Driv", "Driv" }, { "Treb", "Treb" }, { "Bass", "Bass" }, { "Levl", "Levl" } } },
            // Flanger's "PreD"/"Rate" CC keys do NOT match the bulk tags "PDly"/"Sped" - excluded.
            { "Flanger", { { "Dpth", "Dpth" }, { "Fdbk", "Fdbk" }, { "Sync", "Sync" } } },
            // Multi Chorus's "PreD"/"TriS"/"Widt" CC keys do NOT match the bulk tags
            // "PDly"/"Wave"/"Wdth" - excluded, even though semantically plausible.
            { "Multi Chorus", { { "Dpth", "Dpth" }, { "Mix ", "Mix " }, { "Rate", "Rate" },
                                 { "Sync", "Sync" }, { "Voic", "Voic" }, { "LoCt", "LoCt" } } },
            // Tape Echo's "Dely"/"Fdbk"/"Mix "/"RecL"/"Head"/"ExpD" CC keys do NOT match the bulk
            // tags "EDly"/"Sust"/"Vol "/"Rec "/"Tilt"/"4X  " - excluded. "Hiss" is the one toggle
            // this reconciles with real confidence (see docs/implementation-plan.md Round 1).
            { "Tape Echo", { { "Sync", "Sync" }, { "Hiss", "Hiss" }, { "Wow ", "Wow " } } },
            // Eleven SR's selectors - not used for toggle-wiring (no toggles in this effect), kept
            // for when selector reconciliation is tackled.
            { "Eleven SR", { { "Tone", "Tone" }, { "Type", "Type" } } },
        };
        return table;
    }

    // Plausible name-similarity/positional matches against the same 3 real decoded rigs, for CC
    // keys confirmedTagTable() deliberately excludes because the raw tag string differs - see
    // docs/protocol-spec.md "twenty-third"/"twenty-fifth" rounds for how each was derived. Tape
    // Echo's remaining unmatched keys (Dely/Fdbk/Mix /RecL/Head/ExpD) have no recorded
    // correspondence at all - not even a name-similarity guess - so they're absent here too, not
    // fabricated.
    const std::map<juce::String, std::map<juce::String, juce::String>>& extrapolatedTagTable()
    {
        static const std::map<juce::String, std::map<juce::String, juce::String>> table = {
            { "Flanger", { { "PreD", "PDly" }, { "Rate", "Sped" } } },
            { "Multi Chorus", { { "PreD", "PDly" }, { "TriS", "Wave" }, { "Widt", "Wdth" } } },
        };
        return table;
    }
}

std::optional<juce::String> confirmedRawTagForKey (const juce::String& effectName, const juce::String& ccKey)
{
    const auto& table = confirmedTagTable();
    auto effectIt = table.find (effectName);
    if (effectIt == table.end())
        return std::nullopt;

    auto keyIt = effectIt->second.find (ccKey);
    if (keyIt == effectIt->second.end())
        return std::nullopt;

    return keyIt->second;
}

std::optional<juce::String> bestEffortRawTagForKey (const juce::String& effectName, const juce::String& ccKey)
{
    if (auto confirmed = confirmedRawTagForKey (effectName, ccKey))
        return confirmed;

    const auto& table = extrapolatedTagTable();
    auto effectIt = table.find (effectName);
    if (effectIt == table.end())
        return std::nullopt;

    auto keyIt = effectIt->second.find (ccKey);
    if (keyIt == effectIt->second.end())
        return std::nullopt;

    return keyIt->second;
}

int knobRawToCcValue (int32_t raw)
{
    constexpr double span = 4294967295.0; // INT32_MAX - INT32_MIN, i.e. UINT32_MAX
    double normalised = (static_cast<double> (raw) - static_cast<double> (std::numeric_limits<int32_t>::min())) / span;
    return juce::roundToInt (normalised * 127.0);
}
