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
params are added in ElevenHack's per-effect-type builder**. Not yet verified against hardware.

| CC# | Parameter | Notes |
|-----|-----------|-------|
| 3 | Amp Setting 8 | |
| 4 | Wah Pedal | |
| 5 | FX1 Setting 9 | |
| 7 | Volume Pedal | |
| 9 | FX1 Setting 10 | |
| 10 | Amp Setting 6 | |
| 11 | Multi-FX | |
| 12 | FX1 Setting 11 | |
| 13 | Amp Setting 1 | |
| 14 | Amp Setting 2 | |
| 15 | Amp Setting 3 | |
| 16 | Amp Setting 4 | |
| 17 | Rig Volume | |
| 18 | Reverb Setting 1 | |
| 19 | FX Loop Send | |
| 20 | FX1 Setting 1 | |
| 21 | Amp Setting 5 | |
| 22 | Amp Setting 12 | |
| 23 | Amp Setting 11 | |
| 24 | Amp Setting 10 | |
| 25 | Distortion On/Off | 0-63=Off; 64-127=On |
| 26 | FX1 Setting 12 | |
| 27 | Distortion Setting 1 | |
| 28 | Delay On/Off | 0-63=Off; 64-127=On |
| 29 | FX1 Setting 13 | |
| 30 | FX1 Setting 14 | |
| 31 | Delay Setting 14 | |
| 33 | Delay Setting 2 | |
| 34 | Delay Setting 6 | |
| 35 | Delay Setting 3 | |
| 36 | Reverb On/Off | 0-63=Off; 64-127=On |
| 37 | FX2 Setting 8 | |
| 38 | Reverb Setting 2 | |
| 39 | Reverb Setting 4 | |
| 40 | Reverb Setting 3 | |
| 41 | Reverb Setting 6 | |
| 42 | FX1 Setting 2 | |
| 43 | Wah On/Off | 0-63=Off; 64-127=On |
| 44 | Amp Setting 13 | |
| 45 | Amp Setting 14 | |
| 46 | FX2 Setting 9 | |
| 47 | FX2 Setting 10 | |
| 48 | Delay Setting 7 | |
| 49 | Delay Setting 8 | |
| 50 | Modulation On/Off | 0-63=Off; 64-127=On |
| 51 | Modulation Setting 6 | |
| 52 | Modulation Setting 2 | |
| 53 | Modulation Setting 3 | |
| 54 | Modulation Setting 4 | |
| 55 | Delay Setting 9 | |
| 56 | Modulation Setting 7 | |
| 57 | Modulation Setting 5 | |
| 58 | FX2 Setting 11 | |
| 59 | Delay Setting 10 | |
| 60 | FX1 Setting 3 | |
| 61 | Modulation Setting 1 | |
| 62 | Delay Setting 1 | |
| 63 | FX1 On/Off | 0-63=Off; 64-127=On |
| 64 | Tap Tempo | 64-127 = a tap |
| 69 | Tuner On/Off | 0-63=Off; 64-127=On |
| 70 | FX2 Setting 14 | |
| 71 | Cab Sim On/Off | 0-63=Off; 64-127=On |
| 72 | Delay Setting 11 | |
| 73 | Delay Setting 12 | |
| 74 | Delay Setting 13 | |
| 75 | Volume Pedal On/Off | 0-63=Off; 64-127=On |
| 76 | Reverb Setting 5 | |
| 77 | FX1 Setting 4 | |
| 78 | Distortion Setting 2 | |
| 79 | Distortion Setting 3 | |
| 80 | Distortion Setting 4 | |
| 81 | Distortion Setting 5 | |
| 82 | Distortion Setting 6 | |
| 83 | Distortion Setting 7 | |
| 84 | Amp Setting 9 | |
| 85 | Delay Setting 4 | |
| 86 | FX2 On/Off | 0-63=Off; 64-127=On |
| 87 | Delay Setting 5 | |
| 88 | FX Loop Mix | |
| 96 | FX2 Setting 4 | |
| 97 | FX2 Setting 5 | |
| 98 | FX2 Setting 6 | |
| 99 | FX2 Setting 7 | |
| 101 | Knob 1 | (front-panel physical knob, not effect-specific) |
| 102 | Knob 2 | |
| 103 | Knob 3 | |
| 104 | Knob 4 | |
| 105 | Knob 5 | |
| 106 | Knob 6 | |
| 107 | FX Loop On/Off | 0-63=Off; 64-127=On |
| 108 | FX Loop Return | |
| 109 | FX2 Setting 12 | |
| 110 | FX2 Setting 13 | |
| 111 | Amp On/Off | 0-63=Off; 64-127=On |
| 112 | Amp Setting 7 | |
| 113 | FX2 Setting 1 | |
| 114 | FX2 Setting 2 | |
| 115 | FX2 Setting 3 | |
| 116 | FX1 Setting 5 | |
| 117 | FX1 Setting 6 | |
| 118 | FX1 Setting 7 | |
| 119 | FX1 Setting 9 | *(sic — manual repeats "FX1 Setting 9" here as well as at CC5; possible OCR/manual error, needs checking against a second copy of the manual or hardware)* |

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

## SysEx protocol (unofficial — from ElevenHack, not yet hardware-validated)

See [project-overview.md](project-overview.md) "Prior Art Found" section for the full writeup.
Summary retained here for quick reference:

- Frame: `F0 13 0B 0F <msg-type> <command> [params...] F7`
- Message types: `SNDSET (0x00)`, `REQU (0x01)`, `ASYNCSET (0x02)`, `RESPOND (0x12)`
- Bulk payloads use a 7-bit encoding scheme (SysEx data bytes must be 0-127)
- No granular per-parameter SysEx write exists in ElevenHack — bulk rig rewrite only (this is why
  MIDI CC, not SysEx, is the live-tweak path)

## Open items

- [ ] Verify the "CC Setting N → positional param order" hypothesis against real hardware
- [ ] Resolve rig-switching mechanism (Program Change vs. CC vs. SysEx-only)
- [ ] Resolve the CC119/"FX1 Setting 9" duplicate — likely a manual error, confirm against a second
      source or hardware behavior
- [ ] Cabinet/mic-position mapping still not found in either source
