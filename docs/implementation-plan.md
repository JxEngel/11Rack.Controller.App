# Implementation Plan

Trackable task breakdown for actually building the Eleven Rack Controller App. Companion to
[docs/project-overview.md](project-overview.md), which holds the reasoning/decisions — this doc is
just the checklist, kept in sync as we go. Check items off as they're actually done, not planned.

## Milestone 0 — Open research (blocking some later tasks)

- [x] Find the official Eleven Rack User Guide MIDI CC chart (pages 95-98, CC 3-119) and determine
      whether live/real-time parameter control is plain MIDI CC, separate from the bulk-SysEx rig
      save/load path found in ElevenHack. **Done (2026-07-23)** — confirmed yes, two separate
      mechanisms. Full table now in [docs/protocol-spec.md](protocol-spec.md), sourced from
      archive.org's copy of the manual (`manualzilla-id-6921695`).
- [x] Confirm the Eleven Rack enumerates as a USB Audio/MIDI class-compliant device — **Done
      (2026-07-24)**: confirmed via the Milestone 2 skeleton app (plain OS MIDI ports, no special
      driver, successful Identity Request/Reply round-trip). See
      [protocol-spec.md](protocol-spec.md#hardware-validation-log).
- [ ] Determine cabinet/mic-position parameter mapping — not found in ElevenHack's `EffectAmpCab`,
      and not in the official CC chart either (Amp Setting 1-14 covers the amp side only).
- [ ] Verify the "CC 'Setting N' maps positionally to ElevenHack's per-effect knob-add order"
      hypothesis against real hardware (see protocol-spec.md Open Items).
- [ ] Resolve how rig switching works during play — Program Change, a specific CC, or SysEx-only
      (`CMD_CURR_RIG_NUM`). The manual's CC chapter never mentions Program Change; forum threads
      discuss MIDI rig-switching, so this needs direct hardware testing, not just reading.
- [ ] Resolve the apparent CC119 duplicate ("FX1 Setting 9" listed at both CC5 and CC119) — likely
      an error in one copy of the manual; check a second copy or hardware behavior.

## Milestone 1 — Protocol spec doc (from ElevenHack + official CC chart)

`docs/protocol-spec.md` created (2026-07-23) with the MIDI CC table and SysEx summary. Remaining:

- [x] SysEx frame structure, vendor/device/model IDs, message types (`SNDSET`/`REQU`/`ASYNCSET`/
      `RESPOND`)
- [x] Full command ID list (bulk tfx get/set, rig number, save rig, rig name get/set, effect
      description, rig description, effect count, main volume, tuner)
- [x] Official MIDI CC table (real-time control) — separate from the SysEx bulk-transfer path
- [ ] 7-bit data encoding/decoding scheme for bulk payloads — written up in prose in
      project-overview.md; still needs a precise worked example (encode/decode a sample byte
      sequence by hand) before treating it as fully understood
- [ ] Device init/handshake sequence — documented in project-overview.md, not yet copied into
      protocol-spec.md's structured format
- [ ] Full effect/parameter table (every effect type's knobs/switches/selectors, ranges, and value
      encodings — including the two different numeric encodings for the same amp-model list found
      in `EffectAmpCab`, still not fully reconciled) — not yet moved into protocol-spec.md
- [x] Explicitly flag unresolved/unverified items carried over from ElevenHack: unfinished checksum
      handling, the `loadRigStream` command-byte discrepancy, missing cab/mic mapping

## Milestone 2 — JUCE project skeleton

- [x] Scaffold a JUCE project (standalone app target) in this repo — CMake + JUCE via
      `FetchContent` (no manual JUCE install/submodule needed), `Source/Main.cpp` +
      `Source/MainComponent.{h,cpp}`. Not yet build-verified in a sandbox without `cmake`/MSVC on
      PATH — first real build happens per [docs/development-guide.md](development-guide.md).
- [ ] Add Apache-2.0 attribution/NOTICE for ElevenHack-derived protocol knowledge, per its license
      terms (see Licensing Note in project-overview.md) — not yet added; do this once actual
      ElevenHack-derived code (not just protocol knowledge) lands in Milestone 3.
- [x] Basic MIDI I/O: list devices, open a connection, send/receive raw SysEx — implemented as a
      small MIDI monitor (device pickers, hex log of incoming traffic, a button to send a
      Universal SysEx Identity Request). This is the first real, testable artifact — point it at
      the actual Eleven Rack once built to validate the USB-MIDI-class-compliant assumption.
- [x] Known-command test buttons (2026-07-24) — added a picker of read-only (query-only)
      ElevenHack-derived SysEx commands (effect count, main volume, current rig number, rig name,
      rig description, effect description, bulk rig) plus a send button, for testing more of the
      reverse-engineered protocol against real hardware beyond the generic Identity Request. All
      REQU (query) commands only — no SET/write commands included, to avoid risking device state
      during exploratory testing.

## Milestone 3 — Interface/protocol layer (C++ port)

Porting the three ElevenHack pieces identified as directly relevant:

- [ ] `SysExMessage` builder/parser (from `SysEx.java`) — frame construction, 7-bit encode/decode
- [ ] Effect/parameter registry (from `Effect.mBuildEffect()`'s per-effect-type definitions) — a
      strongly-typed C++ model, not a Java port line-by-line
- [ ] Listener/dispatcher for `ASYNCSET` vs `RESPOND` messages (from `ElevenReceiver.parseMessage()`)
      — the "listen for hardware-originated changes" half of the API
- [ ] Transmitter methods (from `ElevenTransmitter`) — the "send updated values" half
- [ ] **Both write paths needed (resolved by Milestone 0 CC-chart research)**: a MIDI CC sender for
      live single-parameter tweaks, and a bulk SysEx rig writer for save/load — not an either/or
- [ ] `.tfx` file format parser/writer (from `tfx/TfxParser.java`) for import/export compatibility

## Milestone 4 — Hardware validation

Nothing above is trusted until confirmed against the real unit:

- [x] Vendor/device ID bytes confirmed live (2026-07-24) via Universal SysEx Identity Request —
      see [protocol-spec.md](protocol-spec.md#hardware-validation-log). This validates the
      addressing bytes only, not yet any Eleven-Rack-specific command.
- [ ] Identify what the OS-enumerated 2 MIDI inputs / 3 MIDI outputs (all named "Eleven Rack")
      actually correspond to — which port is the control/SysEx port
- [ ] Send known ElevenHack-derived messages (request effect count, request rig name, request
      current rig number) and confirm replies match expected structure
- [ ] Confirm the init/handshake sequence works end-to-end against current firmware
- [ ] Confirm a full rig bulk-read round-trips correctly (read, re-encode, compare)
- [ ] Verify checksum/CRC handling for bulk transfers (unresolved in ElevenHack itself)
- [ ] Confirm write path: send a parameter change, verify the unit actually applies it

## Milestone 5 — Editor UI

- [ ] Rig/preset browser (list, load, save, rename) — UI equivalent of ElevenHack's rig-loader,
      built fresh in JUCE
- [ ] Per-effect parameter editing screens (knobs/selectors/switches) driven by the Milestone 1
      effect/parameter table
- [ ] Live state sync: reflect hardware-originated changes (front-panel action) in the UI in real
      time, via the Milestone 3 listener
- [ ] Tuner, main volume, and other rig-global controls

## Not yet scheduled / parked

- Python protocol-discovery logger (`mido`/`python-rtmidi`) — only needed now for gaps ElevenHack
  and the official CC chart don't cover. May end up largely unnecessary given how much Milestone 0-1
  already resolves from prior art.
