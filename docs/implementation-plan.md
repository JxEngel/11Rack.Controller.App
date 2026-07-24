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
- [x] 7-bit data encoding/decoding scheme for bulk payloads — **done (2026-07-24)**: verified
      numerically (not by hand) against the real captured bulk-rig payload, exact round-trip match.
      See `Source/Rack/SevenBitCodec.{h,cpp}` under Milestone 3 for the ported, tested code.
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
- [x] Add Apache-2.0 attribution/NOTICE for ElevenHack-derived protocol knowledge, per its license
      terms (see Licensing Note in project-overview.md) — **Done (2026-07-24)**: root `NOTICE` file
      plus `docs/third-party-licenses/ElevenHack-APACHE-2.0-LICENSE.txt` (the full Apache-2.0 text,
      reproduced from ElevenHack's own `LICENCE.txt`, excluding license blocks for ElevenHack's own
      dependencies we didn't use — HyperSQL, MigLayout, JUnit).
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

Structured as three sub-layers, not one class — see the architecture discussion in
[project-overview.md](project-overview.md) (codec → transport → service). Every source file below
gets its matching test file created in the same change, not added afterward — see
[development-guide.md](development-guide.md) "Running Tests" for how to run them, and the
`RackControllerTests`/CTest infrastructure (already scaffolded, 2026-07-24) for how they're wired up.

**Test infrastructure**
- [x] `RackControllerTests` console app target + CTest wiring — **done (2026-07-24)**, using
      JUCE's built-in `juce::UnitTest`/`UnitTestRunner` (no extra test framework dependency). The
      placeholder smoke test has been removed now that real tests exist below.
- [x] Developer documentation for running tests — **done (2026-07-24)**, see
      [development-guide.md](development-guide.md) "Running Tests".

**Codec layer** (pure byte-level logic — no MIDI I/O, no UI; unit-testable against the byte
sequences already captured in [protocol-spec.md](protocol-spec.md) and
[samples/](samples/bulk-rig-sample-2026-07-24.txt))
- [x] `SysExFrame.{h,cpp}` + `SysExFrameTests.cpp` — **done (2026-07-24)**: frame build/parse
      (from `SysEx.java`) — vendor/device/model IDs, message type, command byte, param extraction,
      `extractString`. 9 tests, all built directly from real hardware-captured byte sequences
      (Effect Count, Main Volume, Rig Name, the confirmed B4 rig-number reply, the confirmed
      Select-Rig write, plus malformed-frame rejection cases). **Build-verified (2026-07-24)**: all
      9 pass in a real build via `RackControllerTests`/CTest.
- [x] `SevenBitCodec.{h,cpp}` + `SevenBitCodecTests.cpp` — **done (2026-07-24)**: the 5-byte
      encoded-int scheme (verified against the real Main Volume=127 round-trip) and the general
      7-bit pack/unpack (from `ParseUtils`/`SysEx.extractFrom7bits`/`encodeTo7bits`). Both were
      verified numerically in Python *before* porting to C++, including a full exact-byte-match
      round-trip against the real captured bulk-rig payload — decoding it, dropping the last byte,
      and re-encoding reproduces the original captured wire bytes exactly. Discovered and documented
      a real subtlety: `decodeFrom7Bits` always returns exactly one more byte than the true data
      length (a deterministic "remainder" from the bit-packing tail) — something else (the `.tfx`
      structure) has to know the true length. **Build-verified (2026-07-24)**: all 5 pass.
      - **Bug fix (2026-07-24)**: `encodeValue` was typed `uint8_t` (unsigned) instead of the
        signed `int8_t` ElevenHack's original Java `byte` actually uses — found via a real hardware
        mismatch on Main Volume (see "fifth round" in [protocol-spec.md](protocol-spec.md)). Fixed
        to take `int8_t`, replicating Java's exact sign-extend-then-unsigned-shift semantics for
        both the special-case check and the general formula (not a naive 8-bit shift, which
        silently mishandles negative input differently). 4 new tests added for negative-value
        encode/decode, including a clean exact round-trip case and confirming the special case
        doesn't misfire for negative input.
- [x] `EffectDefinitions.{h,cpp}` + `EffectDefinitionsTests.cpp` — **done (2026-07-24)**: full
      effect/parameter registry ported from `Effect.mBuildEffect()`/`EffectAmpCab.java` — 52
      specific effect-instance IDs across ~20 effect families, plus the 16 known Amp/Cab models and
      the `EffectClass` (16-slot signal-chain category) enum. Not a line-by-line Java port — a
      strongly-typed data model (`EffectDefinition`/`ParamDefinition`/`SelectOption`). 8 tests,
      including a direct tie to real hardware data: `WAH_BLACK` (effect ID 55 = `0x37`) is the one
      effect ID confirmed to appear in the captured Rig Description reply. Known gaps, faithfully
      carried over from ElevenHack itself rather than invented by this port:
      - Many effect families (Fx Loop, Compressors, Para EQ, Vibe Phaser, Spring/Stereo Reverb,
        BBD/Tape/Dyn Delay, Multi Chorus, Flanger, Roto Speaker) were only ever identified by name
        in ElevenHack — their real per-knob parameters were never decoded. Marked
        `isFullyKnown = false` rather than silently presented as "no parameters."
      - Amp/Cab's display name is dynamic in ElevenHack (depends on the currently selected amp
        model) — represented here with a generic "Amp/Cab" placeholder name instead.
      - The large-32-bit-value alternate encoding of the Amp/Cab selector (seen alongside the small
        0-15 index in ElevenHack's source) is not modeled — needs a real hardware capture of an
        Amp/Cab parameter value to know which representation actually appears on the wire.
      **Build-verified (2026-07-24)**: 1 of 8 tests initially failed on a param-count assertion —
      turned out to be a miscount in the *test* itself (expected 16, should've been 17: Bypass +
      `mCreateRigParams()`'s 16 items), not a bug in the ported data. Fixed; all 8 pass now.
- [ ] `.tfx` file format parser/writer (from `tfx/TfxParser.java`) + tests, for import/export
      compatibility — lower priority than the live-protocol pieces above

**Transport layer**
- [x] `MidiTransport.{h,cpp}` + `MidiTransportTests.cpp` — **done (2026-07-24)**: thin wrapper
      around `juce::MidiInput`/`MidiOutput` (the only place that touches JUCE's MIDI API directly),
      open/close/send/receive raw bytes, with `WeakReference`-based safe thread-hopping for the
      receive callback (not a `juce::Component`, so `Component::SafePointer` wasn't available -
      `WeakReference` is JUCE's equivalent for non-Component classes). As anticipated, most of its
      real behavior can't be unit-tested without physical hardware or a virtual/loopback MIDI port
      (neither available in an automated run) — the 7 tests cover only the hardware-independent
      edge cases (invalid device identifiers, operations with nothing open, idempotent close).
      Real send/receive is verified by hand against the actual unit instead — see
      [protocol-spec.md](protocol-spec.md)'s hardware validation log. **Build-verified (2026-07-24)**.

**Service layer**
- [x] `RackController.{h,cpp}` + `RackControllerTests.cpp` — **done (2026-07-24)**: the facade the
      UI will call. Owns a `MidiTransport`, uses `SysExFrame`/`SevenBitCodec` internally, exposes:
      - Queries confirmed working against real hardware: `requestEffectCount()`,
        `requestMainVolume()`, `requestCurrentRig()`, `requestRigName()`, `requestRigDescription()`,
        `requestEffectDescription()`, `requestBulkRig()`
      - `selectRig(RigId)` — the one write confirmed working against real hardware
      - `saveRig()`, `setRigName()`, `setMainVolume()`, `setTunerOn()` — ported from ElevenHack's
        `ElevenTransmitter` for API completeness, **NOT hardware-validated**; do not wire any of
        these to a UI control without a deliberate decision to test them first (`saveRig`
        specifically overwrites stored data)
      - A `Listener` interface (`onEffectCountReceived`, `onMainVolumeReceived`,
        `onCurrentRigReceived`, `onRigNameReceived`, `onEffectDescriptionReceived`,
        `onTunerStateReceived`, plus raw-payload callbacks for Rig Description and Bulk Rig since
        those aren't fully decoded yet, and `onUnhandledMessage` for anything unrecognized)
      - Simplification vs. `ElevenReceiver`: dispatches identically for `RESPOND` vs `ASYNCSET`
        (ElevenHack's own two switch blocks call the same handlers anyway) — documented in the
        header as an intentional, easily-revisited choice, not an oversight
      - 9 tests drive the real parsing/dispatch pipeline directly (via a friend-class test seam,
        `RackController::handleIncomingBytes`) using every real captured reply from this session:
        Effect Count, Main Volume, Current Rig Number (the confirmed B4), Rig Name ("Big Blue"),
        Effect Description ("Eleven"/"DigiElvnELVu"), Rig Description (raw payload), plus
        malformed-input and not-connected edge cases
      - Covers the SysEx-based write path (`selectRig`, `setMainVolume`, etc.) from the Milestone 0
        CC-chart research; MIDI CC sending for live single-parameter tweaks is not yet implemented
        here — still open
      - **Build-verified (2026-07-24)**: all tests pass.

**Integration**
- [x] `MainComponent` refactored (2026-07-24) to use `RackController` instead of building raw
      SysEx bytes itself: the "known command" picker now calls named `RackController` methods
      directly, "Select Rig" calls `selectRig()`, and incoming messages are handled via
      `RackController::Listener` overrides that log decoded, human-readable info (e.g. "Effect
      Count: 65") instead of raw hex — raw hex is now only shown for `onUnhandledMessage` (anything
      that isn't a recognized Eleven Rack frame, e.g. the Universal Identity Reply). Also added
      `RackController::sendRaw()` as a narrow escape hatch for that one generic, non-Eleven-Rack
      probe. Two small deliberate behavior changes from before: (1) changing either MIDI
      input/output dropdown now reconnects both together (matches `RackController::connect()`'s
      combined API) rather than being fully independent; (2) clicking "Refresh Devices" now
      actually disconnects rather than just resetting the dropdown UI while leaving a stale
      connection open. **Build-verified against real hardware (2026-07-24)**: every previously
      confirmed decode reproduced identically through the real transport path, plus new findings —
      see [protocol-spec.md](protocol-spec.md) "fourth round" for the Rig Description
      tuple-hypothesis confirmation, the newly-discovered MIDI Bank Select CC0/CC32 messages
      accompanying a rig select, and the unhandled async command `0x03` mystery.

## Milestone 4 — Hardware validation

Nothing above is trusted until confirmed against the real unit:

- [x] Vendor/device ID bytes confirmed live (2026-07-24) via Universal SysEx Identity Request —
      see [protocol-spec.md](protocol-spec.md#hardware-validation-log). This validates the
      addressing bytes only, not yet any Eleven-Rack-specific command.
- [ ] Identify what the OS-enumerated 2 MIDI inputs / 3 MIDI outputs (all named "Eleven Rack")
      actually correspond to — which port is the control/SysEx port
- [x] Send known ElevenHack-derived messages and confirm replies match expected structure —
      **Done (2026-07-24)**: effect count, main volume, current rig number, rig name, rig
      description, effect description, and bulk rig all tested live and decoded. See
      [protocol-spec.md](protocol-spec.md#hardware-validation-log) "second round" for full detail
      and the new open items it raised (rig description tuple structure, effect-index-0 mystery).
- [ ] Confirm the init/handshake sequence works end-to-end against current firmware — individual
      commands from the sequence now validated in isolation, but the full sequence hasn't been run
      end-to-end yet
- [ ] Confirm a full rig bulk-read round-trips correctly (read, re-encode, compare) — have a real
      bulk-read sample now ([docs/samples/bulk-rig-sample-2026-07-24.txt](samples/bulk-rig-sample-2026-07-24.txt)),
      but haven't yet attempted re-encoding/writing it back
- [ ] Verify checksum/CRC handling for bulk transfers (unresolved in ElevenHack itself)
- [x] Confirm write path: send a parameter change, verify the unit actually applies it —
      **Confirmed (2026-07-24)**: the "Select Rig" control (`SNDSET CMD_CURR_RIG_NUM`,
      `F0 13 0B 0F 00 02 <bank> <rig> F7`) successfully switched the unit's active rig, visible on
      its own front-panel display — a real write, not just an accepted-looking reply. This is the
      first confirmed write in either direction (SysEx or MIDI CC), and a major derisking point:
      both read and write now work end-to-end against real hardware using ElevenHack-derived frames.

## Milestone 5 — Editor UI

- [x] **App shell restructured (2026-07-24)** to support multiple real feature screens: device
      connection controls (input/output pickers, refresh, status) moved to a new top-level
      `MainComponent`, which owns the single `RackController` and hosts feature components as tabs
      via `juce::TabbedComponent`. The old `MainComponent` content (known-command picker, Select
      Rig, raw-send diagnostics) became `DiagnosticsComponent` — kept as a "Diagnostics" tab since
      it's still useful for direct protocol testing, not replaced.
- [x] **Rig/preset browser — first slice (2026-07-24)**: `RigBrowserComponent`, a "Rig Browser" tab
      listing all 208 rig slots (both banks x 104 rigs). Required a new `RackController` capability
      first: `requestAllRigNames()` sequentially requests every rig name (one at a time, waiting
      for each reply before the next — mirrors ElevenHack's `ElevenInit`, not a 208-request burst,
      which is untested/risky on real hardware), with `cancelRigNameFetch()` as a manual escape
      hatch and `onRigNameFetchComplete()` marking the end. 6 new `RackController` tests cover the
      sequencing logic directly (advance-on-match, ignore-on-mismatch, bank wrap-around,
      completion, cancellation) using the friend-access seam to jump to boundary cases rather than
      simulating all 208 steps.
      - Rig rows show a computed "A1"–"Z4" location label matching `ElevenRack.java`'s
        `m_rigLocToName` scheme (verified against the real hardware-confirmed "B4" = Bank 0, Rig 7)
      - Current rig is highlighted live via `onCurrentRigReceived`
      - Double-click a row to load it via `selectRig()`
      - **Not yet done**: load (via double-click) is there, but *save* and *rename* from this UI
        are not — those map to `saveRig()`/`setRigName()`, which are still unvalidated against
        hardware (see Milestone 4) and deliberately not wired to any control yet
      - **Testing note**: unlike the `Rack/` codec/service layer, this and future UI components
        aren't unit-tested the same way — JUCE `Component` painting/layout isn't practically
        covered by `juce::UnitTest` without much more test infrastructure (e.g. screenshot
        testing), which is out of scope for now. Verification here is manual, against real
        hardware, same as the app always has been.
      - **Build-verified against real hardware (2026-07-24)**: rig list populated correctly, and
        double-clicking a rig successfully loaded it onto the unit.
- [ ] Per-effect parameter editing screens (knobs/selectors/switches) driven by
      `EffectDefinitions` (Milestone 3)
- [ ] Live state sync: reflect hardware-originated changes (front-panel action) in the UI in real
      time — partially done already for the rig browser (current-rig highlight); still needed for
      per-parameter values once the editing screens exist
- [x] Tuner, main volume, and other rig-global controls — `RigGlobalsComponent` ("Globals" tab)
      added (2026-07-24): first hardware test of `setMainVolume()`/`setTunerOn()`, neither
      previously validated — **confirmed working (2026-07-24)** in their original button-based
      form (Set/Request buttons for volume, On/Off buttons for tuner).
      - **Main Volume reworked into live two-way sync** immediately after, on the user's request,
        to prove the pattern the future per-effect parameter controls will reuse: dragging the
        slider sends `setMainVolume()` on every change (`onValueChange`); a device-confirmed
        `onMainVolumeReceived()` moves the slider to match, using `juce::dontSendNotification` so
        the programmatic update doesn't loop back into another send. The separate "Set"/"Request"
        buttons and status label were removed as redundant once live sync replaced them.
      - **Live sync immediately surfaced a real bug** (2026-07-24): the slider's "0" displayed as
        the unit's own "5.0" (center of its 0.0-10.0 scale), not "0.0" — because
        `SevenBitCodec::encodeValue` had been typed `uint8_t` (unsigned) instead of the signed
        `int8_t` ElevenHack's original Java actually uses, silently making the entire lower half of
        the real range unreachable. Fixed (see `SevenBitCodec.{h,cpp}` above and
        [protocol-spec.md](protocol-spec.md) "fifth round"); the slider now shows the unit's real
        0.0-10.0 scale directly, converting to/from the raw signed wire value internally. Confirmed
        against real hardware at two points (raw 0 = "5.0", raw 127 = "10.0"); the negative end
        (raw -127 = "0.0"?) is not yet independently confirmed.
      - Tuner has no state query in the protocol, so its status label only ever reflects a real
        device-confirmed `onTunerStateReceived` callback, never an optimistic guess — two explicit
        buttons (On/Off) are used instead of one toggle, for the same reason (nothing to reliably
        toggle *from*).

## Not yet scheduled / parked

- Python protocol-discovery logger (`mido`/`python-rtmidi`) — only needed now for gaps ElevenHack
  and the official CC chart don't cover. May end up largely unnecessary given how much Milestone 0-1
  already resolves from prior art.
