# Eleven Rack Protocol Spec

Working technical reference, built from two sources: the **official** MIDI CC chart (Eleven Rack
User Guide, Chapter 11) and the **reverse-engineered** SysEx protocol (ElevenHack, see
[project-overview.md](project-overview.md) for how that project was found and assessed). Anything
marked "unofficial" comes from ElevenHack and is not yet validated against real hardware — see
[implementation-plan.md](implementation-plan.md) Milestone 4.

## Two separate control mechanisms (confirmed)

The unit uses **two different mechanisms for two different jobs** — this was the open question
from the ElevenHack review, now resolved:

1. **MIDI CC (official, documented)** — real-time control of the currently loaded rig's parameters
   while playing (turn a knob, hit a footswitch). Plain 3-byte MIDI Control Change messages,
   values 0-127. This is what a foot controller or Pro Tools automation would use live.
   **Confirmed actually functional against real hardware (2026-07-24)**, not just documented —
   see "sixth round" below.
2. **Bulk SysEx transfer (unofficial, from ElevenHack)** — loading/saving whole rigs, querying rig
   names/lists, effect descriptions, rig switching. Used for preset/program *management*, not
   real-time tweaking.

Practical implication for our interface layer: **we need both**. CC for "live knob turn" UI
feedback and control, SysEx bulk transfer for rig browsing/loading/saving — confirming the
Milestone 3 design question in the implementation plan.

Not yet resolved: whether rig *switching* while playing (not just loading a rig file) uses MIDI
Program Change, a specific CC, or only the SysEx `CMD_CURR_RIG_NUM` set message ElevenHack uses.
The official manual's CC chapter doesn't mention Program Change at all (searched — no hits), but
multiple forum threads discuss switching rigs via MIDI, so this needs direct verification against
hardware rather than assuming either source is complete.

## MIDI CC Table (official — Eleven Rack User Guide, Table 12, pages 95-98)

Source: `archive.org/details/manualzilla-id-6921695`, Chapter 11 "Controlling Eleven Rack with
MIDI." Manual states: *"All unused continuous controllers have been omitted from this list"* —
gaps in CC numbers below (0-2, 6, 8, 65-68, 89-95, 100) are intentional, not missing data.

Many entries are **generic positional slots** ("Amp Setting 8", "FX1 Setting 9") rather than named
parameters — the actual knob a given "Setting N" controls depends on which effect is loaded in
that slot. This lines up with ElevenHack's `Effect.mBuildEffect()`, which adds named knobs in a
fixed order per effect type (e.g. Tri Knob Disto adds `Driv`, `Tone`, `Levl` in that order) — the
working hypothesis is **CC "Setting 1", "Setting 2", "Setting 3"... map positionally to the order
params are added in ElevenHack's per-effect-type builder**.

**Confirmed against real hardware (2026-07-24)** — see "seventh round" below: with "Green JRC
Disto" loaded (knob order `Driv`/"Overdrive", `Tone`, `Levl`/"Level"), **CC 27 ("Distortion Setting
1") controlled the Overdrive knob** — exactly the first knob in that effect's `EffectDefinitions`
order. This is the first real confirmation of the positional hypothesis, not just a plausible
theory — it validates the core mechanism the entire per-effect parameter editing UI (Milestone 5)
will depend on.

**The full CC# table has moved** to
[master-control-map.md](master-control-map.md) §1 ("Generic 'Setting N' CC tables"), consolidated
there with the per-effect real parameter mappings this doc's hardware-validation log below
established over time. Every row of the original table here was verified present in that file
before this section was trimmed (2026-07-26) - including the CC 101-106 "Knob 1-6" front-panel
entries, which that verification pass caught as a gap in the master file and fixed before this
table was removed. If you need "what CC does X control" quickly, start there; come back here for
the reasoning behind each mapping.

## Second manual revision found — full per-effect/per-amp-model CC breakdown (2026-07-24)

The table above came from one manual edition (`archive.org/details/manualzilla-id-6921695`,
Chapter 11). While researching the CC119 duplicate, a **different manual revision** turned up
(Guide Part Number 9320-65073-00 REV B 06/21) with a much more detailed Chapter 9 — a real
per-effect-model and per-amp-model CC breakdown, not just the generic "Setting N" labels above.
The full effect-by-effect data this section extracted has since been incorporated into code and
consolidated into [master-control-map.md](master-control-map.md). The raw extraction is preserved
at [eleven-rack-user-guide-chapter9-midi-cc-notes.md](samples/eleven-rack-user-guide-chapter9-midi-cc-notes.md)
only for the amp models (still not wired into the app - see master-control-map.md §4); everything
else there was trimmed as redundant (2026-07-26). (Text-extracted via `pdftotext`, not visually
verified against the original table layout — treat as high-confidence but not hardware-proven.)
Headline findings from that extraction, still relevant as history:

- **Resolves the CC119 duplicate**: it's a real, distinct CC (used alongside CC5 in the same
  effect's own CC list, e.g. Multi-Chorus's FX1 mapping) — the "duplicate of Setting 9" in the
  older generic table was itself a transcription error, not a real hardware quirk.
- **Fixed two real ordering bugs** before any hardware test caught them the hard way: Mod slot's
  Chorus/Vibrato effect (Mode toggle belongs at Setting 5, not Setting 1) and Distortion's Tri Knob
  Disto/"Tri-Knob Fuzz" (Volume/Sustain/Tone, not Sustain/Tone/Level) — both corrected in
  `EffectDefinitions.cpp` with comments citing this source.
- **New real parameter data** for Vibe Phaser and both Reverb types (previously entirely
  name-only) — added to `EffectDefinitions.cpp` and `EffectEditorComponent`'s Mod/Reverb slots.
- **Significant new gap found, not yet resolved**: the manual lists at least 31 distinct amp model
  names with real tone-knob labels, vs. our `EffectDefinitions::ampModelOptions()`'s 16 — several
  names (Black SR, Black Mini, J45, MS-30, RB01b...) don't obviously correspond to anything in our
  list. Needs dedicated reconciliation, likely including a real hardware capture of the Amp/Cab
  parameter, before building any Amp tone-knob editor.
- **Narrows (doesn't close) the rig-switching question**: CC32 is documented as a plain
  Factory/User 2-value toggle, not a general MIDI Bank Select MSB/LSB pair as originally
  hypothesized from observed traffic (see "fourth round" below).
- Real Delay parameter data exists too (BBD/Dyn/Tape Echo, all fully named), but the manual's
  print order for Delay doesn't match ascending Setting-N/CC order the way every other effect
  category does — reconstructing the real order needs more care, so it's captured in the reference
  doc but not yet implemented in code.

## Hardware validation log

**2026-07-24 — first real hardware contact.** Using the Milestone 2 skeleton app, sent a Universal
SysEx Identity Request (`F0 7E 7F 06 01 F7`) to a real Eleven Rack over USB-MIDI. Got a real reply:

```
F0 7E 0F 06 02 13 0B 00 01 00 30 31 35 37 F7
```

Decoded (standard MIDI Universal Non-Realtime Identity Reply format):

| Bytes | Field | Value |
|---|---|---|
| `F0 7E` | Universal Non-Realtime header | — |
| `0F` | Device ID | 0x0F |
| `06 02` | Sub-IDs | General Info / Identity Reply |
| `13` | **Manufacturer ID** | **0x13** |
| `0B 00` | **Family code** (LSB-first) | **0x0B** |
| `01 00` | Family member code (LSB-first) | 1 |
| `30 31 35 37` | Software revision, as ASCII | **"0157"** (likely a firmware version) |
| `F7` | End | — |

**This confirms ElevenHack's `SysEx.VENDOR_ID` (0x13) and `SysEx.DEVICE_ID` (0x0B) constants
against real, current-firmware hardware** — not just 2013-era source code. Meaningfully de-risks
Milestone 4 (hardware validation) for the rest of the SysEx frame format.

Also confirms the unit is genuinely USB-MIDI class-compliant: it enumerated as plain OS-level MIDI
ports with no special driver, which is what let this request/reply round-trip at all.

Noted but not yet resolved: the OS enumerated **2 MIDI inputs and 3 MIDI outputs**, all named
"Eleven Rack" — need to identify each port's exact distinguishing name/purpose (likely multiple
MIDI cables on one class-compliant USB-MIDI interface; unclear yet which port is "the" control
port vs. others).

**Suggested next test**: send one of ElevenHack's actual Eleven-Rack-specific query messages (e.g.
`CMD_COUNT_EFFECT` / "request effect count", frame `F0 13 0B 0F 01 22 F7`) rather than only the
generic Universal Identity Request, to validate the full command-specific frame format, not just
the vendor/device ID bytes.

**2026-07-24 — second round: all known-command queries tested against real hardware.** Every
read-only command from the app's "known command" picker got a real, structured reply. All frames
below start `F0 13 0B 0F`, so only the interesting part of each is shown. All numeric decodes were
verified by re-implementing ElevenHack's exact algorithms in a throwaway Python script and
cross-checking encode against decode, not by hand arithmetic.

- **Request Effect Count** → reply `12 22 41 F7`. Message type `12`=RESPOND, command `22` echoes
  `CMD_COUNT_EFFECT` ✓. Value = `0x41` = **65**. This confirms effect count means the total number
  of distinct effect *algorithm models* across the whole device (Effect.java's per-model IDs go up
  past 90), not the 16 effect-*type* categories in `ElevenRack.m_effectTypes`.
- **Request Main Volume** → reply `12 36 00 3F 7F 7F 7F 0F F7`. Command `36` echoes
  `CMD_MAIN_VOLUME` ✓. Value decodes to **127 (full/max)** via ElevenHack's 5-byte "encoded int"
  scheme (`SysEx.extractFrom7bits` / `ParseUtils.coded7toSignedByte`). Verified by also running the
  *encode* direction (`ParseUtils.byteToEncodedInt`) on `127`, which reproduces `3F 7F 7F 7F 0F`
  byte-for-byte. Note: 127 hits a special-cased "full-scale" branch in the encoder, distinct from
  its general linear formula for other values — worth remembering when decoding other knob values.
- **Request Current Rig Number** → reply `12 02 00 07 F7`. Command `02` echoes `CMD_CURR_RIG_NUM`
  ✓. Bank = 0, rig = 7 → per `ElevenRack.m_rigLocToName`, that's rig **"B4"**. **Confirmed exactly
  correct**: the unit's own front-panel display was showing preset B4 at the time of capture. This
  validates the command frame parsing, the param-extraction offsets, and the bank/rig numbering
  scheme all at once — a strong, independent confirmation, not just an internally-consistent one.
- **Request Rig Name (Bank 0, Rig 0 = "A1")** → reply `12 04 00 00 42 69 67 20 42 6C 75 65 00 F7`.
  Command `04` echoes `CMD_RIG_GETNAME` ✓, bank=0, rig=0, name bytes decode as ASCII: **"Big
  Blue"** — a real factory preset name. Confirms the string-extraction offset and format.
- **Request Rig Description** → reply is 34 payload bytes, command `21` echoes `CMD_RIG_DESC` ✓ —
  **this is genuinely new ground**: ElevenHack itself never decoded this structure (its handler is
  just `Term.println("Got RESMOND RIGDESC")`, a stub). Working hypothesis from the raw bytes:
  byte 0 = `0x0B` = **11** (a count), followed by **11 three-byte tuples**. One tuple's first byte,
  `0x37`, exactly matches `Effect.WAH_BLACK` (55 decimal) — real evidence the tuple's first field is
  a specific effect-instance ID. Not every tuple's bytes matched a constant we have on file, so
  either the device has effect IDs ElevenHack never named, or the tuple layout needs refinement
  with more samples (e.g. request rig descriptions for a couple of different known rigs and compare
  which fields move). **Flagged open, not confirmed.**
- **Request Effect Description (Effect 0)** → reply decodes to `strId = "DigiElvnELVu"`,
  `name = "Eleven"`. Genuine surprise: effect index 0 in the ~65-entry enumeration is *not* an
  Amp/Cab model as the `m_effectTypes` ordering might suggest — it looks like some kind of
  placeholder/root/device-identity entry named after the product itself, not a real effect. Worth
  keeping in mind before assuming "effect index" lines up with `m_effectTypes` position.
- **Request Bulk Rig** → a real 1123-byte reply, arrived as **one single JUCE `MidiMessage`**, not
  split into fragments. This answers an implementation question directly: **JUCE's MIDI input
  already reassembles a multi-packet SysEx transfer into one complete message for us** — our own
  interface layer doesn't need to implement the kind of manual partial-message buffering
  `ElevenReceiver.java` does. All payload bytes stayed within 0x00-0x7F as expected for 7-bit-safe
  SysEx data. Saved as a reference sample — see `docs/samples/bulk-rig-sample-2026-07-24.txt` — for
  future decode work (e.g. diffing two bulk dumps after a single parameter change).

**2026-07-24 — third round: first confirmed write.** Sent a `SNDSET CMD_CURR_RIG_NUM` frame
(`F0 13 0B 0F 00 02 <bank> <rig> F7`) via the app's new "Select Rig" control, moving one rig up or
down from the known B4 starting point. **The unit's own front-panel display switched rigs
accordingly** — a real, visually-confirmed write, not just an accepted-looking SysEx reply. Exact
target bank/rig used for this test not recorded - re-run and note the value if an exact before/after
pair is needed later. This is a major milestone: both the read and write directions of the
SysEx layer are now confirmed working end-to-end against real, current-firmware hardware, using
frames derived entirely from 2013-era ElevenHack source. Meaningfully increases confidence that the
rest of ElevenHack's command set (rig naming, bulk rig write, tuner, main volume set) will also
still work, though each still needs its own direct confirmation before being trusted.

### Open items from this round
- [ ] Confirm which port of the 2 MIDI inputs / 3 MIDI outputs is "the" control port
- [x] Verify the Rig Description tuple-structure hypothesis with more samples — see fourth round
      below; confirmed.
- [ ] Figure out what effect index 0 ("Eleven"/"DigiElvnELVu") actually represents — an Effect
      Index spinner (0-64) was added to the Diagnostics tab (2026-07-24) to browse all indices
      freely rather than only index 0, to help figure this out
- [x] Use the saved bulk-rig sample as a baseline for diffing future captures — done (see below).

**2026-07-24 — fourth round: first real hardware run of the refactored `RackController`
(not the friend-class test seam).** Every previously-confirmed decode reproduced identically
through the real transport path (Effect Count 65, Main Volume 127, Rig Name "Big Blue", Effect
Description "Eleven"/"DigiElvnELVu", Bulk Rig 977 decoded bytes) — good regression confirmation.
Three new findings from this round:

1. **Rig Description tuple hypothesis confirmed.** Captured a second Rig Description reply (after
   re-selecting the same rig, Bank 0/Rig 7 = "B4") and diffed it byte-for-byte against the first
   capture from the second round. Every single byte that changed was exactly the *middle* byte of
   one of the 11 three-byte tuples hypothesized earlier — and every one of those 11 bytes shifted
   by the exact same amount, `+14`. This strongly confirms the `[count byte] + 11 × (byte1, byte2,
   byte3)` structure. **New mystery this raises**: the middle byte shifted even though the *same*
   rig was reselected (not a different one) — suggesting it's not a stable per-slot identity tied
   to rig content, but something more like a monotonically-incrementing counter assigned fresh on
   each (re)load/select event. Needs more captures to pin down (e.g. query Rig Description twice
   in a row with no rig-select in between, to see if it's stable absent an actual reselect).
2. **Standard MIDI Bank Select CC messages appeared alongside the SysEx rig-select**, unprompted:
   `B0 20 00` (CC32=0, Bank Select LSB) and `B0 00 00` (CC0=0, Bank Select MSB), both reporting
   bank 0 - matching the bank we selected. These are *not* SysEx and weren't sent by us; the unit
   emitted them on its own, as plain 3-byte MIDI Control Change messages, immediately after our
   `SNDSET CMD_CURR_RIG_NUM` write. This is a genuinely new data point on the long-open "how does
   rig switching really work" question (see Open Items below) — standard MIDI Bank Select is a
   real, if partial, part of the picture, likely intended to keep external MIDI gear (foot
   controllers, etc.) in sync with rig changes made through other means. Whether sending Bank
   Select + Program Change ourselves would *also* successfully switch rigs (as an alternative to
   the SysEx write) is untested.
3. **An additional, unhandled `ASYNCSET` arrived alongside the expected `CMD_CURR_RIG_NUM`
   confirmation**: `F0 13 0B 0F 02 03 07 00 F7` — message type `02` (ASYNCSET), command `03`
   (`CMD_SAVE_RIG` in ElevenHack's naming), params `[07, 00]` (matching the rig/bank we selected).
   `RackController` correctly routed this to `onUnhandledMessage` per its documented design (this
   command is deliberately unhandled - see RackController.h). Whether `0x03` genuinely means "save"
   in this async context, or means something more like "active rig slot changed" that ElevenHack's
   naming doesn't quite capture, is unresolved — ElevenHack's own source doesn't handle this case
   either, so this is new ground, not a gap in our port specifically.

**2026-07-24 — fifth round: Main Volume's real range is signed, and centered, not 0-127.**
Building live two-way sync for the Main Volume slider (drag the slider, it writes to the device;
the device confirms, the slider updates) surfaced a real bug: our C++ port of ElevenHack's
`ParseUtils.byteToEncodedInt` had typed its input as `uint8_t` (unsigned), but the original Java
signature takes a signed `byte`. This silently restricted every value we could ever *send* to the
non-negative half of the true range.

Confirmed against real hardware:
- Sending raw value `0` (our previous "minimum") displayed as **"5.0"** on the unit's own screen —
  the *center* of its 0.0-10.0 scale, not the minimum.
- Sending raw value `127` (the special-cased "full-scale" encoding) displayed as **"10.0"** — the
  maximum, as originally assumed.

This gives a clean linear map, confirmed at two real points and assumed symmetric for the
(previously unreachable) negative half: `display = 5.0 + raw * (5.0/127)`, i.e. raw range
roughly `[-127, 127]` maps to display `[0.0, 10.0]`, centered at `0`/`5.0`. Fixed in
`SevenBitCodec::encodeValue` (now takes `int8_t`, replicating Java's exact sign-extend-then-
unsigned-shift semantics rather than a naive 8-bit shift, which would have quietly mishandled
negative input differently) and `RackController::setMainVolume`. `RigGlobalsComponent`'s slider
now shows the 0.0-10.0 display scale directly, converting internally via `displayToRaw`/
`rawToDisplay`.

**Confirmed (2026-07-24)**: raw `-127` displays as **"0.0"** on the unit's own screen, exactly as
the symmetric hypothesis predicted — the full `[-127, 127]` → `[0.0, 10.0]` map is now verified at
all three key points (min, center, max), not just two.

**Still not checked**: whether this same signed-range mistake affects any *other* single-value
parameter beyond Main Volume — worth checking before assuming every future per-effect knob is
safely unsigned-only.

Also worth flagging: the underlying encoding is inherently lossy by about 1 raw unit (the `>>1` in
`byteToEncodedInt` discards the low bit, so e.g. encoding `-1` decodes back as `-2` on a round
trip) — a pre-existing property of ElevenHack's own scheme, not something introduced by this fix.
Irrelevant to whether a *send* works correctly, but means our own decoded read-back display can be
off by roughly one raw unit (a fraction of the smallest visible "0.1" display step).

**2026-07-24 — seventh round: the CC "Setting N" positional-mapping hypothesis is confirmed.**
Using the Diagnostics tab's Effect Index browser, confirmed the Effect Description catalog
contains real, recognizable effect model names (not just the index-0 mystery entry). With the
unit's currently-loaded rig showing **"Green JRC OD"** in its Distortion slot — matching
`EffectDefinitions`' `"Green JRC Disto"` entry (effect ID 31), whose knob order is `Driv`
("Overdrive"), `Tone`, `Levl` ("Level") — sent **CC 27 ("Distortion Setting 1")** via the new MIDI
CC test tool. **It controlled the Overdrive knob specifically**, exactly the first knob in that
effect's known order.

This is the first real confirmation of the positional hypothesis (not just a plausible read of the
source), and it validates the core mechanism the entire per-effect parameter editing UI
(Milestone 5) depends on: `CC "Setting N"` really does map to the Nth knob of whichever effect is
loaded, in the order `EffectDefinitions` already encodes.

**Fully confirmed (2026-07-24)**, via the new `EffectEditorComponent` ("Effect Editor" tab): all
three of "Green JRC Disto"'s knobs tested and working correctly — **CC 27 → Overdrive, CC 78 →
Tone, CC 79 → Level**, exactly matching `EffectDefinitions`' knob order (`Driv`, `Tone`, `Levl`)
position-for-position. The Bypass toggle (CC 25) was also confirmed working. This is no longer a
single lucky match — three independent positions on the same effect all landed exactly where the
hypothesis predicted, which is about as strong a confirmation as this kind of testing gets.

**2026-07-24 — sixth round: MIDI CC confirmed to actually control the unit, not just documented.**
Sent a plain 3-byte MIDI Control Change, `B0 45 7F` (CC 69 = 127, "Tuner On/Off" per the official
chart), via the new Diagnostics-tab CC test tool. **The unit's tuner engaged for real.** This is
the first confirmation that the official CC chart isn't just paper documentation — it's a genuine,
working live-control mechanism, and validates the "two separate control mechanisms" architecture
this whole project has been built around since Milestone 0.

The unit replied with **two separate `ASYNCSET` messages**, not one:
- `F0 13 0B 0F 02 40 01 F7` (inferred structure; only its *effect* was observed directly — it
  decoded via our existing `CMD_TUNER` (`0x40`) handler, producing "Tuner: On")
- `F0 13 0B 0F 02 41 40 01 F7` — command `0x41` (`CMD_TUNER_A`), params `[0x40, 0x01]`. Routed to
  `onUnhandledMessage` exactly per its documented design (we deliberately don't decode this
  command yet). **New finding**: `CMD_TUNER_A` is not unused/dead — it's a real paired
  notification the unit sends alongside `CMD_TUNER` on every tuner state change. Its second param
  byte (`0x40`) exactly matches `CMD_TUNER`'s own command byte — a plausible but unconfirmed
  hypothesis is that `CMD_TUNER_A` is some kind of generic "referenced command's state changed"
  wrapper (command-byte-being-referenced + new value), not something tuner-specific at all. Not
  enough data yet to be sure - flagged as an open item.

**Still open**: this confirms CC control works for a simple on/off toggle. It does **not** yet test
the "CC 'Setting N' maps positionally to whichever knob a given effect adds in that order"
hypothesis for *continuous* per-effect parameters (e.g. a distortion's drive/gain) — that needs a
follow-up test against a `Setting N` CC while a specific, known effect is loaded.

**2026-07-24 — eighth round: real hardware only has 2 Reverb models, and `EffectEditorComponent`'s
Reverb dropdown showed 5.** User feedback while trying the new Reverb slot: the unit itself only
offers 2 selectable Reverb effects, but the dropdown listed 5 entries — 2x "Spring_Reverb" and 3x
"Stereo Reverb" — and whichever of the 3 "Stereo Reverb" IDs was tried on the unit matched the
single effect the unit calls **"Eleven SR."**

Root cause: `EffectDefinitions`' "sibling ID" groups (multiple ElevenHack effectIds sharing one
name/params — already used elsewhere, e.g. Volume Pedal's 38/72, Chorus/Vibrato's 11/39/40) are not
distinct selectable models, just multiple underlying IDs for the same real effect. Reverb's groups
(37/47 for the Spring reverb, 51/52/53 for Eleven SR) hit this hardest since they have the most
siblings, but `EffectEditorComponent`'s dropdown was populated with one entry per raw effectId
rather than deduplicating by name — so it showed every sibling as if it were a separate option. Now
hardware-confirmed (not just theorized) that these sibling IDs are functionally identical for live
CC control purposes; the leading hypothesis for *why* they exist at all is one ID per rack slot an
effect can be placed into (native slot / FX1 / FX2 — see the alternate CC sets many effects have
"as FX1"/"as FX2" in the Chapter 9 breakdown above), not per-model variation.

**Fixed**: `EffectEditorComponent::rebuildEffectList()` now deduplicates by definition name, keeping
the first effectId seen per name. Also renamed the two Reverb entries to match the unit's own
on-screen names exactly (`EffectDefinitions.cpp`'s internal `"Spring_Reverb"`/`"Stereo Reverb"` →
`"Blackpanel Spring Reverb"`/`"Eleven SR"`). This fix generalizes to the Mod slot too, which had the
same problem less visibly (Chorus/Vibrato's 3 IDs and Orange Phaser's 2 IDs were also each showing
as separate dropdown entries before this fix, just not yet reported).

**2026-07-24 — ninth round: Chorus/Vibrato's Sync control didn't work - wrong value scale, not a
wiring bug.** User report: "chorus/vibrato controls don't seem to work." Root cause found by
re-examining the Chapter 9 breakdown: our `Sync` selector's options used ElevenHack's own small
0-13 index scheme (the encoding for the *bulk SysEx rig-file field*, a completely different
transport), not the live-CC encoding. The manual documents a distinct "FX Sync Setting Values"
table for CC-controlled sync parameters: the CC byte (0-127) is bucketed into 14 contiguous ranges
(0-4, 5-14, 15-24, ... 124-127), one per named note value. Sending the raw 0-13 index instead meant
almost every selectable option landed in or near the very first bucket ("Off") regardless of which
one was actually picked — from the user's perspective, moving the Sync dropdown did effectively
nothing.

**Fixed**: added a shared `ccSyncSelector()` helper in `EffectDefinitions.cpp` using the real
14-value/14-range table, each option's CC value set to its range's midpoint. Applied to both
Chorus/Vibrato's `Sync` and Vibe Phaser's `Sync` (the latter was previously modeled as a plain
knob specifically because this exact table wasn't confirmed yet for it - same context, same label,
so almost certainly the same scale).

**Confirmed (2026-07-24)**: after the fix, Sync works. So do Bypass, the Mode toggle, and the
Chorus/Rate/Depth knobs — the "Chorus" knob briefly looked broken too, but that was the unit
sitting in Vibrato mode (no live readback, so we don't know/set the unit's actual current mode) —
the Chorus-specific knob is naturally inert while in Vibrato mode, not a bug. Switching Mode
explicitly and retrying confirmed it. **Chorus/Vibrato is now the second fully hardware-confirmed
effect in `EffectEditorComponent`, after Distortion.**

**Tenth round: the Mod slot was missing half the unit's real effect list.** User feedback: the
unit's Mod slot actually offers 6 effects, in this on-screen order: C1 Chor/Vib, Multi Chorus,
Flanger, Vibe Phaser, Orange Phaser, Roto Speaker. Only 3 were in the dropdown. Added the other 3
using the same Chapter 9 sourcing as before:
- **Flanger**: fully known now (Bypass, Pre-Delay, Depth, Rate, Sync, Feedback) - fits the existing
  7-slot Mod `settingCc` array cleanly.
- **Multi Chorus**: fully known now (Bypass, Rate, Sync, Depth, Pre-Delay, Mix, Tri/Sine, Voices,
  Lo Cut, Width) - needed the Mod slot's `settingCc` array extended from 7 to 9 entries, since two
  of its real CCs (89, 90) fall outside the officially-named "Modulation Setting 1-7" list.
- **Roto Speaker**: initially only partially known (Bypass, Speed, Balance) - its "Type" selector
  was left out because the manual's named-option count (9 tokens: "120 122 21H Foam Drum Rover
  Memphis Wolf Watery") and range count (8: 0-9,10-27,28-45,46-63,64-82,83-100,101-118,119-127)
  didn't match during transcription.

**Eleventh round: Roto Speaker's Type - confirmed a list control, wrong merge guessed, then
corrected against the real on-unit list.** User feedback: Roto Speaker's Type is indeed a
selector, not a knob. First attempt reasoned that "120" and "122" (adjacent tokens with no
separator in the transcription) were one grouped option, "120/122," since that gave exactly 8
options matching the 8 known CC ranges. **Confirmed wrong**: 120 and 122 are two distinct, separate
options on the real unit. The user then read the actual 8-option list directly off the unit:
120, 122, 21H, "Foam Dr", Rover, Memphis, Wolf, Watery - the real merge was "Foam"+"Drum" into one
(likely display-truncated) option, "Foam Dr", not "120"+"122". Re-added with this corrected list,
values still at each range's midpoint. **The option list and order are now hardware-confirmed; the
specific CC value chosen per option is still just a range midpoint, not independently verified.**

None of Flanger, Multi Chorus, or Roto Speaker are hardware-tested yet - only Chorus/Vibrato and
Orange Phaser (via Distortion's earlier positional-knob confirmation) have real hardware evidence
behind them so far.

**Twelfth round: Orange Phaser's Sync was also wrongly modeled as a toggle.** User feedback: Orange
Phaser's Sync is a list, not an on/off switch, same as the other Sync controls in this slot. The
manual's own description settles it - "Sync Synchronizes the modulation rate to the Rig tempo by a
specific rhythmic subdivision" is unambiguously the tempo-sync note-value scheme (the same shared
`ccSyncSelector` table used elsewhere), not a toggle; this was a modeling mistake made when Orange
Phaser was first added, before that shared table existed. Fixed in `EffectDefinitions.cpp`. Not yet
re-tested against hardware.

This is now the second Sync-as-toggle mistake found (after Chorus/Vibrato's Sync-as-tiny-index
mistake) — worth deliberately re-checking every other place a bare `toggle("Sync", "Sync")` or
similarly-labeled control appears before assuming any of them are simple on/off switches.

**Thirteenth round: Eleven SR's "Type" selector added, resolving the 26-vs-25 count mismatch the
same way as Roto Speaker's.** User feedback: Eleven SR's Type selector was missing from the UI (it
had been deliberately left unmodeled - see the eighth/tenth-round notes above - because the
manual's raw transcription came out to 26 name tokens against only 25 CC ranges). Same pattern as
Roto Speaker: the user confirmed against the real on-unit list that the first two tokens, "Echo"
and "Room", are one option, "Echo Room" - not two. That resolves the count to 25-and-25 cleanly.
Added as a selector in `EffectDefinitions.cpp` (CC values at each range's midpoint) and wired into
the Reverb slot's existing `settingCc` array (Type lands on the already-present Setting5/CC76 slot
- no array changes needed). **Option list/order is hardware-confirmed; the specific CC value per
option is still just a range midpoint, not independently verified** - same caveat as Roto Speaker's
Type. Not yet re-tested against hardware end-to-end.

**Fourteenth round: Tap Tempo and FX Loop added to `RigGlobalsComponent`.** After a status check on
what's still missing overall (Delay, FX Loop, FX1/FX2, Tap Tempo), added the two easy ones: Tap
Tempo (CC 64, a single momentary button - "64-127 = a tap") and FX Loop (Bypass=107, Send=19,
Return=108, Mix=88 - promoted from name-only to real params in `EffectDefinitions.cpp`). Neither
fits `EffectEditorComponent`'s "pick which model is loaded" pattern - there's exactly one of each
on the whole unit, not several selectable models - so both were added directly to
`RigGlobalsComponent` alongside Main Volume/Tuner instead, using their fixed CCs (not a "Setting N"
positional scheme). Not yet hardware-tested. Delay and FX1/FX2 remain open - see Open Items.

**Fifteenth round: Delay added to `EffectEditorComponent` - BBD Delay and Tape Echo fully, Dyn
Delay only partially.** The CC data existed already (see "Second manual revision found" above), but
had been deliberately deferred because Chapter 9's print order for Delay doesn't follow ascending
Setting-N/CC order the way every other category does. Resolved the true order by looking up each
named param's CC against the confirmed generic "Delay Setting N" table (Setting1=62, 2=33, 3=35,
4=85, 5=87, 6=34, 7=48, 8=49, 9=55).

Cross-referencing Chapter 3's "Exploring Rigs" plain-English descriptions (not just the Chapter 9
CC table) turned out to matter for getting each param's real *type* right, not just its order -
without it, at least two params would have been wrongly modeled as knobs: BBD Delay's "Mod" is
described as "Switches the modulation effect between Vibrato...and Chorus" (a toggle, same
Chorus/Vibrato convention as C1 Chor/Vib's own Mode switch), and both BBD Delay's "Noise" and Tape
Echo's "Hiss" are explicitly described as toggle switches. Sync is modeled as the same shared
tempo-sync selector as everywhere else - Dyn Delay's own description confirms it explicitly
("Ranges from OFF...to a variety of rhythmic note values").

**BBD Delay and Tape Echo are fully modeled** (Bypass + all 9 real params each). **Dyn Delay is
only partially modeled** (Bypass, Delay, Sync, Feedback, Mix) - its own description goes on to
describe a "Mode" selector (Mono/Stereo/Cross/Pong routing options) plus several more real,
well-described params after it (L/R Ratio, Hi-Cut, Lo-Cut, Width, Env Mod Rate, EM Feedback, EM
Mix), but the manual gives **no CC-range breakdown at all** for Mode's 4 options (unlike Roto
Speaker's or Eleven SR's selectors, which at least had a rough range table to start reconstructing
from). Since the positional CC mechanism can't skip a slot, guessing Mode's ranges wrong would
silently misalign every param after it onto the wrong CC - so, consistent with how Wah's VxCr and
the original Reverb Type gaps were handled, this stops at Mix rather than guess. "Expanded Delay"
(BBD Delay/Tape Echo, Setting8) has a confirmed CC but no Chapter 3 description at all, so its real
type (knob vs. switch) is unconfirmed - modeled as a knob, the safer default when uncertain.

None of Delay is hardware-tested yet.

**Sixteenth round: two more Delay corrections.** User feedback: (1) "Expanded Delay" is confirmed
an on/off switch, not a knob - **fixed** in `EffectDefinitions.cpp` (both BBD Delay and Tape Echo).
(2) All three delay effects have a "Fine" on/off control the user doesn't yet know the function of
- this matches the "Fine" control described under Dyn Delay's own Chapter 3 write-up ("Toggles
finer control of delay time in or out. Toggled by SW2 in page one of the controls"), which turns
out to be shared across all three delay types, not Dyn-Delay-specific as its placement in the text
implied. **Still no confirmed CC for it anywhere in the manual.** Leading hypothesis, not yet
verified: BBD Delay and Tape Echo only use CCs for Setting1-9 out of the 14 slots the generic
"Delay Setting N" table has (Setting10-14 = CC59/72/73/74/31 are unused by either) - "Fine" is a
plausible candidate for Setting10/CC59, the next available slot, on those two effects at least.
Needs a hardware check (send CC 59 at 0 and 127 via the Diagnostics tab while a delay effect is
loaded, watch if Fine toggles) before encoding it - not added to code yet.

**Seventeenth round: Tape Echo confirmed fully working (Expanded Delay fix included); Dyn Delay's
Mode confirmed and the rest of that effect completed; CC 59 ruled out for Fine.** User tested
CC 59 (the "Fine" hypothesis above) with no effect - **ruled out**. It turns out CC 59 wasn't
actually free to begin with: it's Dyn Delay's own real "Env Mod Rate" control, so it was never a
clean guess. Fine's CC remains unknown; since it's shared across all three delay types but no
single Setting-N slot is unused by all three simultaneously, it likely has its own dedicated CC
outside this whole numbering scheme, or may not be MIDI-addressable at all (the manual describes
it as toggled by a physical "SW2" switch, hinting at a front-panel/display-only control). Left
unresolved - see Open Items.

Also confirmed: Tape Echo's controls (Delay/Sync/Feedback/Mix/Rec Level/Head/Wow/Hiss, plus the
just-fixed Expanded Delay) all work correctly on real hardware - what looked like "missing"
controls in an earlier report was Dyn Delay's deliberately-partial modeling, not a regression.

User also swept Dyn Delay's Mode (CC 87) at 0/42/85/127 and got Mono/Stereo/Cross/Pong in that
exact order - **confirmed**. This unblocked the rest of Dyn Delay's real, Chapter-3-described
params (Ratio, Hi-Cut, Lo-Cut, Width, Em Rate, Em Feedback, Em Mix - all plain knobs), now added.
**Dyn Delay is fully modeled.** The exact range boundaries between Mode's 4 values are still not
independently confirmed - only the 4 tested points are hardware-verified, same caveat as Roto
Speaker's/Eleven SR's selectors.

**All of Delay (BBD Delay, Tape Echo, Dyn Delay) is now fully modeled**, except Fine (all three)
and Expanded Delay's exact CC-range boundary, unconfirmed either. This slot is now hardware-tested
for Tape Echo end-to-end; BBD Delay and Dyn Delay's knobs are proven correct only via the shared
CC-mapping mechanism and Dyn Delay's own Mode sweep, not independently retested control-by-control.

**Eighteenth round: FX1/FX2 unblocked - they're not free-form after all.** User checked the real
unit and found FX1/FX2 only ever host a **fixed** effect list, resolving the core blocker that had
kept these slots out of scope since Milestone 5 began: C1 Chor/Vib, Multi Chorus, Flanger, Vibe
Phaser, Orange Phaser, Roto Speaker (the same 6 Mod-slot effects), plus Graphic EQ, Para EQ, Gray
Compressor, and Dyn3 Compressor.

Re-examining the manual's own "(as FX1)"/"(as FX2)" CC lists for these effects surfaced two more
corrections, both caught before hardware testing:
- **Multi Chorus's "Width" and "Lo Cut" were in the wrong order** in the Mod slot's own definition.
  Cross-checking against FX1's well-established, independently-confirmed CC table showed Width
  landing on the lower-numbered Setting slot and Lo Cut on the higher one - the opposite of how the
  manual's own *named* param order lists them. Same "named order != true positional order" pattern
  as Delay. Fixed in `EffectDefinitions.cpp` (affects the Mod slot too, not just FX1/FX2).
- **Vibe Phaser has no documented FX1/FX2 CC data at all**, unlike every other Mod-slot effect -
  even though the user confirmed the real unit can place it there. Initially excluded from the
  FX1/FX2 dropdown rather than guess.

**Nineteenth round: Vibe Phaser added to FX1/FX2 after all.** User feedback: Vibe Phaser is
present in FX1/FX2 with the same controls as the Mod slot (Volume, Depth, Rate, Sync, Chorus/
Vibrato switch). Rather than the manual simply lacking this documentation, the other three
Mod-slot effects with confirmed FX1/FX2 data (Chorus/Vibrato, Flanger, Orange Phaser) all showed
their FX1/FX2 param order exactly matches their Mod-slot order, just wired to different CCs - a
consistent enough pattern across 3 effects to extrapolate the same for Vibe Phaser with reasonable
confidence. Added using its existing Mod-slot param order/CCs unchanged (no new `EffectDefinitions`
work needed - the data model was already complete and correctly ordered). **This one CC mapping is
extrapolated, not directly documented** - flagged as such in the FX1/FX2 note text, unlike the
other effects in this slot which have their own explicit manual CC data.

Promoted three previously name-only effects using the same CC-to-Setting-N reconstruction as Delay:
**Gray Compressor** (Bypass, Sustain, Level), **Dyn3 Compressor** (renamed from ElevenHack's
internal "Dyn Compressor" - Bypass, Threshold, Attack, Release, Gain, Ratio, Knee), and **Para EQ**
(Bypass + 13 real EQ-band params). Unlike every other effect category, none of these three (nor
Graphic EQ) has a dedicated native slot on the unit at all - FX1/FX2 is the *only* place they're
controllable. Para EQ has a genuine gap at Setting3 (no real param uses it) - modeled as an explicit
"(unused)" placeholder rather than skipped outright, since skipping would misalign every param
after it; moving that control does nothing on real hardware, which is expected, not a bug.

Added two new slots, "FX1" and "FX2", to `EffectEditorComponent` - same pattern as every other slot,
reusing the existing `EffectDefinitions` entries (Chorus/Vibrato, Multi Chorus, Flanger, Orange
Phaser, Roto Speaker, Graphic EQ all already existed; only the 3 compressor/EQ effects above needed
promoting) with FX1/FX2's own bypass/Setting-N CCs substituted in. None of this is hardware-tested
yet.

**Also fixed while implementing this**: some effects (Para EQ has 14 real params) have more control
rows than reliably fit in the app window regardless of size - `EffectEditorComponent`'s param list
is now inside a scrollable `juce::Viewport` rather than a fixed-height area, so this scales to any
number of params going forward instead of silently clipping/overlapping off-screen.

**Twentieth round: Para EQ has 2 more real controls the manual never documented - "L Type" and
"H Type".** User feedback, read directly off the unit's screen with Para EQ loaded in FX1: the
real control list is Gain/Freq/Q/**Type** per outer band (L, H - Type options Shelf/Peak/HP6/HP12/
HP24/Notch for L, Shelf/Peak/LP6/LP12/LP24/Notch for H), plus Gain/Freq/Q (no Type) for each inner
band (LM, HM), plus Output - 14 real controls total, confirmed to be exactly the 13 already coded
(cross-verified against the manual's own explicit CC-to-name pairing, unaffected by this finding)
plus these 2 new Type selectors, neither of which the manual's Chapter 9 CC table documents at all.

Important distinction: the on-screen band-grouped order the user described (L block, Output, LM
block, HM block, H block) is a **display** order, not necessarily the internal CC-Setting-N order
`EffectEditorComponent`'s positional mechanism depends on - so the existing 13 params were *not*
reordered to match it, only the new Type controls need placing.

Tested the one known-unused slot (Setting3, CC 60 in FX1) as a candidate for one of the two Type
controls - **no effect on either Type control**. Ruled out. Neither Type control has a known CC.
**Deferred** - not blocking anything else, since FX1/FX2's other 9 effects are unaffected; Para EQ
stays at its current 13-real-param + 1-placeholder state until there's a real lead on the Type CCs.

**Twenty-first round (2026-07-26): the Bulk Rig payload's byte format is fully decoded - not
guessed, read directly from source.** ElevenHack's own `tfx/TfxParser.java`/`Section.java`/
`ParseUtils.java` turned out to document the exact structure - the "most promising lead" flagged in
the Open Items entry below was correct. Format, confirmed byte-for-byte against both ElevenHack's own
`bulkdump.bin` test fixture and our real hardware capture:

- 36-byte header: 4-byte version + 4-byte header code (both raw, no transform) + a 28-byte rig name
  built from 7 quadlets, each individually byte-reversed (`buf[3],buf[2],buf[1],buf[0]`) then
  concatenated and trimmed at the first NUL.
- One TOC `Section` (id `'A'`) immediately after the header: a 4-byte block header (`byteSize` as a
  little-endian uint16, always a multiple of 8; `sectionId` as the 4th byte) followed by exactly
  `byteSize/8` key/value quadlet pairs - the pair count is derived from `byteSize`, not a hardcoded
  37. Each pair is a 4-char tag (same byte-reversal as the name) plus a signed 32-bit little-endian
  value (can legitimately be negative - e.g. `Driv = -1073741824` - this is Java `int` overflow
  semantics carried through, not a bug).
- The TOC holds rig-level globals (`RVol`, `Tmpo`, `FXc1`-`FXc4`, `Msyc`, etc. - matching most of the
  existing `Rig Params`/`RigGlobalsComponent` registry, plus one previously-undocumented field,
  `WoLS`) and, for each of 10 signal-chain slot letters `'C'` through `'L'`, a `Wor<letter>`/
  `Wst<letter>` pair giving that slot's `effectId`/`category`. `WorB`/`WstB` are themselves rig
  globals despite the name shape - there is no slot letter `'B'`.
- 10 more `Section`s (ids `'C'`-`'L'`) follow back-to-back with no padding, count field, or end
  marker - the only termination signal is fewer than 12 bytes remaining. Each holds that slot's own
  raw field-tag → value map (section byte sizes vary per effect - verified 7/4/16/3/24/4/10/6/10/4
  pairs across the one real sample checked, not a fixed per-effect-type length).
- Verified independently in a Python prototype before porting (this project's standard practice, per
  `SevenBitCodec`'s earlier validation) - ElevenHack's reference fixture decoded to exactly the
  expected 1082 bytes, rig name "Metal Rythm 1", with every TOC/section byte boundary matching
  ElevenHack's own logic exactly.
- One correction along the way: the 7-bit decode input must be `message[6:]` **including** the
  trailing `F7` terminator (matching ElevenHack's own `Arrays.copyOfRange(msg, 6, msg.length)`), not
  `message[6:-1]` as first assumed - the extra trailing byte doesn't affect the decoded result's
  meaningful content, since `SevenBitCodec::decodeFrom7Bits` already always produces one
  not-fully-meaningful trailing byte regardless (see `SevenBitCodec.h`).
- Shipped as `Source/Rack/BulkRigParser.h`/`.cpp` + `BulkRigParserTests.cpp` (build-verified, all
  test groups pass). This is a **structural** decode only - the TOC's `effectId`/`category` numbering
  isn't yet reconciled against `EffectDefinitions`'s own IDs, and the signed-32-bit values aren't
  calibrated against the 0-127 CC scale `EffectEditorComponent` uses. See
  docs/implementation-plan.md Milestone 5 for the full writeup and what's still open.

**Twenty-second round (2026-07-27): the Bulk Rig slot letter IS the real signal-chain position -
confirmed against a real rig, not just inferred.** User loaded a real rig ("SLO 100") with an
independently-known real pedal order (read directly off it) and compared it against a live Bulk Rig
decode's slot letters:

- Decoded letter order (C through L): `Vol, Wah, FX2, Disto, AmpCab, FXLoop, Mod, Delay, FX1, Reverb`
  (AmpCab is one combined slot/letter for both Amp and Cab - see the twenty-first round above).
- Real order on the unit: `VOL, WAH, FX2, Dist, Amp, Cab, FX Loop, MOD, DLY, FX1, REV`.
- These match **exactly**, position for position (expanding AmpCab's one letter into its two
  adjacent real blocks, Amp then Cab). This confirms `BulkRigParser::ParsedRig::slots`'s existing
  C-to-L order (already documented in `BulkRigParser.h` as "in letter order") directly encodes real
  signal-chain position - not just weakly suggested by one earlier sample as previously written here.

**Correction to earlier hardware-structure notes**: Amp/Cab's position among the other blocks is
**not** fixed - only their adjacency to each other is (Amp always immediately before Cab). In this
sample, the Amp/Cab pair sat in the middle of the order (after Distortion, before FX Loop), not in
any single fixed spot. Input and Output still aren't confirmed the same way (neither appears in the
Bulk Rig payload at all), but the unit's own manual describes them as the chain's fixed endpoints,
which is retained as the one remaining unconfirmed-but-reasonable assumption.

Two follow-on fixes from this round:
- `SignalChainComponent` (the new "Signal Chain" tab - see docs/implementation-plan.md Milestone 5)
  was rebuilding the chain in a fixed guessed order regardless of the real decode - fixed to build
  the chain's actual order directly from `rig.slots` (already correct) instead.
- `DiagnosticsComponent`'s Bulk Rig log was printing each slot's letter as its ASCII code ("67"
  instead of "C") - a `juce::String` numeric-conversion overload was firing for the raw `char`
  instead of the single-character one; `juce::String::charToString` fixes it. This is what made the
  letter-order comparison hard to read in the first place.

**Twenty-third round (2026-07-27): cross-referenced all 3 real decoded rigs against
`EffectDefinitions.cpp` to reconcile Bulk Rig raw field tags with live-CC `ParamDefinition::key`
values - "Round 1" of mapping decoded values into the UI.** Went through every effect in the 7
`SlotParamsPanel`-editable slots (Distortion/Wah/Mod/Reverb/Delay/FX1/FX2) we have real decoded data
for, comparing our own "JCM 800" and "SLO 100" captures plus ElevenHack's "Metal Rythm 1" reference
fixture against each effect's CC-side key list:

- **Exact string matches** (safe, no guessing): Sine Wah/Black Wah (`Filt`,`VxCr`), DC_Disto
  (`Driv`,`Treb`,`Bass`,`Levl` - and by strong analogy the other 4 Distortion models, which share
  ElevenHack's one internal Distortion field set), Flanger (`Dpth`,`Fdbk`,`Sync` only - NOT
  `PreD`/`Rate`, which turn out to differ from the bulk tags `PDly`/`Sped`), Multi Chorus (`Dpth`,
  `Mix `,`Rate`,`Sync`,`Voic`,`LoCt` only - NOT `PreD`/`TriS`/`Widt`, which differ from `PDly`/
  `Wave`/`Wdth`), Tape Echo (`Sync`,`Hiss`,`Wow ` only - NOT `Dely`/`Fdbk`/`Mix `/`RecL`/`Head`/
  `ExpD`), Eleven SR (`Tone`,`Type`).
- **Key finding that bounds this round's scope**: every single per-effect KNOB value across all 3
  samples is byte-identical to the same effect's value in every OTHER sample - i.e. every knob we've
  ever decoded is sitting at its untouched factory default. We have zero evidence of what a
  non-default knob position looks like as a raw int32, so calibrating knobs to the 0-127 CC scale
  right now would be a fabricated guess, not a derived formula. Confirmed with the user; deferred to
  a follow-up hardware calibration test ("Round 2" - see docs/implementation-plan.md).
- **Toggles are different: 0/1 is unambiguous regardless of scale**, so any toggle with a confirmed
  exact-match raw tag can be safely read back right now. Across all 7 editable slots' effects,
  exactly **one** such toggle exists in our data: Tape Echo's `Hiss`. Every other toggle in these
  slots either has no real decoded data at all (Chorus/Vibrato, Vibe Phaser, BBD Delay) or only a
  semantically-plausible-but-not-exact-string bulk tag (Multi Chorus's `TriS` "Tri/Sine" likely
  corresponds to bulk tag `Wave`, Tape Echo's `ExpD` "Expanded Delay" likely corresponds to `4X  ` -
  neither wired up, to avoid showing a wrong toggle state under a real-looking label).
- Also confirmed, not wired into any UI yet: `Amp/Cab`'s `sld6` (amp model selector) is a compact
  0-15 index matching `EffectDefinitions::ampModelOptions()`'s own index scale directly - resolves
  the "known gap" flagged in `EffectDefinitions.h` about which of two possible amp-model encodings
  the unit actually uses on the wire. Rig Params' `WorB` global also matches its own selector's value
  scale exactly (`WorB=60` = "Guitar"). Neither is wired into any UI - Amp/Cab has no `SlotConfig`
  entry yet, and Rig Params globals aren't editable at all - recorded here for whenever they are.
- Shipped: `SlotConfig.h`/`.cpp`'s new `confirmedRawTagForKey()` (a small table of only the
  confirmed-exact-match pairs above, returning `nullopt` for anything not directly confirmed -
  deliberately data, not a positional or name-similarity heuristic), `SlotParamsPanel::setSlot()`'s
  new `knownToggleStates` parameter, and `SignalChainComponent` populating/passing it through from a
  live decode. Build-verified (both targets compile/link, 67 test groups pass, 5 new).

**Twenty-fourth round (2026-07-28): "Round 2" hardware calibration test - knob raw-value scale
confirmed.** The user ran a real full-range sweep of Sine Wah's `Filt` knob (the "Position"/wah-pedal
knob) and decoded the Bulk Rig payload at three points:

| Knob position | Raw `Filt` value |
|---|---|
| Fully down (min) | `-2147483648` (`INT32_MIN`) |
| Exact centre | `0` |
| Fully up (max) | `2147483647` (`INT32_MAX`) |

This is the full native signed-32-bit range, used linearly, split exactly at zero - the standard
**Q31 fixed-point** pattern (a normalized value stored at full 32-bit resolution: min/mid/max land
exactly on `INT32_MIN`/`0`/`INT32_MAX`, not on some smaller or offset range). Conversion to the 0-127
CC scale `SlotParamsPanel`'s sliders use:

```
t  = (raw - INT32_MIN) / 4294967295.0   // 0.0..1.0
cc = round(t * 127)
```

**Scope decision (confirmed with the user)**: rather than range-test every individual knob, this
formula is now applied to every OTHER knob-kind param `confirmedRawTagForKey()` already has a
confirmed raw tag for (Distortion's `Driv`/`Tone`/`Levl`/`Treb`/`Bass`, Flanger's `Dpth`/`Fdbk`, Multi
Chorus's `Dpth`/`Rate`/`Voic`/`LoCt`, Tape Echo's `Wow `, Eleven SR's `Tone`, Wah's `VxCr`) - on the
assumption this is the unit's one general knob encoding, not something bespoke per effect. This is a
well-supported hypothesis (Q31 is an extremely standard, exact-fit fixed-point format, not a
coincidental-looking number), but only Wah's `Filt` has actually been range-tested - treat the rest
as extrapolated, same treatment as the earlier "Vibe Phaser CC mapping" extrapolation. Selector-kind
params (`Sync`, `Type`, etc.) are explicitly NOT covered by this formula and remain unconverted - they
use a different, still-unconfirmed encoding (see the `Amp/Cab sld6` compact-index finding above,
which shows selectors don't all share one scale either).

Shipped: `SlotConfig.h`/`.cpp`'s new `knobRawToCcValue()`, `SlotParamsPanel::setSlot()`'s new
`knownKnobValues` parameter, and `SignalChainComponent` populating/passing it through from a live
decode (converting each confirmed knob-kind raw tag's value via `knobRawToCcValue()` instead of just
toggle-kind ones). Build-verified (both targets compile/link, 68 test groups pass, 1 new covering the
confirmed min/mid/max readings).

**Twenty-fifth round (2026-07-28): extended reconciliation to the "near-miss" pairs "twenty-third
round" deliberately excluded.** User asked to cover "all the different effects and their values" -
this splits into two separate gaps: the value-scale formula (solved above) and knowing which raw
field corresponds to which CC param at all (tag identification). For the latter, the twenty-third
round had already turned up plausible name-similarity correspondences for two effects (real data,
just not an exact string match) but left them unwired on principle:

- **Flanger**: `PreD` (Pre-Delay) -> bulk tag `PDly`; `Rate` -> bulk tag `Sped`.
- **Multi Chorus**: `PreD` -> `PDly`; `TriS` (Tri/Sine, a toggle) -> `Wave`; `Widt` (Width) -> `Wdth`.

Both are now wired up via a new `SlotConfig::bestEffortRawTagForKey()`, kept as a table SEPARATE
from `confirmedTagTable()` so `confirmedRawTagForKey()` still means "exact string match, no
ambiguity" for anything that needs that stricter guarantee - `bestEffortRawTagForKey()` checks the
confirmed table first, then falls back to this wider, explicitly-lower-confidence one.
`SignalChainComponent::updateBlockDataFromRig()` now calls `bestEffortRawTagForKey()` instead of
`confirmedRawTagForKey()`, so both toggle and knob reconciliation pick up the wider set.

**Tape Echo's remaining unmatched keys (`Dely`/`Fdbk`/`Mix `/`RecL`/`Head`/`ExpD`) are explicitly NOT
included** - unlike Flanger/Multi Chorus, no actual corresponding bulk tag was ever recorded for
these, only the fact that the CC key names don't match anything. Without knowing what they DO match,
even a name-similarity guess would be fabricated, not inferred - this stays parked until either a
fresh Tape Echo decode surfaces the real unmatched tags, or the user recalls them.

Build-verified (both targets compile/link, 71 test groups pass, 3 new covering
`bestEffortRawTagForKey()`'s fallback behaviour and confirming `confirmedRawTagForKey()` itself is
unaffected by the wider table).

**Twenty-sixth round (2026-07-28): started mapping the Signal Chain tab's "Input" block - added a
live CC sniffer, then used it to test True-Z.** `DiagnosticsComponent::onUnhandledMessage` now
specially recognizes a plain 3-byte MIDI Control Change and logs it as a clear "Incoming MIDI CC: CC
n = v (channel c)" line (alongside the existing raw hex dump, so nothing is lost if it turns out to
be something else) - a general-purpose tool for hunting undocumented CCs going forward, not specific
to this test.

Used it to test True-Z (the `PIGI` Rig Params field, already fully modeled in
`EffectDefinitions.cpp` with all 13 options, but flagged in `master-control-map.md` §5 as having "no
CC mapping ... sought"). Cycling through several different True-Z settings on real hardware produced
`F0 13 0B 0F 02 03 07 01 F7` every single time - message type `02` (ASYNCSET), command `03`, params
`[07, 01]`. This is the SAME command byte as the previously-documented "fourth round" mystery
(`F0 13 0B 0F 02 03 07 00 F7`, seen after a rig *select*, params `[07, 00]`) - initially looked
promising since only the last byte differed (`00` vs `01`), but since it stayed fixed at `01`
regardless of which True-Z option was actually selected, **this async message does not encode
True-Z's value** - it's unrelated to this particular control, whatever it actually represents (still
unresolved - see Open Items). Genuine negative result, not a dead end for the tool itself.

**Conclusion**: True-Z (and by the same reasoning, the other un-sought Rig Params fields - Input
Selector, Exp. Pedal Selector) still have no known live MIDI CC. `SignalChainComponent`'s "Input"
block now shows True-Z + Input Selector read-only, decoded straight from the Bulk Rig payload's
`rigGlobals` (`PIGI`/`WorB`) via a small new `rigParamOptionName()` helper - consistent with the
"never guess" approach used everywhere else: no live-editable control is shown, since none can
actually be wired up yet. Removed the long-standing restriction that Input/Output blocks couldn't be
clicked/selected at all, since Input now has real content worth showing when selected (Output still
falls through to the generic "not yet mapped" message, same as Amp/Cab/Volume/FX Loop).

See [project-overview.md](project-overview.md) "Prior Art Found" section for the full writeup.
Summary retained here for quick reference:

- Frame: `F0 13 0B 0F <msg-type> <command> [params...] F7`
- Message types: `SNDSET (0x00)`, `REQU (0x01)`, `ASYNCSET (0x02)`, `RESPOND (0x12)`
- Bulk payloads use a 7-bit encoding scheme (SysEx data bytes must be 0-127)
- No granular per-parameter SysEx write exists in ElevenHack — bulk rig rewrite only (this is why
  MIDI CC, not SysEx, is the live-tweak path)

## Open items

- [x] Confirm MIDI CC actually controls the unit (not just documented) — **confirmed (2026-07-24)**,
      see "sixth round" above (CC 69 engaged the real tuner).
- [x] Verify the "CC Setting N → positional param order" hypothesis against real hardware —
      **confirmed (2026-07-24)**, see "seventh round" above: CC 27/78/79 ("Distortion Setting
      1/2/3") controlled Overdrive/Tone/Level respectively on the loaded "Green JRC Disto" effect —
      all three knob positions confirmed, exactly matching `EffectDefinitions`' order.
- [ ] Determine what `CMD_TUNER_A` (`0x41`) represents — a real paired `ASYNCSET` alongside
      `CMD_TUNER` (`0x40`) on every tuner state change, not unused/dead as assumed. Possible generic
      "referenced command changed" wrapper - unconfirmed (see sixth round above).
- [ ] Resolve rig-switching mechanism (Program Change vs. CC vs. SysEx-only) — **narrowed
      (2026-07-24)**: the official manual documents CC32 as a plain Factory/User 2-value toggle
      ("User/Factory Bank Change"), not a general MIDI Bank Select MSB/LSB pair as originally
      hypothesized from observed traffic (fourth round) — but whether Program Change (or this Bank
      Change CC) alone can *drive* a rig switch, vs. only the SysEx `CMD_CURR_RIG_NUM` write, is
      still untested.
- [x] Resolve the CC119/"FX1 Setting 9" duplicate — **resolved (2026-07-24)**: a newer manual
      revision's per-effect CC breakdown shows CC119 used simultaneously with CC5 in the same
      effect's own CC list (Multi-Chorus's FX1 mapping) — it's a real, distinct CC; the "duplicate"
      in the generic table was a transcription error in that source, not a hardware quirk. See
      [eleven-rack-user-guide-chapter9-midi-cc-notes.md](samples/eleven-rack-user-guide-chapter9-midi-cc-notes.md).
- [ ] Cabinet/mic-position mapping still not found in either source
- [ ] Rig Description's per-tuple middle byte appears to be a monotonic counter assigned on each
      rig (re)load, not a stable identity — confirm with repeated same-state queries (see fourth
      round above)
- [ ] Determine what the unhandled `ASYNCSET` command `0x03` (`CMD_SAVE_RIG`'s command byte, but in
      an async context) actually represents — arrives alongside `CMD_CURR_RIG_NUM` after a rig
      select; ElevenHack doesn't handle this case either. **Partial negative result (2026-07-28,
      "twenty-sixth round")**: it does NOT track True-Z's value - cycling through several True-Z
      settings produced the identical `[07, 01]` params every time. Whatever `0x03` really means,
      it isn't "a Rig Params field changed."
- [x] Check whether other single-value parameters (beyond Main Volume) were also ported with the
      same unsigned-instead-of-signed mistake `encodeValue` had — **resolved (2026-07-24)**:
      audited every caller of ElevenHack's `ParseUtils.byteToEncodedInt`/`SysEx.buildSetEncoded01`
      in the Java source; Main Volume is the *only* parameter that ever used this encoding scheme.
      Every other single-value command uses plain unencoded bytes and is naturally non-negative
      (tuner on/off, bank/rig numbers), or isn't sent per-parameter at all (per-effect knobs only
      round-trip through full-rig bulk transfers in ElevenHack, never individually).
- [ ] Verify the "CC 'Setting N' maps positionally" hypothesis for **non-knob** params (toggles,
      selectors) against real hardware — still knob-only confirmed (Distortion). However, the
      Chorus/Vibrato and Vibe Phaser orderings now used in the Mod slot (`SlotConfig`/
      `SlotParamsPanel`) are no longer a guess extending that hypothesis - they're sourced directly
      from the official manual's own per-effect CC breakdown (see above), a stronger basis than
      before, just not yet hardware-confirmed.
- [ ] Wah, Mod, and Reverb slots (added/expanded 2026-07-24, now on the Signal Chain tab) are not
      yet hardware-tested — only Distortion has been confirmed so far (see implementation-plan.md
      Milestone 5).
- [ ] **New (2026-07-24)**: the official manual lists at least 31 distinct amp model names with
      real tone-knob CCs, vs. `EffectDefinitions::ampModelOptions()`'s 16 (ported from ElevenHack) —
      several names don't obviously match anything in our list (Black SR, Black Mini, J45, MS-30,
      RB01b...). Needs dedicated reconciliation - likely a real hardware capture of the Amp/Cab
      parameter value - before an Amp tone-knob editor can be built. See the reference doc above.
- [x] Real per-knob Delay data existed in the official manual (BBD/Dyn/Tape Echo) but its printed
      order didn't follow ascending Setting-N/CC order the way every other effect category does —
      **resolved (2026-07-24)**: reconstructed the true order by looking up each named param's CC
      against the confirmed generic "Delay Setting N" table. BBD Delay and Tape Echo are now fully
      modeled; see "fifteenth round" above.
- [x] Dyn Delay's "Mode" selector (Mono/Stereo/Cross/Pong) had no CC-range breakdown anywhere in
      the manual, unlike Roto Speaker's/Eleven SR's selectors, blocking the rest of that effect's
      real params — **resolved (2026-07-24)**: hardware-swept CC 87 at 0/42/85/127, confirmed
      Mono/Stereo/Cross/Pong in that exact order. Dyn Delay is now fully modeled (Ratio, Hi-Cut,
      Lo-Cut, Width, Em Rate, Em Feedback, Em Mix all added) — see "seventeenth round" above. The
      exact boundaries between the 4 tested points are still not independently confirmed.
- [x] "Expanded Delay" (BBD Delay/Tape Echo, CC 49/Setting8) had a confirmed CC but no Chapter 3
      description anywhere, so its type was unconfirmed — **resolved (2026-07-24)**: confirmed an
      on/off switch, not a knob. Fixed in `EffectDefinitions.cpp`; also hardware-confirmed working
      end-to-end on Tape Echo.
- [ ] All three delay effects (BBD Delay, Tape Echo, Dyn Delay) have a "Fine" on/off control with
      no confirmed CC anywhere in the manual — CC 59 tested and **ruled out** (2026-07-24; it's
      actually Dyn Delay's own real "Env Mod Rate", never a clean guess to begin with). No further
      hypothesis - may have its own dedicated CC outside this numbering scheme entirely, or may not
      be MIDI-addressable at all (described as toggled by a physical "SW2" switch in the manual).
- [ ] BBD Delay and Dyn Delay's individual knobs aren't independently hardware-tested yet - only
      Tape Echo has been confirmed control-by-control (plus Dyn Delay's own Mode sweep).
- [x] FX1/FX2 were blocked on not knowing which effect family a given rig assigns to those flexible
      slots — **resolved (2026-07-24)**: user checked the real unit and found FX1/FX2 only ever
      host a fixed effect list (the 6 Mod-slot effects, plus Graphic EQ/Para EQ/Gray Compressor/
      Dyn3 Compressor), not an open-ended assignment. Both slots now added (originally to
      `EffectEditorComponent`, since removed - see "eighteenth round" above and
      implementation-plan.md). Vibe Phaser is now included too
      (added "nineteenth round") using its Mod-slot CCs extrapolated, not directly documented in
      the manual for this context. None of FX1/FX2 is hardware-tested yet.
- [ ] "Fine" being present on all three delay types was the trigger for re-checking Multi Chorus's
      Width/Lo Cut order via FX1's CC table, which turned out to be swapped from what was
      originally coded for the Mod slot — fixed, but not independently hardware-retested since.
- [ ] Para EQ has 2 real controls the manual never documents at all — "L Type" and "H Type"
      (confirmed on real hardware 2026-07-24 - see "twentieth round" above). Neither has a known
      CC; the one candidate tested (CC 60/Setting3) was ruled out for both. Deferred - not blocking
      anything else in FX1/FX2.
- [x] Decode the Bulk Rig payload's byte structure — **resolved (2026-07-26)**, see "twenty-first
      round" above and `Source/Rack/BulkRigParser.h`/`.cpp`. The header/TOC/per-slot section format
      is now fully decoded and shipped. **Mostly resolved since** (see
      docs/implementation-plan.md "Not yet scheduled / parked" for the full breakdown): the TOC's
      `effectId`/`category` numbering was reconciled against `EffectDefinitions`'s own IDs
      (2026-07-26/27), raw field tags were mapped onto `ParamDefinition::key` for confirmed/
      best-effort matches (`SlotConfig::confirmedRawTagForKey()`/`bestEffortRawTagForKey()`), and
      the signed-32-bit-to-0-127-CC-scale calibration was done for knob-kind params
      (`SlotConfig::knobRawToCcValue()`) - all wired into `SlotParamsPanel`/`SignalChainComponent`.
      Selector-kind params (`Sync`, `Type`, `sld6`, ...) remain explicitly unresolved - confirmed NOT
      a single uniform formula there.
