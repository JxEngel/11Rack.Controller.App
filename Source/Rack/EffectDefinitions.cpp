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

        // A tempo-sync selector as controlled via live MIDI CC - NOT the same scale as the small
        // 0-13 index ElevenHack's own bulk-SysEx field uses for this same kind of field (that
        // index is for the rig-file/bulk-transfer wire format, a completely different transport).
        // Real hardware buckets the live CC byte (0-127) into ranges - this is the official Eleven
        // Rack User Guide, Chapter 9 "FX Sync Setting Values" table (14 named values over 14
        // contiguous 0-127 ranges), which going by its position/generic naming in the manual is
        // evidently a shared table for every CC-controlled "Sync" parameter, not specific to the
        // one section (FX Loop) it happened to be printed under. Each option's value here is the
        // midpoint of its real range, so sending it lands solidly inside the bucket, not on an
        // edge. Sending the raw 0-13 index instead (as this code did before 2026-07-24) mostly
        // lands in/near the "Off" bucket regardless of which option was picked - the likely cause
        // of a real hardware report that Chorus/Vibrato's Sync control "doesn't work."
        ParamDefinition ccSyncSelector (std::string key, std::string label)
        {
            return selector (std::move (key), std::move (label), {
                { 2, "Off" }, { 9, "Whole Note" }, { 19, "Dotted Half Note" }, { 29, "Half Note" },
                { 39, "Half Note Triplet" }, { 49, "Dotted Quarter Note" }, { 59, "Quarter Note" },
                { 68, "Quarter Note Triplet" }, { 78, "Dotted Eighth Note" }, { 88, "Eighth Note" },
                { 98, "Eighth Note Triplet" }, { 108, "Dotted Sixteenth Note" },
                { 118, "Sixteenth Note" }, { 125, "Sixteenth Note Triplet" },
            });
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
            // Tri Knob Disto's param order/labels below match the official Eleven Rack User
            // Guide, Chapter 9 ("Tri-Knob Fuzz": Bypass=CC25, Volume=CC27/Setting1,
            // Sustain=CC78/Setting2, Tone=CC79/Setting3) rather than ElevenHack's original
            // Sustain/Tone/Level order - the same category of ordering bug as Chorus/Vibrato,
            // caught here from the manual before ever reaching a hardware test of this model.
            addGroup (all, { 29 }, "Tri Knob Disto", {
                bypass(), knob ("Levl", "Volume"), knob ("Driv", "Sustain"), knob ("Tone", "Tone"),
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
            // Param order here is deliberately NOT ElevenHack's own field-declaration order (which
            // puts the Mode toggle first) - it's reordered to match the real MIDI CC "Setting N"
            // assignment confirmed in the official Eleven Rack User Guide, Chapter 9 ("as MOD":
            // Chorus=CC61/Setting1, Rate=CC52/Setting2, Sync=CC53/Setting3, Depth=CC54/Setting4,
            // the Chorus/Vibrato mode toggle=CC57/Setting5 - toggle LAST, not first). This order is
            // what `EffectEditorComponent`'s positional Setting-N mapping depends on; the original
            // ElevenHack-field order silently produced a wrong mapping (Mode toggle sent on Setting
            // 1 instead of Setting 5) until this fix, caught before any hardware test of this slot.
            addGroup (all, { 11, 39, 40 }, "Chorus/Vibrato", {
                bypass(), knob ("ChIn", "Chorus"), knob ("VbRt", "Vibrato Rate"),
                ccSyncSelector ("Sync", "Sync"),
                knob ("VbDp", "Vibrato Depth"), toggle ("Mode", "Chorus/Vibrato"),
            });

            // --- Orange Phaser (2 variants, identical parameters) ---
            // Sync was originally modeled as a plain on/off toggle - **confirmed wrong on real
            // hardware (2026-07-24)**: it's a tempo-sync note-value list, not a toggle. The
            // official manual's own description settles it: "Sync Synchronizes the modulation
            // rate to the Rig tempo by a specific rhythmic subdivision" - the same kind of control
            // as Chorus/Vibrato's Sync, so modeled with the same shared `ccSyncSelector` table.
            addGroup (all, { 34, 71 }, "Orange Phaser", {
                bypass(), knob ("Sped", "Rate"), ccSyncSelector ("Sync", "Sync"),
            });

            // --- Vibe Phaser (2 variants, identical parameters) ---
            // Previously name-only (ElevenHack itself never decoded real params for this one) -
            // promoted to fully known using the official Eleven Rack User Guide, Chapter 9 ("as
            // MOD": Volume=CC61/Setting1, Depth=CC52/Setting2, Rate=CC53/Setting3,
            // Sync=CC54/Setting4, Chorus/Vibrato mode toggle=CC57/Setting5). Sync modeled as the
            // same tempo-sync selector as Chorus/Vibrato's own Sync (see `ccSyncSelector` above) -
            // both are named "Sync" in the same Modulation CC context, so almost certainly the
            // same value table. Not yet hardware-tested.
            addGroup (all, { 35, 46 }, "Vibe Phaser", {
                bypass(), knob ("Volm", "Volume"), knob ("Dpth", "Depth"), knob ("Rate", "Rate"),
                ccSyncSelector ("Sync", "Sync"), toggle ("Mode", "Chorus/Vibrato"),
            });

            // --- Flanger (2 variants, identical parameters) ---
            // Previously name-only - promoted to fully known using the official Eleven Rack User
            // Guide, Chapter 9 (as MOD: Bypass=50, Pre-Delay=61/Setting1, Depth=52/Setting2,
            // Rate=53/Setting3, Sync=54/Setting4, Feedback=57/Setting5). Key names below are
            // synthetic (ElevenHack never assigned real ones for this name-only effect), unlike
            // keys ported from ElevenHack's own source elsewhere in this file. Sync modeled as the
            // same tempo-sync selector as Chorus/Vibrato/Vibe Phaser. Not yet hardware-tested.
            addGroup (all, { 69, 70 }, "Flanger", {
                bypass(), knob ("PreD", "Pre-Delay"), knob ("Dpth", "Depth"), knob ("Rate", "Rate"),
                ccSyncSelector ("Sync", "Sync"), knob ("Fdbk", "Feedback"),
            });

            // --- Multi Chorus ---
            // Previously name-only - promoted to fully known using the official Eleven Rack User
            // Guide, Chapter 9. Its params map onto the Mod slot's shared CCs differently than
            // Chorus/Vibrato's do - expected, since each effect model assigns its own knobs to the
            // shared "Setting N" CC numbers independently (as MOD: Bypass=50, Rate=61/Setting1,
            // Sync=52/Setting2, Depth=53/Setting3, Pre-Delay=54/Setting4, Mix=57/Setting5,
            // Tri/Sine=51/Setting6, Voices=56/Setting7). Also uses two CCs (89, 90) outside the
            // officially-named "Modulation Setting 1-7" list - the Mod slot's `settingCc` array in
            // EffectEditorComponent.cpp was extended to 9 entries to reach them.
            //
            // Width/Lo Cut order corrected (2026-07-24) while reconciling this effect's FX1/FX2
            // CC data (see EffectEditorComponent.cpp's new FX1/FX2 slots): the manual's own
            // *named* param order lists "Lo Cut" before "Width", but the FX1 CC list (a
            // well-established, independently-cross-checked table) shows Width landing on the
            // lower-numbered Setting slot (Setting8) and Lo Cut on the higher one (Setting9) - the
            // opposite of print order, same "named order != true positional order" pattern already
            // seen with Delay. Corrected here on the assumption the two extra CCs (89, 90) follow
            // the same true relative order regardless of which slot (Mod/FX1/FX2) this effect is
            // placed in - not independently hardware-confirmed for the Mod slot specifically.
            // Synthetic key names (ElevenHack never assigned real ones). Not yet hardware-tested.
            addGroup (all, { 88, 89, 90 }, "Multi Chorus", {
                bypass(), knob ("Rate", "Rate"), ccSyncSelector ("Sync", "Sync"),
                knob ("Dpth", "Depth"), knob ("PreD", "Pre-Delay"), knob ("Mix ", "Mix"),
                toggle ("TriS", "Tri/Sine"), knob ("Voic", "Voices"), knob ("Widt", "Width"),
                knob ("LoCt", "Lo Cut"),
            });

            // --- Roto Speaker ---
            // Previously name-only - promoted to fully known using the official Eleven Rack User
            // Guide, Chapter 9 (as MOD: Bypass=50, Speed=61/Setting1, Balance=52/Setting2,
            // Type=53/Setting3). Confirmed on real hardware (2026-07-24) that Type is a
            // list/selector control, not a knob.
            //
            // Speed is a 3-way range selector (Slow/Brake/Fast over 0-31/32-95/96-127). Type's raw
            // transcription had 9 name tokens ("120 122 21H Foam Drum Rover Memphis Wolf Watery")
            // against only 8 CC ranges (0-9,10-27,28-45,46-63,64-82,83-100,101-118,119-127) - a
            // first attempt (wrongly) assumed "120"/"122" were one merged option; **confirmed on
            // real hardware they're two distinct options**, and the actual real list, read
            // directly off the unit, is 8 options: 120, 122, 21H, "Foam Dr" (a single, likely
            // truncated option name - "Foam" and "Drum" were mis-split into two tokens during PDF
            // extraction, not "120"/"122"), Rover, Memphis, Wolf, Watery. Each option's CC value
            // below is its range's midpoint, in that order - hardware-confirmed option list and
            // order, but the specific CC value chosen per range is still just the midpoint, not
            // independently verified.
            addGroup (all, { 75, 76, 77 }, "Roto Speaker", {
                bypass(),
                selector ("Sped", "Speed", { { 15, "Slow" }, { 63, "Brake" }, { 111, "Fast" } }),
                knob ("Bal ", "Balance"),
                selector ("Type", "Type", {
                    { 4, "120" }, { 18, "122" }, { 36, "21H" }, { 54, "Foam Dr" },
                    { 73, "Rover" }, { 91, "Memphis" }, { 109, "Wolf" }, { 123, "Watery" },
                }),
            });

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

            // --- Reverb (2 variants, different real parameter sets) ---
            // Previously name-only (ElevenHack itself never decoded real params for either) -
            // promoted to fully known using the official Eleven Rack User Guide, Chapter 9
            // ("Blackpanel Spring Reverb": Bypass=36, Mix=18/Setting1, Decay=38/Setting2,
            // Tone=40/Setting3; "Eleven SR" adds Pre-Delay=39/Setting4).
            //
            // Real hardware only exposes 2 selectable Reverb models (confirmed 2026-07-24) -
            // named "Blackpanel Spring Reverb" and "Eleven SR" on the unit itself, matching these
            // two groups exactly. The 2-and-3-sibling-ID groups below (37/47, 51/52/53) are NOT 3
            // distinct "Stereo Reverb" variants - every one of Eleven SR's 3 IDs was confirmed on
            // hardware to be the same single reverb model. Consistent with the general pattern
            // elsewhere in this file (e.g. Volume Pedal's 38/72, Chorus/Vibrato's 11/39/40) where
            // one real effect gets multiple ElevenHack effectIds - most likely one ID per rack
            // slot it can be placed into (its native slot, FX1, FX2 - see the "(as FX1)"/"(as
            // FX2)" alternate CC sets the manual lists for many effects), not per-model variation.
            // Names below match the unit's own on-screen labels exactly, not ElevenHack's internal
            // field name (which had a stray underscore, "Spring_Reverb").
            addGroup (all, { 37, 47 }, "Blackpanel Spring Reverb", {
                bypass(), knob ("Mix ", "Mix"), knob ("Decy", "Decay"), knob ("Tone", "Tone"),
            });
            // Eleven SR also has a "Type" selector (CC 76/Setting5, 25 named reverb types over
            // uneven CC-value ranges). The manual's raw transcription ran the option names
            // together with no delimiters and came out to 26 tokens against 25 CC ranges -
            // resolved with the user's help (2026-07-24) reading the real on-unit list: the first
            // two tokens, "Echo"/"Room", are one option, "Echo Room" (same kind of mis-split as
            // Roto Speaker's "Foam"/"Drum" -> "Foam Dr"). Each option's CC value below is its
            // range's midpoint - option list/order is hardware-confirmed, but the exact CC value
            // per option is still just a range midpoint, not independently verified.
            addGroup (all, { 51, 52, 53 }, "Eleven SR", {
                bypass(), knob ("Mix ", "Mix"), knob ("Decy", "Decay"), knob ("Tone", "Tone"),
                knob ("PreD", "Pre-Delay"),
                selector ("Type", "Type", {
                    { 1, "Echo Room" }, { 5, "Studio" }, { 10, "Small Room" }, { 16, "Jazz Club" },
                    { 21, "Small Club" }, { 26, "Garage" }, { 32, "Medium Room" },
                    { 37, "Tiled Room" }, { 42, "Wood Room" }, { 48, "Small Theater" },
                    { 53, "Medium Theater" }, { 58, "Large Theater" }, { 64, "Rich Hall" },
                    { 69, "Concert Hall" }, { 74, "Bright Hall" }, { 80, "Church" },
                    { 85, "Cathedral" }, { 90, "Arena" }, { 96, "Small Plate" },
                    { 101, "Medium Plate" }, { 106, "Large Plate" }, { 112, "Canyon" },
                    { 117, "Supa Long" }, { 122, "Early Reflect 1" }, { 126, "Early Reflect 2" },
                }),
            });

            // --- FX Loop (rig-level utility, not effect-selectable - see docs/implementation-plan.md) ---
            // Previously name-only - promoted to fully known using the official Eleven Rack User
            // Guide, Chapter 9 (Bypass=107, Send=19, Return=108, Mix=88). Unlike every other slot
            // in this file, FX Loop's CCs are fixed/direct, not a "Setting N" positional scheme -
            // there's only ever one FX Loop, not several selectable models, so it doesn't fit
            // `EffectEditorComponent`'s "pick a model, map its knobs positionally" pattern. Exposed
            // directly in `RigGlobalsComponent` instead, with fixed CCs, same as Tap Tempo. Not yet
            // hardware-tested.
            addGroup (all, { 16, 17, 56, 57, 73, 14, 15 }, "Fx Loop", {
                bypass(), knob ("Send", "Send"), knob ("Retn", "Return"), knob ("Mix ", "Mix"),
            });

            // --- Delay (3 distinct models) ---
            // Previously name-only - promoted to (mostly) fully known. Two sources combined
            // deliberately: the Chapter 9 CC table gives the CC numbers, but its print order
            // does NOT match ascending Setting-N order the way every other effect category does
            // (unlike Amp/Distortion/Mod/Reverb) - the real order below was reconstructed by
            // looking up each named param's CC against the confirmed generic "Delay Setting N"
            // table (Setting1=62, 2=33, 3=35, 4=85, 5=87, 6=34, 7=48, 8=49, 9=55 - see the CC table
            // above). The Chapter 3 "Exploring Rigs" descriptions were also needed to get the
            // right *type* per param - without them, several of these would have been wrongly
            // modeled as knobs: BBD Delay's "Mod" is a Chorus/Vibrato toggle ("Switches the
            // modulation effect between Vibrato...and Chorus"), and both BBD Delay's "Noise" and
            // Tape Echo's "Hiss" are literally described as toggle switches, not knobs. Sync is
            // modeled as the same tempo-sync selector as every other Sync control in this file -
            // Dyn Delay's own description confirms it ("Ranges from OFF...to a variety of
            // rhythmic note values"). None of this is hardware-tested yet.
            addGroup (all, { 27, 48 }, "BBD Delay", {
                bypass(), knob ("Dely", "Delay"), ccSyncSelector ("Sync", "Sync"),
                knob ("Fdbk", "Feedback"), knob ("Mix ", "Mix"), knob ("Inpt", "Input Level"),
                toggle ("Mod ", "Mod (Chorus/Vibrato)"), knob ("Dpth", "Depth"),
                // Confirmed on real hardware (2026-07-24): Expanded Delay is an on/off switch, not
                // a knob (the manual has no Chapter 3 description of it at all, so this was
                // previously unconfirmed and modeled as a knob).
                toggle ("ExpD", "Expanded Delay"), toggle ("Nois", "Noise"),
            });
            addGroup (all, { 28, 49 }, "Tape Echo", {
                bypass(), knob ("Dely", "Delay"), ccSyncSelector ("Sync", "Sync"),
                knob ("Fdbk", "Feedback"), knob ("Mix ", "Mix"), knob ("RecL", "Rec Level"),
                knob ("Head", "Head"), knob ("Wow ", "Wow"),
                toggle ("ExpD", "Expanded Delay"), // confirmed on hardware, see BBD Delay's comment
                toggle ("Hiss", "Hiss"),
            });
            // Now fully modeled (2026-07-24) - Mode's CC values were confirmed on real hardware
            // (0=Mono, 42=Stereo, 85=Cross, 127=Pong, in the same order the manual lists them),
            // unblocking the rest of this effect's real, Chapter-3-described params (Ratio,
            // Hi-Cut, Lo-Cut, Width, Em Rate, Em Feedback, Em Mix - all plain knobs). The exact
            // range boundaries between the 4 Mode values are still not independently confirmed -
            // only these specific 4 tested points are hardware-verified.
            addGroup (all, { 80, 81, 82 }, "Dyn Delay", {
                bypass(), knob ("Dely", "Delay"), ccSyncSelector ("Sync", "Sync"),
                knob ("Fdbk", "Feedback"), knob ("Mix ", "Mix"),
                selector ("Mode", "Mode", {
                    { 0, "Mono" }, { 42, "Stereo" }, { 85, "Cross" }, { 127, "Pong" },
                }),
                knob ("Rato", "Ratio"), knob ("HiCt", "Hi-Cut"), knob ("LoCt", "Lo-Cut"),
                knob ("Wdth", "Width"), knob ("EmRt", "Em Rate"), knob ("EmFb", "Em Feedback"),
                knob ("EmMx", "Em Mix"),
            });

            // --- FX1/FX2-only effects (compression, EQ) ---
            // Unlike Distortion/Wah/Mod/Reverb/Delay, these have no dedicated native slot on the
            // unit at all - the manual only ever documents them "as FX1"/"as FX2", never as their
            // own standalone CC set. Previously name-only - promoted to fully known using the
            // official Eleven Rack User Guide, Chapter 9, with real order reconstructed via
            // CC-to-Setting-N lookup (same method as Delay) rather than trusting print order.
            // Not yet hardware-tested. See EffectEditorComponent.cpp's new FX1/FX2 slots.
            addGroup (all, { 32 }, "Gray Compressor", {
                bypass(), knob ("Sust", "Sustain"), knob ("Levl", "Level"),
            });
            // Renamed from ElevenHack's internal "Dyn Compressor" to match the unit's own on-screen
            // name, "Dyn3 Compressor" (same reasoning as the Reverb renames earlier).
            addGroup (all, { 85, 86 }, "Dyn3 Compressor", {
                bypass(), knob ("Thrs", "Threshold"), knob ("Atck", "Attack"),
                knob ("Rels", "Release"), knob ("Gain", "Gain"), knob ("Rato", "Ratio"),
                knob ("Knee", "Knee"),
            });
            // Setting3 is genuinely unused by Para EQ (13 real params fill Settings 1-2 and 4-14,
            // leaving a gap) - modeled as an explicit placeholder rather than skipped outright,
            // since the positional CC mechanism can't skip a slot without misaligning everything
            // after it. Moving this control does nothing on real hardware.
            addGroup (all, { 78, 79 }, "Para EQ", {
                bypass(), knob ("LGan", "L Gain"), knob ("LMGn", "LM Gain"),
                knob ("Unus", "(unused)"),
                knob ("HMGn", "HM Gain"), knob ("HGan", "H Gain"), knob ("Out ", "Output"),
                knob ("LFrq", "L Freq"), knob ("LMFq", "LM Freq"), knob ("LQ  ", "L Q"),
                knob ("LMQ ", "LM Q"), knob ("HMFq", "HM Freq"), knob ("HMQ ", "HM Q"),
                knob ("HFrq", "H Freq"), knob ("HQ  ", "H Q"),
            });

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
