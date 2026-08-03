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

## Effects (non-Amp) — superseded, see master-control-map.md

Everything this section originally covered (Compression, Delay, Distortion, EQ, Modulation,
Reverb, Volume Pedal/Wah, FX Loop/Tap Tempo/Tuner/Misc) has since been fully incorporated into
`EffectDefinitions.cpp`/`SlotConfig.cpp` (per-slot CC data, originally landed via a now-removed
`EffectEditorComponent.cpp`) and `SignalChainComponent.cpp`'s "Rig globals" row (Tap Tempo/FX
Loop/Tuner/Main Volume, originally a now-removed `RigGlobalsComponent.cpp`), cross-checked
against real hardware where noted, and consolidated into
[master-control-map.md](../master-control-map.md) - that file is now the current reference for all
of it. This raw-extraction version was removed (2026-07-26) because several of its notes had gone
stale (describing bugs already fixed - e.g. Tri Knob Disto's order, Eleven SR's Type count mystery
- as if still open) and every data point in it was verified present in the master file first. For
the *history* of how each of these was found/corrected, see `protocol-spec.md`'s hardware
validation log instead - that reasoning wasn't duplicated here to begin with.

The **Amp models** section above is the exception and is kept in full - Amp tone knobs are still
not wired into the app at all (see master-control-map.md §4), so this remains the only place that
data exists.
