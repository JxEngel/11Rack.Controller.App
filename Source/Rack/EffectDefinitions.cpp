#include "EffectDefinitions.h"

namespace Rack::EffectDefinitions
{
    namespace
    {
        ParamDefinition knob (std::string key, std::string label, int minValue = 0, int maxValue = 127, int step = 1)
        {
            ParamDefinition p;
            p.key = std::move (key);
            p.label = std::move (label);
            p.kind = ParamKind::knob;
            p.minValue = minValue;
            p.maxValue = maxValue;
            p.step = step;
            return p;
        }

        ParamDefinition toggle (std::string key, std::string label)
        {
            ParamDefinition p;
            p.key = std::move (key);
            p.label = std::move (label);
            p.kind = ParamKind::toggle;
            return p;
        }

        ParamDefinition selector (std::string key, std::string label, std::vector<SelectOption> options)
        {
            ParamDefinition p;
            p.key = std::move (key);
            p.label = std::move (label);
            p.kind = ParamKind::selector;
            p.options = std::move (options);
            return p;
        }

        ParamDefinition bypass()
        {
            return toggle ("bypa", "Bypass");
        }

        // Adds one EffectDefinition per id in `ids`, all sharing the same name/params/isFullyKnown
        // - mirrors how ElevenHack's mBuildEffect() has multiple effectIds mapping to one
        // "sibling group" (e.g. two Wah pedal variants with identical parameters).
        void addGroup (std::vector<EffectDefinition>& all,
                        std::initializer_list<int> ids,
                        std::string name,
                        std::vector<ParamDefinition> params,
                        bool isFullyKnown = true)
        {
            for (int id : ids)
            {
                EffectDefinition def;
                def.effectId = id;
                def.name = name;
                def.params = params; // copy per entry
                def.isFullyKnown = isFullyKnown;
                all.push_back (def);
            }
        }

        // Effect types where ElevenHack only identified the name (plus the universal Bypass
        // switch) - the real per-effect parameters were never decoded. See the header comment on
        // `isFullyKnown` before treating these as "no parameters."
        void addNameOnlyGroup (std::vector<EffectDefinition>& all, std::initializer_list<int> ids, std::string name)
        {
            addGroup (all, ids, std::move (name), { bypass() }, /* isFullyKnown */ false);
        }

        std::vector<EffectDefinition> buildAll()
        {
            std::vector<EffectDefinition> all;

            // --- Rig-level globals (effectId -1, ElevenHack's RIG_PARAMS) ---
            {
                EffectDefinition rigParams;
                rigParams.effectId = -1;
                rigParams.name = "Rig Params";
                rigParams.isFullyKnown = true;
                rigParams.params = {
                    bypass(), // added unconditionally by ElevenHack's mBuildEffect() before the
                              // per-effect switch - kept for fidelity even though it's a slightly
                              // odd fit for rig-level globals
                    knob ("RVol", "Volume"),
                    toggle ("RMno", "Mono/Stereo"),
                    knob ("Tmpo", "Tempo", 120000, 6000000, 1),
                    knob ("FXc1", "Fxc1"),
                    knob ("FXc2", "Fxc2"),
                    knob ("FXc3", "Fxc3"),
                    knob ("FXc4", "Fxc4"),
                    knob ("GlSF", "GlSF"),
                    knob ("Msyc", "Msyc"),
                    knob ("RslL", "RslL"),
                    knob ("Vol1", "Vol1"),
                    knob ("Vol2", "Vol2"),
                    selector ("WorB", "Input Selector", {
                        { 60, "Guitar" }, { 61, "Mic" }, { 20, "Line L" }, { 21, "Line R" },
                        { 63, "Line L + R" }, { 22, "Digital L" }, { 23, "Digital R" },
                        { 64, "Digital L + R" }, { 18, "Re-Amp" },
                    }),
                    selector ("WstB", "WstB constant", { { 11, "11" } }),
                    selector ("PIGI", "True Z Selector", {
                        { 125, "Auto" }, { 0, "1 MOhm" }, { 1, "1 MOhm + Cap" },
                        { 2, "230 kOhm" }, { 3, "230 kOhm + Cap" }, { 4, "90 kOhm" },
                        { 5, "90 kOhm + Cap" }, { 6, "70 kOhm" }, { 7, "70 kOhm + Cap" },
                        { 8, "32 kOhm" }, { 9, "32 kOhm + Cap" }, { 10, "22 kOhm" },
                        { 11, "22 kOhm + Cap" },
                    }),
                    selector ("ExpT", "Exp.Pedal Selector", {
                        { 0, "None" }, { 1, "Multiple FX" }, { 2, "Rig Volume" },
                        { 3, "Volume" }, { 4, "Wah" },
                    }),
                };
                all.push_back (rigParams);
            }

            // --- Amp/Cab (effectId 12) ---
            {
                EffectDefinition ampCab;
                ampCab.effectId = 12;
                // ElevenHack's EffectAmpCab.getName() picks the name dynamically from whichever
                // amp model is currently selected (via m_ampMap) rather than having one fixed
                // name - not representable as a single static string here.
                ampCab.name = "Amp/Cab";
                ampCab.isFullyKnown = true;
                ampCab.params = { bypass(), selector ("sld6", "Amp type", ampModelOptions()) };
                all.push_back (ampCab);
            }

            // --- Volume Pedal ---
            addGroup (all, { 38, 72 }, "Volume Pedal", {
                bypass(), knob ("Vol ", "Position"), knob ("Min ", "Min Volume"), toggle ("Tapr", "Linear/Log"),
            });

            // --- Distortion (5 distinct models, each with its own knob labels) ---
            addGroup (all, { 29 }, "Tri Knob Disto", {
                bypass(), knob ("Driv", "Sustain"), knob ("Tone", "Tone"), knob ("Levl", "Level"),
            });
            addGroup (all, { 30 }, "Black Op Disto", {
                bypass(), knob ("Driv", "Distortion"), knob ("Tone", "Cut"), knob ("Levl", "Volume"),
            });
            addGroup (all, { 31 }, "Green JRC Disto", {
                bypass(), knob ("Driv", "Overdrive"), knob ("Tone", "Tone"), knob ("Levl", "Level"),
            });
            addGroup (all, { 87 }, "White Boost Disto", {
                bypass(), knob ("Driv", "Gain"), knob ("Treb", "Treble"), knob ("Bass", "Bass"), knob ("Levl", "Volume"),
            });
            addGroup (all, { 91 }, "DC_Disto", {
                bypass(), knob ("Driv", "Distortion"), knob ("Treb", "Treble"), knob ("Bass", "Bass"), knob ("Levl", "Volume"),
            });

            // --- Wah (both variants share the same parameters) ---
            addGroup (all, { 36 }, "Sine Wah", { bypass(), knob ("Filt", "Position"), knob ("VxCr", "VxCr") });
            // WAH_BLACK = 55 = 0x37 - the one effect ID confirmed to appear in a real captured Rig
            // Description reply (see docs/protocol-spec.md "second round"), giving this specific
            // entry a direct hardware cross-check, not just a source-code transcription.
            addGroup (all, { 55 }, "Black Wah", { bypass(), knob ("Filt", "Position"), knob ("VxCr", "VxCr") });

            // --- Chorus/Vibrato (3 variants, identical parameters) ---
            addGroup (all, { 11, 39, 40 }, "Chorus/Vibrato", {
                bypass(), toggle ("Mode", "Chorus/Vibrato"), knob ("ChIn", "Chorus"),
                knob ("VbDp", "Vibrato Depth"), knob ("VbRt", "Vibrato Rate"),
                selector ("Sync", "Sync", {
                    { 0, "None" }, { 1, "Whole Note" }, { 2, "Whole dot" }, { 3, "Half Note" },
                    { 4, "Half dot" }, { 5, "Quarter Note" }, { 6, "Quarter dot" },
                    { 7, "Eighth Note" }, { 8, "Eighth dot" }, { 9, "Sixteenth Note" },
                    { 10, "Sixteenth dot" }, { 11, "Thirty-second Note" }, { 13, "Thirty-second dot" },
                }),
            });

            // --- Orange Phaser (2 variants, identical parameters) ---
            addGroup (all, { 34, 71 }, "Orange Phaser", { bypass(), knob ("Sped", "Rate"), toggle ("Sync", "Sync") });

            // --- Graphic EQ (2 variants, identical parameters) ---
            // Labels embed ElevenHack's documented real-world ranges (e.g. "-12 to +12" dB) as
            // text for a human reader, but ElevenHack itself never encoded those as the knobs'
            // actual min/max - it left them at the default 0-127 range. Kept faithful to that
            // here rather than guessing real min/max values ourselves.
            addGroup (all, { 33, 50 }, "Graphic EQ", {
                bypass(),
                knob ("LwSh", "100 Hz (-12 to +12)"),
                knob ("LMGn", "370 Hz (-18 to +18)"),
                knob ("MGn ", "800 Hz (-18 to +18)"),
                knob ("HMGn", "2 KHz (-18 to +18)"),
                knob ("HiSh", "3.25 KHz (-12 to +12)"),
                knob ("Out ", "Output (-20 to +6)"),
            });

            // --- Effect types ElevenHack only identified by name (real params not decoded) ---
            addNameOnlyGroup (all, { 16, 17, 56, 57, 73, 14, 15 }, "Fx Loop");
            addNameOnlyGroup (all, { 32 }, "Gray Compressor");
            addNameOnlyGroup (all, { 85, 86 }, "Dyn Compressor");
            addNameOnlyGroup (all, { 78, 79 }, "Para EQ");
            addNameOnlyGroup (all, { 35, 46 }, "Vibe Phaser");
            addNameOnlyGroup (all, { 37, 47 }, "Spring_Reverb");
            addNameOnlyGroup (all, { 51, 52, 53 }, "Stereo Reverb");
            addNameOnlyGroup (all, { 27, 48 }, "BBDDelay");
            addNameOnlyGroup (all, { 28, 49 }, "Tape Delay");
            addNameOnlyGroup (all, { 80, 81, 82 }, "Dyn Delay");
            addNameOnlyGroup (all, { 88, 89, 90 }, "Multi Chorus");
            addNameOnlyGroup (all, { 69, 70 }, "Flanger");
            addNameOnlyGroup (all, { 75, 76, 77 }, "Roto Speaker");

            return all;
        }
    }

    std::string effectClassName (EffectClass effectClass)
    {
        switch (effectClass)
        {
            case EffectClass::ampCab:    return "Amp/Cab";
            case EffectClass::fxLoop:    return "FX Loop";
            case EffectClass::vol:       return "Vol";
            case EffectClass::wah:       return "Wah";
            case EffectClass::mod:       return "Mod";
            case EffectClass::reverb:    return "Reverb";
            case EffectClass::delay:     return "Delay";
            case EffectClass::disto:     return "Disto";
            case EffectClass::fx1:       return "FX1";
            case EffectClass::fx2:       return "FX2";
            case EffectClass::input:     return "Input";
            case EffectClass::type0B:    return "0B";
            case EffectClass::type0C:    return "0C";
            case EffectClass::type0D:    return "0D";
            case EffectClass::type0E:    return "0E";
            case EffectClass::type0F:    return "0F";
            case EffectClass::rigParams: return "Globals";
        }
        return "Unknown";
    }

    std::optional<EffectDefinition> lookup (int effectId)
    {
        static const std::vector<EffectDefinition> all = buildAll();

        for (const auto& def : all)
            if (def.effectId == effectId)
                return def;

        return std::nullopt;
    }

    const std::vector<SelectOption>& ampModelOptions()
    {
        // The 16 known Amp/Cab models, by canonical small index - from ElevenHack's
        // EffectAmpCab.m_ampMap. See the header for the known gap (a second, large-32-bit-value
        // encoding of these same names that isn't modeled here yet).
        static const std::vector<SelectOption> options = {
            { 0, "59 Tweed Lux" },
            { 1, "59 Tweed Bass" },
            { 2, "64 Black Panel Lux Vib" },
            { 3, "64 Black Panel Lux Norm" },
            { 4, "66 AC Hi Boost" },
            { 5, "67 Black Duo" },
            { 6, "69 Plexiglas 100W" },
            { 7, "82 Lead 800 100W" },
            { 8, "85 M-2 Lead" },
            { 9, "89 SL-100 Drive" },
            { 10, "89 SL-100 Crunch" },
            { 11, "89 SL-100 Clean" },
            { 12, "92 Treadplate Modern" },
            { 13, "92 Treadplate Vintage" },
            { 14, "DC Modern Overdrive" },
            { 15, "DC Vintage Crunch" },
        };
        return options;
    }
}
