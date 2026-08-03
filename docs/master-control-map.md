# Eleven Rack Master Control Map

**Purpose**: this is the single, current, self-contained reference for every control this project
has mapped to a MIDI Control Change (CC) number and value/dropdown meaning. If you're starting from
scratch and need to know "what CC does X control, and what values does it take," this file should
answer it without needing to read `EffectDefinitions.cpp`, `EffectEditorComponent.cpp`, or the long
narrative history in `protocol-spec.md`.

**This file is a snapshot, not the source of truth for code.** `Source/Rack/EffectDefinitions.cpp`
and `Source/EffectEditorComponent.cpp` are what the app actually runs; if this file and the code
ever disagree, the code is right and this file is stale - regenerate it from the code, not the
other way around. Built 2026-07-26 from a direct read of both files plus `protocol-spec.md`'s
official CC chart.

**What this file does NOT replace**: `protocol-spec.md`'s hardware-validation log (the full history
of how each mapping was found/corrected, useful if a mapping turns out wrong and you want to know
why it was believed correct), the per-effect research notes in
`docs/samples/eleven-rack-user-guide-chapter9-midi-cc-notes.md` (raw manual extraction, including
data for effects *not yet* wired into the app - Amp tone knobs, per-amp-model knob labels, Delay's
"Fine" leads), and `docs/implementation-plan.md` (project status/milestones). Keep all of those -
they contain reasoning and raw data this file deliberately omits for readability.

## How to read the tables

- **CC** = MIDI Control Change number (0-127), sent as a plain 3-byte message `B0 <CC> <value>` on
  channel 1. This is the *live, real-time* control mechanism - separate from the SysEx bulk-transfer
  protocol used for rig save/load/browse (see `protocol-spec.md`).
- **Type**: `knob` (continuous 0-127 slider), `toggle` (0-63 = Off, 64-127 = On), or `selector`
  (a dropdown - CC value ranges bucket to named options, see each selector's own value list).
- **Setting N**: the *generic* positional CC slot for a given category (e.g. "Distortion Setting
  1"), independent of which specific effect model is currently loaded there. Each effect model
  assigns its own real knobs to these generic slots in its own order - see each per-effect table.
- **Status** column: `HW-confirmed` = independently verified against real hardware end-to-end;
  `manual` = sourced from the official Eleven Rack User Guide, not yet hardware-tested; `guess` =
  a value we chose without direct confirmation (e.g. a range midpoint), flagged explicitly.

---

## 1. Generic "Setting N" CC tables (one per slot category)

These are the fixed CC numbers for each slot. What each "Setting N" *controls* depends entirely on
which effect model is currently loaded there - see Section 3 for the per-effect mapping.

| Slot | Bypass CC | Setting 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Distortion | 25 | 27 | 78 | 79 | 80 | 81 | 82 | 83 | - | - | - | - | - | - | - |
| Wah | 43 | 4 *(only CC - no Setting-N scheme exists for Wah)* | | | | | | | | | | | | | |
| Mod | 50 | 61 | 52 | 53 | 54 | 57 | 51 | 56 | 89† | 90† | - | - | - | - | - |
| Reverb | 36 | 18 | 38 | 40 | 39 | 76 | 41 | - | - | - | - | - | - | - | - |
| Delay | 28 | 62 | 33 | 35 | 85 | 87 | 34 | 48 | 49 | 55 | 59 | 72 | 73 | 74‡ | 31‡ |
| FX1 | 63 | 20 | 42 | 60 | 77 | 116 | 117 | 118 | 119 | 5 | 9 | 12 | 26 | 29 | 30 |
| FX2 | 86 | 113 | 114 | 115 | 96 | 97 | 98 | 99 | 37 | 46 | 47 | 58 | 109 | 110 | 70 |
| Amp *(not yet wired into the app - see §5)* | 111 | 13 | 14 | 15 | 16 | 21 | 10 | 112 | 3 | 84 | 24 | 23 | 22 | 44 | 45 |

†Mod's Setting 8/9 (CC 89, 90) aren't part of the officially-named "Modulation Setting 1-7" list -
they're extra CCs only Multi Chorus uses, appended positionally in code.
‡Delay's Setting 13/14 (CC 74, 31) are confirmed CC numbers from the official chart but no
currently-modeled effect uses them (BBD Delay/Tape Echo use Settings 1-9, Dyn Delay uses 1-12).

**On the CC 119 / "FX1 Setting 9" mixup**: an older manual transcription listed CC 119 as a
duplicate of CC 5 ("FX1 Setting 9"). Confirmed (2026-07-24) that's wrong - CC 119 is a real,
distinct CC (FX1 Setting 8, per this table), used simultaneously with CC 5 by Multi Chorus. See
`protocol-spec.md`'s "second manual revision found" section for the full story.

---

## 2. Rig-level globals (fixed CCs, not part of any Setting-N slot)

These aren't effect-selectable - there's exactly one of each on the whole unit.

| Control | CC | Type | Values | Status | Notes |
|---|---|---|---|---|---|
| Tuner On/Off | 69 | toggle | 0-63=Off, 64-127=On | HW-confirmed | Also settable via SysEx `CMD_TUNER` (`RackController::setTunerOn`) - both mechanisms work independently. No query either way. |
| Tap Tempo | 64 | momentary | 64-127 = a tap | manual | `RigGlobalsComponent`'s "Tap" button sends 127 on click. Not a persistent value. |
| FX Loop Bypass | 107 | toggle | 0-63=Off, 64-127=On | manual | Implemented directly in `RigGlobalsComponent` via its own hardcoded constants, not through `EffectDefinitions`'s "Fx Loop" entry (which models the same Bypass/Send/Return/Mix params but isn't referenced by any UI code - a redundant, currently-unused model). |
| FX Loop Send | 19 | knob | 0-127 | manual | |
| FX Loop Return | 108 | knob | 0-127 | manual | |
| FX Loop Mix | 88 | knob | 0-127 | manual | |
| Volume Pedal Bypass | 75 | toggle | 0-63=Off, 64-127=On | manual | Not yet exposed in any UI component - see §5. |
| Volume Pedal Position | 7 | knob | 0-127 | manual | Not yet exposed in any UI component - see §5. |
| Multiple FX Control (pedal position) | 11 | knob | 0-127 | manual | Not yet exposed in any UI component. |
| Rig Volume (pedal position) | 17 | knob | 0-127 | manual | Not yet exposed in any UI component. Distinct from Main Volume (below). |
| User/Factory Bank Change | 32 | toggle | 0 = User Rigs, 1 = Factory Rigs | manual | A plain 2-value toggle, **not** a general MIDI Bank Select MSB/LSB pair, despite CC 32 also being the standard Bank Select LSB controller number. |
| Amp Bypass | 111 | toggle | 0-63=Off, 64-127=On | manual | Universal across all amp models. |
| Amp Output | 92 | knob | 0-127 | manual | Universal across all amp models. |
| Cab/Mic Bypass | 71 | toggle | 0-63=Off, 64-127=On | manual | Also called "Cab Sim On/Off" in the original manual edition's generic CC table - same CC, naming variant. |
| Knob 1 | 101 | knob | 0-127 | manual | Front-panel physical knob, not effect-specific. Not yet exposed in any UI component. |
| Knob 2 | 102 | knob | 0-127 | manual | Same as above. |
| Knob 3 | 103 | knob | 0-127 | manual | Same as above. |
| Knob 4 | 104 | knob | 0-127 | manual | Same as above. |
| Knob 5 | 105 | knob | 0-127 | manual | Same as above. |
| Knob 6 | 106 | knob | 0-127 | manual | Same as above. |

**Main Volume is NOT a plain CC** - it uses a separate SysEx command (`CMD_MAIN_VOLUME`, a 5-byte
signed-value encoding via `SevenBitCodec::encodeValue`/`RackController::setMainVolume(int8_t)`).
Confirmed range: raw `-127` = unit display `0.0` (min), raw `0` = display `5.0` (center), raw `127`
= display `10.0` (max). `RigGlobalsComponent`'s slider shows the 0.0-10.0 display scale directly.

**The shared tempo-sync selector** (`ccSyncSelector` in code) - used by every "Sync" parameter
across Mod and Delay effects (see §3) - is not itself a Setting-N slot, but its value table is
universal enough to list once here:

| CC value | Option |
|---|---|
| 2 | Off |
| 9 | Whole Note |
| 19 | Dotted Half Note |
| 29 | Half Note |
| 39 | Half Note Triplet |
| 49 | Dotted Quarter Note |
| 59 | Quarter Note |
| 68 | Quarter Note Triplet |
| 78 | Dotted Eighth Note |
| 88 | Eighth Note |
| 98 | Eighth Note Triplet |
| 108 | Dotted Sixteenth Note |
| 118 | Sixteenth Note |
| 125 | Sixteenth Note Triplet |

*(Each value is the midpoint of a 14-way bucket spanning 0-127; sending any value inside a bucket's
true range should select the same option, but only these exact midpoints are used in code.)*

---

## 3. Per-effect parameter mappings, by slot

Each effect's params are listed in **positional order** - the Nth param maps to the slot's Setting
N CC from §1. `Status` marks the whole effect's hardware-test state; per-param caveats are noted
inline where they exist.

### Distortion (Bypass = CC 25)

| Effect | ID(s) | Status |
|---|---|---|
| Tri Knob Disto | 29 | manual |
| Black Op Disto | 30 | manual |
| Green JRC Disto | 31 | **HW-confirmed** (2026-07-24) |
| White Boost Disto | 87 | manual |
| DC_Disto | 91 | manual |

| Effect | Setting 1 (CC 27) | 2 (78) | 3 (79) | 4 (80) |
|---|---|---|---|---|
| Tri Knob Disto | Volume (knob) | Sustain (knob) | Tone (knob) | - |
| Black Op Disto | Distortion (knob) | Cut (knob) | Volume (knob) | - |
| Green JRC Disto | Overdrive (knob) | Tone (knob) | Level (knob) | - |
| White Boost Disto | Gain (knob) | Treble (knob) | Bass (knob) | Volume (knob) |
| DC_Disto | Distortion (knob) | Treble (knob) | Bass (knob) | Volume (knob) |

### Wah (Bypass = CC 43, single direct CC — no Setting-N scheme)

| Effect | ID(s) | Position (CC 4) | VxCr | Status |
|---|---|---|---|---|
| Sine Wah | 36 | knob, 0-127 | **no known CC** - not exposed | manual |
| Black Wah | 55 | knob, 0-127 | **no known CC** - not exposed | manual (id confirmed via a real captured Rig Description reply) |

### Mod (Bypass = CC 50)

| Effect | ID(s) | Status |
|---|---|---|
| Chorus/Vibrato (C1 Chor/Vib) | 11, 39, 40 | **HW-confirmed** (2026-07-24) |
| Orange Phaser | 34, 71 | manual (Rate knob HW-confirmed via Distortion's positional test; Sync not re-tested since its toggle→selector fix) |
| Vibe Phaser | 35, 46 | manual |
| Flanger | 69, 70 | manual |
| Multi Chorus | 88, 89, 90 | manual |
| Roto Speaker | 75, 76, 77 | manual (option list/order HW-confirmed; per-option CC value is a **guess**, see §4) |

| Effect | S1 (61) | S2 (52) | S3 (53) | S4 (54) | S5 (57) | S6 (51) | S7 (56) | S8 (89) | S9 (90) |
|---|---|---|---|---|---|---|---|---|---|
| Chorus/Vibrato | Chorus (knob) | Vibrato Rate (knob) | Sync (selector) | Vibrato Depth (knob) | Mode (toggle: 0-63=Chorus, 64-127=Vibrato) | - | - | - | - |
| Orange Phaser | Rate (knob) | Sync (selector) | - | - | - | - | - | - | - |
| Vibe Phaser | Volume (knob) | Depth (knob) | Rate (knob) | Sync (selector) | Mode (toggle: 0-63=Chorus, 64-127=Vibrato) | - | - | - | - |
| Flanger | Pre-Delay (knob) | Depth (knob) | Rate (knob) | Sync (selector) | Feedback (knob) | - | - | - | - |
| Multi Chorus | Rate (knob) | Sync (selector) | Depth (knob) | Pre-Delay (knob) | Mix (knob) | Tri/Sine (toggle) | Voices (knob) | Width (knob) | Lo Cut (knob) |
| Roto Speaker | Speed (selector, see below) | Balance (knob) | Type (selector, see below) | - | - | - | - | - | - |

Roto Speaker's selectors:
- **Speed**: 15 = Slow, 63 = Brake, 111 = Fast (3-way, uneven ranges 0-31/32-95/96-127 per the manual)
- **Type**: 4 = "120", 18 = "122", 36 = "21H", 54 = "Foam Dr", 73 = "Rover", 91 = "Memphis", 109 = "Wolf", 123 = "Watery" (option list/order HW-confirmed 2026-07-24; each CC value is the midpoint of its range, not independently verified per-option)

### Reverb (Bypass = CC 36)

| Effect | ID(s) | Status |
|---|---|---|
| Blackpanel Spring Reverb | 37, 47 | manual |
| Eleven SR (Stereo Reverb) | 51, 52, 53 | manual (Type option list/order HW-confirmed 2026-07-24) |

**Confirmed on real hardware (2026-07-24)**: the unit only has these 2 real Reverb models - all 3
of Eleven SR's sibling IDs are the same single effect, not 3 variants.

| Effect | S1 (18) | S2 (38) | S3 (40) | S4 (39) | S5 (76) |
|---|---|---|---|---|---|
| Blackpanel Spring Reverb | Mix (knob) | Decay (knob) | Tone (knob) | - | - |
| Eleven SR | Mix (knob) | Decay (knob) | Tone (knob) | Pre-Delay (knob) | Type (selector, see below) |

Eleven SR's Type selector (25 options, option list/order HW-confirmed 2026-07-24; each CC value is
a range midpoint, not independently verified per-option):

| CC | Option | CC | Option | CC | Option |
|---|---|---|---|---|---|
| 1 | Echo Room | 48 | Small Theater | 90 | Arena |
| 5 | Studio | 53 | Medium Theater | 96 | Small Plate |
| 10 | Small Room | 58 | Large Theater | 101 | Medium Plate |
| 16 | Jazz Club | 64 | Rich Hall | 106 | Large Plate |
| 21 | Small Club | 69 | Concert Hall | 112 | Canyon |
| 26 | Garage | 74 | Bright Hall | 117 | Supa Long |
| 32 | Medium Room | 80 | Church | 122 | Early Reflect 1 |
| 37 | Tiled Room | 85 | Cathedral | 126 | Early Reflect 2 |
| 42 | Wood Room | | | | |

### Delay (Bypass = CC 28)

| Effect | ID(s) | Status |
|---|---|---|
| BBD Delay | 27, 48 | manual |
| Tape Echo | 28, 49 | **HW-confirmed** (2026-07-24) |
| Dyn Delay | 80, 81, 82 | manual (Mode HW-confirmed at 4 tested points) |

| Effect | S1 (62) | S2 (33) | S3 (35) | S4 (85) | S5 (87) | S6 (34) | S7 (48) | S8 (49) | S9 (55) | S10 (59) | S11 (72) | S12 (73) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| BBD Delay | Delay (knob) | Sync (selector) | Feedback (knob) | Mix (knob) | Input Level (knob) | Mod (toggle: 0-63=Chorus, 64-127=Vibrato) | Depth (knob) | Expanded Delay (toggle) | Noise (toggle) | - | - | - |
| Tape Echo | Delay (knob) | Sync (selector) | Feedback (knob) | Mix (knob) | Rec Level (knob) | Head (knob) | Wow (knob) | Expanded Delay (toggle) | Hiss (toggle) | - | - | - |
| Dyn Delay | Delay (knob) | Sync (selector) | Feedback (knob) | Mix (knob) | Mode (selector, see below) | Ratio (knob) | Hi-Cut (knob) | Lo-Cut (knob) | Width (knob) | Em Rate (knob) | Em Feedback (knob) | Em Mix (knob) |

Dyn Delay's Mode selector (4 tested points HW-confirmed 2026-07-24; exact range boundaries between
them not independently confirmed): 0 = Mono, 42 = Stereo, 85 = Cross, 127 = Pong.

**All three delay effects also have a "Fine" on/off control with no known CC anywhere** - not
exposed in the app. CC 59 was tested and ruled out (2026-07-24; it's actually Dyn Delay's own real
"Em Rate"). See §4.

### FX1 (Bypass = CC 63) and FX2 (Bypass = CC 86)

Same 10 effects in both slots, reusing the exact `EffectDefinitions` entries above/below with
FX1/FX2's own Setting-N CCs substituted in (see §1 for the CC table). Order below matches the
on-unit dropdown order (confirmed 2026-07-24).

| Effect | ID(s) | Status |
|---|---|---|
| Chorus/Vibrato | 11, 39, 40 | manual (see Mod slot for per-param HW status) |
| Multi Chorus | 88, 89, 90 | manual |
| Flanger | 69, 70 | manual |
| Vibe Phaser | 35, 46 | manual - **CCs extrapolated**, not directly documented for FX1/FX2 (see §4) |
| Orange Phaser | 34, 71 | manual |
| Roto Speaker | 75, 76, 77 | manual |
| Graphic EQ | 33, 50 | manual |
| Para EQ | 78, 79 | manual - has 2 more real controls with no known CC, see §4 |
| Gray Compressor | 32 | manual |
| Dyn3 Compressor | 85, 86 | manual |

Param order (positions map to Setting N per §1's FX1/FX2 rows - same order applies to both slots):

| Effect | S1 | S2 | S3 | S4 | S5 | S6 | S7 | S8 | S9 | S10 | S11 | S12 | S13 | S14 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Chorus/Vibrato | Chorus (knob) | Vibrato Rate (knob) | Sync (selector) | Vibrato Depth (knob) | Mode (toggle) | - | - | - | - | - | - | - | - | - |
| Multi Chorus | Rate (knob) | Sync (selector) | Depth (knob) | Pre-Delay (knob) | Mix (knob) | Tri/Sine (toggle) | Voices (knob) | Width (knob) | Lo Cut (knob) | - | - | - | - | - |
| Flanger | Pre-Delay (knob) | Depth (knob) | Rate (knob) | Sync (selector) | Feedback (knob) | - | - | - | - | - | - | - | - | - |
| Vibe Phaser | Volume (knob) | Depth (knob) | Rate (knob) | Sync (selector) | Mode (toggle) | - | - | - | - | - | - | - | - | - |
| Orange Phaser | Rate (knob) | Sync (selector) | - | - | - | - | - | - | - | - | - | - | - | - |
| Roto Speaker | Speed (selector) | Balance (knob) | Type (selector) | - | - | - | - | - | - | - | - | - | - | - |
| Graphic EQ | 100 Hz (knob) | 370 Hz (knob) | 800 Hz (knob) | 2 KHz (knob) | 3.25 KHz (knob) | Output (knob) | - | - | - | - | - | - | - | - |
| Para EQ | L Gain (knob) | LM Gain (knob) | **(unused)** | HM Gain (knob) | H Gain (knob) | Output (knob) | L Freq (knob) | LM Freq (knob) | L Q (knob) | LM Q (knob) | HM Freq (knob) | HM Q (knob) | H Freq (knob) | H Q (knob) |
| Gray Compressor | Sustain (knob) | Level (knob) | - | - | - | - | - | - | - | - | - | - | - | - |
| Dyn3 Compressor | Threshold (knob) | Attack (knob) | Release (knob) | Gain (knob) | Ratio (knob) | Knee (knob) | - | - | - | - | - | - | - | - |

Graphic EQ knob labels also carry documented real-world ranges as text (not enforced in code):
100 Hz (-12 to +12 dB), 370 Hz (-18 to +18), 800 Hz (-18 to +18), 2 KHz (-18 to +18), 3.25 KHz
(-12 to +12), Output (-20 to +6).

**Para EQ's Setting 3 is a genuine, confirmed-unused gap** - moving it does nothing on real
hardware. **Para EQ also has 2 more real controls the manual never documents at all** - "L Type"
(options: Shelf, Peak, HP6, HP12, HP24, Notch) and "H Type" (options: Shelf, Peak, LP6, LP12, LP24,
Notch), confirmed on real hardware 2026-07-24. Neither has a known CC - see §4.

---

## 4. Known gaps - real controls with no CC mapping yet

| Control | Where | What's known | What's missing |
|---|---|---|---|
| VxCr | Wah (both models) | Real knob, exists in `EffectDefinitions` | No CC anywhere in the manual |
| Fine | All 3 Delay effects | Confirmed present on hardware, on/off | No known CC; CC 59 tested and ruled out (2026-07-24) |
| L Type / H Type | Para EQ (FX1/FX2) | Confirmed present on hardware, 6-option selectors each | No known CC; CC 60 (Setting 3, the one known gap) tested and ruled out for both (2026-07-24) |
| Amp tone knobs | Amp/Cab | CC numbers known (§1's Amp row: Setting 1-14), and ~31 real amp model names with their own knob labels are documented in `docs/samples/eleven-rack-user-guide-chapter9-midi-cc-notes.md` | Not wired into `EffectDefinitions`/`EffectEditorComponent` at all - needs a different UI shape (one effect ID with many selectable models, not many effect IDs), and the model list itself needs reconciling (`EffectDefinitions::ampModelOptions()` only has 16, manual has ~31) |
| Volume Pedal, Multi FX Control, Rig Volume pedal | Rig-level | CC numbers confirmed (§2) | Not exposed in any UI component yet (Volume Pedal's params are modeled in `EffectDefinitions` already; the pedal position CCs just aren't wired to a control) |

**Extrapolated, not directly documented**: Vibe Phaser's FX1/FX2 CC mapping (§3, FX1/FX2 section) -
reused its Mod-slot param order, based on the pattern holding for 3 other effects whose FX1/FX2
data *is* directly documented (Chorus/Vibrato, Flanger, Orange Phaser all have FX1/FX2 order
exactly matching their Mod-slot order). Confirmed on hardware that Vibe Phaser genuinely has the
same 5 controls in FX1/FX2 as in Mod, but the specific CC-per-control mapping is the extrapolation,
not independently re-verified control-by-control.

**Guessed CC values (option list confirmed, per-option exact value not independently verified)**:
Roto Speaker's Type, Eleven SR's Type, Dyn Delay's Mode's exact range boundaries (only 4 sample
points tested), the shared tempo-sync selector's range boundaries (§2).

**Bigger, separate effort**: decoding the Bulk Rig SysEx payload (977 bytes) into per-effect-slot
values, so the app could read what's actually loaded instead of requiring manual dropdown
selection everywhere in this file. See `docs/implementation-plan.md` "Not yet scheduled / parked"
and `protocol-spec.md` Open Items - tracked separately, not part of this CC-mapping effort.

---

## 5. Effects modeled but not exposed in any `EffectEditorComponent` slot

These exist in `EffectDefinitions.cpp` (so their real parameters are known, where known) but have
no corresponding UI slot yet:

- **Volume Pedal** (IDs 38, 72): Bypass, Position (knob), Min Volume (knob), Linear/Log (toggle).
  CCs for Bypass (75) and Position (7) are known (§2); Min Volume/Linear-Log have no documented CC.
  **Wired into a real `SlotConfig` (2026-07-28)**: the Signal Chain tab's "Volume" block now opens
  the same live-CC editor as every other mapped slot, sending Bypass (CC 75) and Position (CC 7) -
  Min Volume/Linear-Log stay omitted rather than guessed, same treatment as Wah's second knob.
- **Amp/Cab** (ID 12): Bypass (CC 111, §2) + a 16-option Amp type selector - see §4 for why the
  real tone knobs (CC 13/14/15/16/21/10/112/3/84/24/23/22/44/45) aren't wired up.
- **Rig Params** (ID -1, rig-level globals): Volume, Mono/Stereo, Tempo, 4× Fxc slots, GlSF, Msyc,
  RslL, Vol1, Vol2, Input Selector (9 options), a constant field, True Z Selector (13 options),
  Exp. Pedal Selector (5 options). These are SysEx bulk-rig fields, not live MIDI CC parameters - no
  CC mapping has been sought for most of them. **True-Z specifically WAS sought (2026-07-28) and
  ruled out**: cycling through several settings on real hardware via the new CC-sniffer in
  `DiagnosticsComponent` produced no varying signal at all (just an unrelated, unchanging async
  message - see protocol-spec.md "twenty-sixth round"). Input Selector/True-Z are shown as
  locally-editable dropdowns on the Signal Chain tab's "Input" block (pre-filled from the Bulk Rig
  decode) - not synced live, since no CC exists; the first block mapped with an editable UI but no
  live-write path at all.

---

## Sources

- Official Eleven Rack User Guide - two distinct editions used: `archive.org/details/manualzilla-id-6921695`
  (Chapter 11, the generic CC chart in §1) and Guide Part Number 9320-65073-00 REV B 06/21 (Chapters
  3 and 9, the detailed per-effect/per-amp breakdown most of §3 comes from - see
  `docs/samples/eleven-rack-user-guide-chapter9-midi-cc-notes.md` for the raw extraction).
- ElevenHack (GitLab, `gitlab.com/schmidg/elevenhack`, Apache-2.0, Guillaume Schmid) - source of the
  effect/parameter registry structure (`Effect.mBuildEffect()`), the 16 known Amp/Cab models, and
  the SysEx protocol layer. See `NOTICE` and `docs/third-party-licenses/` for attribution.
- Real hardware testing throughout this project, 2026-07-24 through 2026-07-26 - see
  `protocol-spec.md`'s full hardware validation log for the complete narrative of every
  confirmation and correction referenced above.
