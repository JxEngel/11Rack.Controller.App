# Eleven Rack User Guide, Chapter 9 — Full MIDI CC Breakdown (raw extraction notes)

Captured 2026-07-24. This is a **more detailed, per-effect-model/per-amp-model** MIDI CC chart than
the generic "Setting N" table in [protocol-spec.md](../protocol-spec.md) — that table came from an
earlier transcription citing `archive.org/details/manualzilla-id-6921695`. This one comes from a
**different, apparently later manual revision** found via web search:

- URL: `https://static1.squarespace.com/static/50130a40e4b00a22f5c59aea/t/51196305e4b07a990fff66b4/1360618245581/Eleven_Rack_User_Guide.pdf`
- Guide Part Number: **9320-65073-00 REV B 06/21** (per the PDF's own legal-notices page)
- Chapter 9, "Eleven Rack MIDI Controls" (pages 96–124 in this edition, vs. "Chapter 11" pages
  95-98 in the manualzilla edition — different pagination/chapter numbering entirely, consistent
  with a different revision)

**Extraction method**: fetched the PDF, converted to plain text with `pdftotext` (poppler-utils —
no PDF rendering tool was available to visually inspect the original table layout). The source PDF
uses multi-column tables that `pdftotext` flattens into a linear one-value-per-line stream; row/
column association below was reconstructed by matching each named parameter to its CC number using
the already-established generic "Setting N" table, not by visually confirming the original layout.
**Treat everything here as "very likely correct, not hardware-verified"** — good enough to guide
implementation, not a substitute for a real hardware check before shipping anything derived from
it. Two concrete extraction issues already found and worked around:

- For **Amp models**, the manual's print order does follow ascending Setting-N/CC order (verified
  by cross-checking several models) — used directly below.
- For **Delay models** (BBD Delay, Dyn Delay, Tape Echo), the manual's print order does **NOT**
  match ascending Setting-N order (e.g. Mix/CC85 is printed before Feedback/CC35, even though
  Feedback's CC number is lower) — the CC-to-param pairs are correct, but their *position* in the
  "Setting N" sequence had to be re-derived from CC number, not print order. Not yet used in code
  for this reason (see protocol-spec.md Open Items) — flagging here so it isn't miscopied later.
- Some dense multi-effect comparison tables (Compression, Para EQ, Modulation FX1/FX2 lists) had
  clearly interleaved columns after flattening (two side-by-side tables' rows got concatenated) —
  reconstructed with best effort below by matching list lengths to header counts, but treat these
  specifically as lower-confidence than the single-effect tables.

## Amp models — significant open gap

**This chart lists at least 31 distinct amp model names**, each with its own real tone-knob labels
mapped to the generic "Amp Setting N" CCs (13, 14, 15, 16, 21, 10, 112, 3, 84, 24, 23, 22, 44, 45,
in that order) — far more than the **16** models our `EffectDefinitions::ampModelOptions()`
currently has (ported from ElevenHack, which apparently only modeled 16). The names don't obviously
map 1:1 onto our 16 (e.g. "Black SR", "Black Mini", "J45", "MS-30", "RB01b Red, Blue, and Green"
have no obvious match in our list) — **this needs dedicated reconciliation work, not a guess**,
possibly requiring a real hardware capture of the Amp/Cab SysEx parameter to see which numeric
model IDs the unit actually sends. Not attempted here. See protocol-spec.md Open Items.

Per-model tone knob labels, in Setting1→N order (blank = model doesn't use that many settings):

| Model | Setting1 (13) | Setting2 (14) | Setting3 (15) | Setting4 (16) | Setting5 (21) | Setting6 (10) | Setting7 (112) | Setting8 (3) | Setting9 (84) | Setting10 (24) | Setting11 (23) | Setting12 (22) | Setting13 (44) | Setting14 (45) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Tweed Lux | Tone | Instrument Volume | Mic Volume | Noise Gate Threshold | Noise Gate Release | Output | | | | | | | | |
| Tweed Bass | Presence | Middle | Bass | Treble | Bright Volume | Normal Volume | Noise Gate Threshold | Noise Gate Release | Output | | | | | |
| Black Panel Lux Vibrato | Volume | Treble | Bass | Vibrato Speed | Vibrato Sync | Vibrato Intensity | Vibrato On/Off | Noise Gate Threshold | Noise Gate Release | Output | | | | |
| Black Panel Lux Normal | Volume | Treble | Bass | Vibrato Speed | Vibrato Sync | Vibrato Intensity | | Noise Gate Threshold | Noise Gate Release | Output | | | | |
| Black Vib | Volume | Treble | Mid | Bass | Bright Switch | Vibrato Speed | Vibrato Sync | Vibrato Intensity | Vibrato On/Off | Noise Gate Threshold | Noise Gate Release | Output | | |
| Black SR | Volume | Treble | Mid | Bass | Bright Switch | Vibrato Speed | Vibrato Sync | Vibrato Intensity | Vibrato On/Off | Noise Gate Threshold | Noise Gate Release | Output | | |
| Black Mini | Volume | Treble | Bass | Vibrato Speed | Vibrato Sync | Vibrato Intensity | Vibrato On/Off | Noise Gate Threshold | Noise Gate Release | Output | | | | |
| J45 | Presence | Bass | Middle | Treble | Volume 1 | Volume 2 | Noise Gate Threshold | Noise Gate Release | Output | | | | | |
| AC Hi Boost | Normal Volume | Brilliant Volume | Bass | Treble | Cut | Tremolo Speed | Tremolo Sync | Tremolo Depth | Tremolo On/Off | Noise Gate Threshold | Noise Gate Release | Output | | |
| Black Panel Duo | Volume | Treble | Middle | Bass | Bright | Vibrato Speed | Vibrato Sync | Vibrato Intensity | Vibrato On/Off | Noise Gate Threshold | Noise Gate Release | Output | | |
| Plexiglas Vari | Presence | Bass | Middle | Treble | Volume 1 | Volume 2 | Noise Gate Threshold | Noise Gate Release | Output | | | | | |
| Plexiglas - 50w | Presence | Bass | Middle | Treble | Volume 1 | Volume 2 | Noise Gate Threshold | Noise Gate Release | Output | | | | | |
| Plexiglas - 100W | Presence | Bass | Middle | Treble | Volume 1 | Volume 2 | Noise Gate Threshold | Noise Gate Release | Output | | | | | |
| Blue Line Bass | Volume | Treble | Mid | Bass | U-Lo | U-Hi | Mid Freq | Bright On/Off | Noise Gate Threshold | Noise Gate Release | Output | | | |
| Lead 800 - 100W | Presence | Bass | Middle | Treble | Preamp Volume | Master Volume | Noise Gate Threshold | Noise Gate Release | Output | | | | | |
| M-2 Lead | Volume | Treble | Bass | Middle | Drive | Master | Bright | Presence | Noise Gate Threshold | Noise Gate Release | Output | | | |
| SL-100 Drive | Preamp | Bass | Middle | Treble | Presence | Master | Mod | Noise Gate Threshold | Noise Gate Release | Output | | | | |
| SL-100 Crunch | Preamp | Bass | Middle | Treble | Presence | Master | Bright | Noise Gate Threshold | Noise Gate Release | Output | | | | |
| SL-100 Clean | Preamp | Bass | Middle | Treble | Presence | Master | Bright | Noise Gate Threshold | Noise Gate Release | Output | | | | |
| Treadplate Modern | Master | Presence | Bass | Middle | Treble | Gain | Noise Gate Threshold | Noise Gate Release | Output | | | | | |
| Treadplate Vintage | Master | Presence | Bass | Middle | Treble | Gain | Noise Gate Threshold | Noise Gate Release | Output | | | | | |
| MS-30 | Volume | Bass | Treble | Cut | Master | Noise Gate Threshold | Noise Gate Release | Output | | | | | | |
| RB01b Red, Blue, and Green | Presence | Volume | Treble | Mid | Bass | Gain | Boost | Noise Gate Threshold | Noise Gate Release | Output | | | | |
| DC Modern Overdrive | Gain | Bass | Middle | Treble | Presence | Master | Bright | Tremolo Speed | Tremolo Sync | Tremolo Depth | Tremolo On/Off | Noise Gate Threshold | Noise Gate Release | Output |
| DC Modern SOD | Gain | Bass | Middle | Treble | Presence | Master | Bright | Tremolo Speed | Tremolo Sync | Tremolo Depth | Tremolo On/Off | Noise Gate Threshold | Noise Gate Release | Output |
| DC Modern 800 | Gain | Bass | Middle | Treble | Presence | Master | Bright | Tremolo Speed | Tremolo Sync | Tremolo Depth | Tremolo On/Off | Noise Gate Threshold | Noise Gate Release | Output |
| DC Modern Clean | Gain | Bass | Middle | Treble | Presence | Master | Bright | Tremolo Speed | Tremolo Sync | Tremolo Depth | Tremolo On/Off | Noise Gate Threshold | Noise Gate Release | Output |
| DC Vintage Crunch | Gain | Bass | Middle | Treble | Presence | Master | Bright | Tremolo Speed | Tremolo Sync | Tremolo Depth | Tremolo On/Off | Noise Gate Threshold | Noise Gate Release | Output |
| DC Vintage Overdrive | Gain | Bass | Middle | Treble | Presence | Master | Bright | Tremolo Speed | Tremolo Sync | Tremolo Depth | Tremolo On/Off | Noise Gate Threshold | Noise Gate Release | Output |
| DC Vintage Clean | Gain | Bass | Middle | Treble | Presence | Master | Bright | Tremolo Speed | Tremolo Sync | Tremolo Depth | Tremolo On/Off | Noise Gate Threshold | Noise Gate Release | Output |
| DC Bass | Gain | Bass | Middle | Treble | Presence | Master | Bright | Tremolo Speed | Tremolo Sync | Tremolo Depth | Tremolo On/Off | Noise Gate Threshold | Noise Gate Release | Output |

Universal amp CCs (all models): AMP BYPASS = CC111, AMP OUTPUT = CC92, CAB/MIC BYPASS = CC71.

## Effects

All CC numbers below are as they'd be assigned when the effect is loaded into its *natural* slot
(Distortion/Wah/Mod/Reverb/Delay). Several effect types can ALSO be loaded into the generic FX1/FX2
slots, in which case they get a different CC set — those alternate mappings are noted inline where
found, but are lower-confidence (see the Compression/EQ/Modulation flattened-table caveat above).

### Compression
- **Dyn3 Compressor**: Threshold, Attack, Release, Bypass, Gain, Ratio, Knee.
  As FX1: 20, 42, 60, 63, 77, 116, 117. As FX2: 113, 114, 115, 86, 96, 97, 98.
- **Gray Compressor**: Bypass, Sustain, Level.
  As FX1: 63, 20, 42. As FX2: 86, 113, 114.
(Gray Compressor's own `EffectDefinitions` entry is still name-only — this only covers the FX1/FX2
placement CCs, not confirmation of the knobs themselves.)

### Delay (print order ≠ Setting-N order — see caveat above; NOT yet used in code)
- **BBD Delay**: Bypass=28, Delay=62, Sync=33, Mix=85, Feedback=35, Input Level=87, Mod=34,
  Depth=48, Noise=55, Expanded Delay=49.
- **Dyn Delay**: Bypass=28, Sync=33, L/R Ratio=34, Feedback=35, Hi-Cut=48, Lo-Cut=49, Width=55,
  Env Mod Rate=59, Delay=62, EM Feedback=72, EM Mix=73, Mix=85, Mode=87
  (Mode: 0-63="Chorus", 64-127="Vibrato" per the manual text — likely a copy-paste label error in
  the manual itself, since this is a delay effect, not modulation; flagged, not corrected here).
- **Tape Echo** (= our "Tape Delay"): Bypass=28, Delay=62, Sync=33, Mix=85, Feedback=35,
  Rec Level=87, Head=34, Wow=48, Hiss=55, Expanded Delay=49.

### Distortion (cross-validated — matches our own 2026-07-24 hardware test exactly)
- **Green JRC Overdrive** (= our "Green JRC Disto"): Bypass=25, Drive=27, Tone=78, Level=79.
  This is an exact match to the CC27/78/79 → Overdrive/Tone/Level hardware confirmation already in
  protocol-spec.md's "seventh round" — strong cross-validation of both the manual and our test.
- **Black Op Distortion**: Bypass=25, Distortion=27, Cut=78, Volume=79.
- **DC Distortion**: Bypass=25, Distortion=27, Treble=78, Bass=79, Volume=80.
- **Tri-Knob Fuzz** (= our "Tri Knob Disto"): Bypass=25, **Volume**=27, **Sustain**=78, Tone=79.
  Note: this order (Volume/Sustain/Tone) DIFFERS from our current `EffectDefinitions` entry, which
  has Sustain/Tone/Level in that order (ported from ElevenHack's own field order, unconfirmed
  against hardware, and this effect isn't in `EffectEditorComponent`'s Distortion dropdown to begin
  with). Flagged, not fixed — the Distortion slot's 5 offered models are 29/30/31/87/91, and 29 is
  "Tri Knob Disto" — **this may be its own separate ordering bug**, same category as the
  Chorus/Vibrato one already found and fixed, but not yet cross-checked param-by-param here.
- **White Boost**: Bypass=25, Distortion=27, Treble=78, Bass=79, Volume=80.

### EQ
- **Graphic EQ**: Bypass, 100 Hz, 370 Hz, 800 Hz, 2 kHz, 3.25 kHz, Output (matches our existing
  `EffectDefinitions` entry well).
- **Para EQ**: Bypass, L Q, LM Q, HM Freq, L Gain, HM Q, H Freq, H Q, LM Gain, HM Gain, H Gain,
  Output, L Freq, LM Freq (13 real params — currently name-only in `EffectDefinitions`).
  As FX1: 63, 20, 42, 60, 77, 116, 117. As FX2: 86, 113, 114, 115, 96, 97, 98.
  In FX1 (full list): 63, 5, 9, 12, 20, 26, 29, 30, 42, 77, 116, 117, 118, 119.
  In FX2 (full list): 86, 37, 47, 58, 113, 109, 110, 70, 114, 96, 97, 98, 99, 46.

### Modulation
- **C1 Chorus/Vibrato** (as Mod): Bypass=50, Chorus=61, Rate=52, Sync=53, Depth=54,
  Chorus/Vibrato toggle=57. **Already applied** to `EffectDefinitions`/`EffectEditorComponent`
  (this fixed a real ordering bug — see EffectDefinitions.cpp comments).
- **Flanger** (as Mod): Bypass=50, Pre-Delay=61, Depth=52, Rate=53, Sync=54, Feedback=57.
  Still name-only in `EffectDefinitions` — not yet added to the Mod slot's dropdown.
- **Orange Phaser** (as Mod): Bypass=50, Rate=61, Sync=52. Matches our existing entry and mapping.
  As FX1: 63, 20, 42, 60, 77, 116. As FX2: 86, 113, 114, 115, 96, 97.
- **Vibe Phaser** (as Mod): Bypass=50, Volume=61, Depth=52, Rate=53, Sync=54,
  Chorus/Vibrato toggle=57. **Already applied** (previously name-only, now fully known).
- **Multi-Chorus** (as Mod): Bypass=50, Tri/Sine=51, Sync=52, Depth=53, Pre-Delay=54, Voices=56,
  Mix=57, Rate=61, Lo Cut=89, Width=90. Still name-only in `EffectDefinitions`.
  As FX1: 63, 117, 42, 60, 77, 118, 116, 20, 5, 119. As FX2: 86, 98, 114, 115, 96, 99, 97, 113, 37, 46.
- **Roto Speaker** (as Mod): Bypass=50, Speed=61, Balance=52, Type=53. Still name-only.
  Speed is itself a 3-way range selector (Slow/Brake/Fast over 0-31/32-95/96-127), Type an 8-way
  range selector (120/122/21H/Foam/Drum/Rover/Memphis/Wolf/Watery over uneven ranges 0-9 through
  119-127). As FX1: 63, 20, 42, 60. As FX2: 86, 113, 114, 115.

**CC119 resolution**: Multi-Chorus's FX1 list above includes CC119 as its own distinct value,
used simultaneously with CC5 in the *same* effect's CC list (different params). This is strong
evidence that our old generic CC table's "CC 119 = FX1 Setting 9 (*sic* — duplicate of CC 5)" open
item was a **transcription/manual error in that earlier source**, not a real duplicate — CC119 is
a genuine, distinct CC (most likely "FX1 Setting 8", the slot that table was missing), not a repeat
of CC5/"FX1 Setting 9". See protocol-spec.md Open Items.

### Reverb — **already applied** (Mix/Decay/Tone[/Pre-Delay], Type selector deliberately omitted)
- **Blackpanel Spring Reverb**: Bypass=36, Mix=18, Decay=38, Tone=40.
  As FX1: 63, 20, 42, 60, 77, 116. As FX2: 86, 113, 114, 115, 96, 97.
- **Eleven SR** (= our "Stereo Reverb"): Bypass=36, Mix=18, Decay=38, Tone=40, Pre-Delay=39,
  Type=76. Type is a 25-way range selector (Echo, Room, Studio, Small Room, Jazz Club, Small Club,
  Garage, Medium Room, Tiled Room, Wood Room, Small Theater, Medium Theater, Large Theater, Rich
  Hall, Concert Hall, Bright Hall, Church, Cathedral, Arena, Small Plate, Medium Plate, Large
  Plate, Canyon, Supa Long, Early Reflect 1, Early Reflect 2 — 26 names actually listed, one more
  than the 25 CC ranges given, so there's a counting mismatch somewhere in the source table itself)
  over ranges 0-2, 3-7, 8-13, 14-18, 19-23, 24-29, 30-34, 35-39, 40-45, 46-50, 51-55, 56-61, 62-66,
  67-71, 72-77, 78-82, 83-87, 88-93, 94-98, 99-103, 104-109, 110-114, 115-119, 120-125, 126-127.
  Not modeled in code — see the mismatch just noted as exactly why.

### Volume Pedal, Wah — cross-validated, matches our existing code
- **Volume Pedal**: Bypass=75, Position=7.
- **Black Wah** / **Sine Wah** (manual calls it "Shine Wah", likely a typo): Bypass=43, Position=4.
  Confirms Wah genuinely has only ONE MIDI-controllable knob in the entire official spec — VxCr
  isn't just "not yet found," the manual's own exhaustive list has nothing for it.

### FX Loop, Tap Tempo, Tuner, Misc (rig-level, not yet modeled in EffectDefinitions)
- **FX Loop**: Bypass=107, Send=19, Return=108, Mix=88.
- **Tap Tempo**: CC64 (64-127 = a tap).
- **Tuner**: Bypass=69 (matches what we already use).
- **Multiple FX Control**: Pedal Position=11.
- **Rig Volume**: Pedal Position=17.
- **User/Factory Bank Change**: CC32, value 1=Factory Rigs, 0=User Rigs.
  This is a **plain 2-value toggle**, not a generic 128-value MIDI Bank Select MSB/LSB pair as
  originally hypothesized from observing CC0/CC32 traffic (protocol-spec.md fourth round) — see
  Open Items, this narrows (but doesn't fully close) the rig-switching-mechanism question.
