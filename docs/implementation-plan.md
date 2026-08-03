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
- [x] Verify the "CC 'Setting N' maps positionally to ElevenHack's per-effect knob-add order"
      hypothesis against real hardware (see protocol-spec.md Open Items). **Test tool added
      (2026-07-24)**: a "MIDI CC (test)" section on the Diagnostics tab (CC# + Value spinners, Send
      button) sends a raw 3-byte Control Change via `RackController::sendRaw()`.
      - **First test confirmed (2026-07-24)**: CC 69 (Tuner On/Off) = 127 actually engaged the
        real unit's tuner — MIDI CC is a genuine, working live-control mechanism, not just
        documentation. Also surfaced that `CMD_TUNER_A` (`0x41`) is a real paired `ASYNCSET`
        alongside `CMD_TUNER`, not unused as assumed — see
        [protocol-spec.md](protocol-spec.md) "sixth round".
      - **Positional hypothesis confirmed (2026-07-24)**: with "Green JRC Disto" loaded (knob order
        `Driv`/"Overdrive", `Tone`, `Levl`), CC 27 ("Distortion Setting 1") controlled the Overdrive
        knob specifically — exactly the first knob in `EffectDefinitions`' order for that effect.
        See [protocol-spec.md](protocol-spec.md) "seventh round". This validates the core mechanism
        the entire per-effect parameter editing UI (Milestone 5) will depend on. "Setting 2"/
        "Setting 3" → 2nd/3rd knob not yet independently confirmed, only "Setting 1" tested so far.
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
        doesn't misfire for negative input. **Build-verified (2026-07-24)**: all pass.
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
- [x] **Replaced the manual MIDI input/output dropdowns with auto-connect (2026-07-28).**
      `MainComponent::connectToRack()` looks for a MIDI input and output whose name contains
      "Eleven Rack" and connects directly - confirmed with the user that in this app's own device
      lists, "Eleven Rack" only ever appears once per direction, so a name match is unambiguous (see
      the resolved Milestone 4 item above on the earlier "2 in / 3 out" raw-OS-port count). Runs
      automatically once at launch; the two dropdowns and the old "Refresh Devices" button are gone,
      replaced by a single "Reconnect" button for the manual-retry case (unit plugged in/powered on
      after launch). Build-verified (compiles/links); real-hardware auto-connect behavior itself
      still needs a manual check with the unit plugged in.

## Milestone 4 — Hardware validation

Nothing above is trusted until confirmed against the real unit:

- [x] Vendor/device ID bytes confirmed live (2026-07-24) via Universal SysEx Identity Request —
      see [protocol-spec.md](protocol-spec.md#hardware-validation-log). This validates the
      addressing bytes only, not yet any Eleven-Rack-specific command.
- [x] Identify what the OS-enumerated 2 MIDI inputs / 3 MIDI outputs (all named "Eleven Rack")
      actually correspond to — which port is the control/SysEx port. **Resolved in practice
      (2026-07-28)**: that "2 in / 3 out" count was from an earlier, separate raw-OS-port
      observation - confirmed directly against this app's own `MidiTransport::getAvailableInputs()`/
      `getAvailableOutputs()` lists, "Eleven Rack" only ever appears once per direction, so there's
      no real ambiguity to resolve for this app's purposes. `MainComponent` now auto-connects by
      name match instead of requiring a manual device picker - see `MainComponent::connectToRack()`.
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
- [x] Per-effect parameter editing screens (knobs/selectors/switches) driven by
      `EffectDefinitions` (Milestone 3) — **first slice added (2026-07-24)**:
      `EffectEditorComponent` ("Effect Editor" tab), scoped to the **Distortion slot only** at
      first — proves the mechanism confirmed in "seventh round" ([protocol-spec.md](protocol-spec.md))
      actually works as a live editor, not just a one-off test.
      - Pick which of the 5 known Distortion models is loaded, from a dropdown (defaults to
        "Green JRC Disto", matching the hardware test) — then a Bypass toggle (CC 25) and a slider
        per knob, positionally mapped to CC 27/78/79/80/81/82/83 ("Distortion Setting 1-7") per the
        confirmed hypothesis
      - New `RackController::sendMidiCc(ccNumber, value)` — a proper named method for the
        now-confirmed-real CC live-tweak mechanism, replacing the diagnostic-only `sendRaw` hack for
        this purpose. **Not unit-tested** (a deliberate, documented choice, not an oversight): the
        method is a trivial 3-byte construction with no way to observe outgoing bytes without new
        mock infrastructure, and it's already proven correct against real hardware twice (CC 69,
        CC 27) — stronger evidence than a synthetic test would give.
      - **Two real, deliberate limitations, not accidental gaps**: (1) no way to auto-detect which
        effect is actually loaded — that depends on the still-unresolved Rig Description structure,
        so you tell it yourself via the dropdown; (2) no live readback — CC has no query mechanism,
        so sliders reflect only what you've set from this screen, not the unit's true current state,
        until you move them.
      - **Build-verified against real hardware (2026-07-24)**: Bypass and all three knobs
        (Overdrive/Tone/Level) confirmed working correctly. This also fully closes out the "Setting
        2/3 map to the 2nd/3rd knob" open item from [protocol-spec.md](protocol-spec.md) — all
        three positions on the same effect landed exactly where the hypothesis predicted.
      - **Extended to a "Slot" picker with Wah and Mod (2026-07-24)** — but NOT uniformly to every
        slot, after checking exactly which effect families have real per-knob data in
        `EffectDefinitions` vs. name-only stubs:
        - **Wah** — one confirmed knob only (Position, "Wah Pedal" = CC 4) + Bypass (CC 43). The
          chart has no "Setting N" scheme for Wah at all (unlike every other slot), and the second
          knob (VxCr) has no known CC, so it's intentionally omitted rather than guessed.
        - **Mod** — limited to the two Mod-slot effects with real decoded parameters,
          Chorus/Vibrato and Orange Phaser; the others this slot can hold (Vibe Phaser, Multi
          Chorus, Flanger, Roto Speaker) are name-only in `EffectDefinitions` and are omitted. This
          is also the first time the positional "Setting N" mapping is applied to non-knob params
          (Chorus/Vibrato's Mode toggle and Sync selector) — **an untested extension** of the
          knob-only hypothesis confirmed for Distortion, flagged as such in the UI's note text, not
          presented as confirmed.
        - **Deliberately NOT built (at the time)**: Reverb and Delay (every known variant of both
          was entirely name-only in `EffectDefinitions` — ElevenHack never decoded a single real
          knob for either), Amp tone knobs (only a model selector exists; no Gain/Bass/Mid/Treble/
          etc., even though the CC chart implies 14 of them), and FX1/FX2 (`EffectDefinitions`
          doesn't record which effect families a rig actually assigns to those flexible slots, so
          we don't know which CC range would even apply to a given effect placed there).
        - **Not yet hardware-tested**: Wah and Mod are unverified against the real unit — only
          Distortion has been confirmed so far.
      - **A second, more detailed manual revision found (2026-07-24)** — see
        [protocol-spec.md](protocol-spec.md) "Second manual revision found" and
        [eleven-rack-user-guide-chapter9-midi-cc-notes.md](samples/eleven-rack-user-guide-chapter9-midi-cc-notes.md)
        — filled in real per-effect CC data ElevenHack never decoded, closing part of the Reverb
        gap above, and caught two real bugs before any hardware test could:
        - **Fixed**: Mod slot's Chorus/Vibrato had its Mode toggle in the wrong position (Setting 1
          instead of Setting 5) and Distortion's Tri Knob Disto ("Tri-Knob Fuzz") had the wrong
          knob order/labels (Sustain/Tone/Level instead of Volume/Sustain/Tone) — both corrected in
          `EffectDefinitions.cpp`, neither had been hardware-tested yet so nothing user-visible had
          shipped wrong, but both would have been wrong the first time someone tried them.
        - **Added**: Vibe Phaser to the Mod slot's dropdown (previously name-only, now has real
          manual-sourced params) and a new **Reverb** slot (Bypass/Mix/Decay/Tone, plus Pre-Delay
          for Stereo Reverb) — both `EffectDefinitions`-entries and `EffectEditorComponent` support
          updated together, same pattern as Distortion/Wah/Mod. Stereo Reverb's "Type" selector
          (25 named reverb types over uneven CC ranges) is deliberately not included — the
          range-to-name transcription wasn't confident enough to encode without a hardware check.
        - **Still not built**: Delay (real per-knob data now exists in the manual, but its print
          order doesn't match ascending Setting-N order the way every other category does, so the
          true order needs more careful reconstruction before it's safe to implement) and Amp tone
          knobs (blocked on a newly-found, bigger problem: the manual lists ~31 amp models against
          our 16, needing reconciliation - possibly a hardware capture - before any amp knob editor
          would be trustworthy). FX1/FX2 still blocked on the same slot-assignment-detection gap as
          before. See protocol-spec.md Open Items for all of the above.
        - Build-verified (compiles, all 52 test groups pass) but **not yet hardware-tested** — Wah,
          Mod, and Reverb all still need a real hardware pass; only Distortion has one so far.
      - **Chorus/Vibrato Sync bug found and fixed (2026-07-24)** — hardware report: "chorus/vibrato
        controls don't seem to work." Root cause: the `Sync` selector used ElevenHack's small
        0-13 bulk-SysEx-field index instead of the real live-CC encoding (a 0-127 byte bucketed
        into 14 ranges, per the manual's "FX Sync Setting Values" table) — sending the tiny index
        landed in/near the "Off" bucket regardless of the option picked. Fixed via a shared
        `ccSyncSelector()` helper, applied to both Chorus/Vibrato and Vibe Phaser. **Confirmed
        working on real hardware (2026-07-24)** after the fix — Bypass, Mode toggle, Chorus, Rate,
        Depth, and Sync all correct. (The Chorus knob briefly looked broken too, but that was the
        unit sitting in Vibrato mode from earlier testing, not a bug - no live readback means we
        don't know/force the unit's current mode.) **Chorus/Vibrato is now the second fully
        hardware-confirmed effect, after Distortion.**
      - **Mod slot expanded to all 6 real on-unit effects (2026-07-24)** — was missing half the
        unit's actual Mod-slot list (only Chorus/Vibrato, Orange Phaser, Vibe Phaser were present).
        Added, in the unit's own on-screen order: Flanger (fully known), Multi Chorus (fully known -
        needed the Mod slot's `settingCc` array extended from 7 to 9 entries for two CCs outside
        the standard "Setting 1-7" list), and Roto Speaker (partially known - Speed/Balance only,
        its Type selector omitted for the same low-confidence reason as Eleven SR's reverb Type).
        Not yet hardware-tested.
      - **Three more corrections from continued hardware testing (2026-07-24)**: Roto Speaker's
        Type selector (added, reverted after a wrong name-merge guess, then re-added correctly with
        the user's help reading the real on-unit list: 120, 122, 21H, "Foam Dr", Rover, Memphis,
        Wolf, Watery); Orange Phaser's Sync (was wrongly modeled as an on/off toggle - the manual's
        own description settles it as the same tempo-sync list as every other Sync control, fixed
        via `ccSyncSelector`); and Eleven SR's Type selector (added, using the same
        user-confirmed-list approach as Roto Speaker - the first two "names" were actually one,
        "Echo Room"). See [protocol-spec.md](protocol-spec.md) rounds eleven through thirteen for
        the full detail on each. All three option lists/orders are hardware-confirmed; the specific
        CC value chosen per option remains an unverified range-midpoint guess in every case.
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
        against real hardware at all three key points: raw 0 = "5.0" (center), raw 127 = "10.0"
        (max), and raw -127 = "0.0" (min) — **confirmed (2026-07-24)**.
      - Tuner has no state query in the protocol, so its status label only ever reflects a real
        device-confirmed `onTunerStateReceived` callback, never an optimistic guess — two explicit
        buttons (On/Off) are used instead of one toggle, for the same reason (nothing to reliably
        toggle *from*).
      - **Tap Tempo and FX Loop added (2026-07-24)** — the two remaining "easy" gaps identified when
        auditing what's still missing overall (Delay and FX1/FX2 are the harder remaining ones, see
        below). Neither fits `EffectEditorComponent`'s "pick which model is loaded" pattern, since
        there's exactly one Tap Tempo and one FX Loop on the whole unit, not several selectable
        models - both exposed directly here instead, using fixed CCs (not a "Setting N" positional
        scheme). Tap Tempo (CC 64) is a single momentary button - one click, one tap, nothing to
        read back or sync. FX Loop (Bypass=107, Send=19, Return=108, Mix=88) is Bypass + 3 knobs,
        newly promoted from name-only to real params in `EffectDefinitions.cpp`. Not yet
        hardware-tested.
      - **Still open (at the time)**: Delay (see below) and FX1/FX2 (still blocked on not knowing
        which effect family a given rig assigns to those flexible slots - would need the
        still-unresolved Rig Description decode, or a hardware capture, to unblock). See
        protocol-spec.md Open Items.
- [x] **Delay slot added (2026-07-24)** — `EffectEditorComponent`'s new "Delay" slot. The print
      order mismatch flagged above was resolved by looking up each named param's CC against the
      confirmed generic "Delay Setting N" table rather than reading the manual top-to-bottom.
      **BBD Delay and Tape Echo are fully modeled** (Bypass + all 9 real params each) — cross-
      referencing Chapter 3's plain-English descriptions (not just the Chapter 9 CC table) caught
      two params that would have been wrongly modeled as knobs otherwise: BBD Delay's "Mod" and
      both effects' Noise/Hiss switch are toggles, not continuous controls. **Dyn Delay is only
      partially modeled at first** (Bypass, Delay, Sync, Feedback, Mix) - its "Mode" selector
      (Mono/Stereo/Cross/Pong) had no CC-range data anywhere in the manual, and since the
      positional mechanism can't skip a slot, every later real param (L/R Ratio, Hi-Cut, Lo-Cut,
      Width, Env Mod Rate, EM Feedback, EM Mix) would have misaligned if Mode were guessed wrong -
      left out pending a real hardware sweep, same reasoning as Wah's VxCr and the original Reverb
      Type gaps.
      - **Completed the same day**: user confirmed "Expanded Delay" is a switch, not a knob (fixed)
        and hardware-tested Tape Echo control-by-control - all correct. Then swept Dyn Delay's Mode
        (CC 87) at 0/42/85/127 and confirmed Mono/Stereo/Cross/Pong in that exact order, unblocking
        the rest of Dyn Delay's real params (Ratio, Hi-Cut, Lo-Cut, Width, Em Rate, Em Feedback, Em
        Mix - all now added). **Dyn Delay is now fully modeled.** A separate hypothesis for the
        "Fine" on/off control present on all three delay types (CC 59) was tested and ruled out -
        its CC remains unknown. Build-verified (both targets, 53 test groups pass); Tape Echo is
        hardware-confirmed end-to-end, BBD Delay/Dyn Delay's individual knobs are not yet
        independently retested (Dyn Delay's Mode sweep is the only direct hardware data point).
- [x] **FX1/FX2 unblocked and added (2026-07-24)** — the core blocker (not knowing which effect
      family a rig assigns to those slots) turned out to be a wrong assumption: the user checked
      the real unit and found FX1/FX2 only ever host a **fixed** list - the 6 Mod-slot effects
      (C1 Chor/Vib, Multi Chorus, Flanger, Vibe Phaser, Orange Phaser, Roto Speaker) plus Graphic
      EQ, Para EQ, Gray Compressor, and Dyn3 Compressor.
      - Promoted 3 more previously name-only effects (Gray Compressor, Dyn3 Compressor - renamed
        from ElevenHack's internal "Dyn Compressor", Para EQ) using the same CC-to-Setting-N
        reconstruction as Delay. None of these (nor Graphic EQ) have a dedicated native slot on the
        unit at all - FX1/FX2 is the only place they're controllable. Para EQ has a genuine unused
        slot (Setting3) modeled as an explicit "(unused)" placeholder rather than skipped, since
        skipping would misalign every param after it.
      - **Caught another real bug in the process**: cross-checking Multi Chorus's already-shipped
        Mod-slot definition against FX1's independently-confirmed CC table revealed its "Width" and
        "Lo Cut" were coded in the wrong order - same "named print order != true positional order"
        pattern as Delay. Fixed (affects the Mod slot too, not just FX1/FX2).
      - Vibe Phaser is excluded from FX1/FX2's dropdown - the user confirmed the real unit can
        place it there, but the manual documents no FX1/FX2 CC data for it at all, unlike every
        other Mod-slot effect. Still fully usable in the Mod slot itself.
      - Added "FX1" and "FX2" slots to `EffectEditorComponent`, same pattern as every other slot,
        just wired to FX1/FX2's own bypass/Setting-N CCs.
      - **Also fixed while implementing this**: some effects (Para EQ has 14 real params) have more
        rows than reliably fit in the app window at any reasonable size - the per-effect param list
        is now inside a scrollable `juce::Viewport` instead of a fixed-height area, a real
        deterministic layout problem caught by doing the row-height math up front, not a guess.
      - Removed the now-fully-unused `addNameOnlyGroup()` helper and its test, since every effect
        ID in the registry is now at least partially known - a real milestone for how far the
        reverse-engineering has come this session.
      - Build-verified (both targets, 55 test groups pass). **None of FX1/FX2 is hardware-tested
        yet.**
- [x] **`BulkRigParser` — Bulk Rig payload structurally decoded (2026-07-26)**, closing the "what's
      missing" gap the parked item below described. The byte format turned out to be fully specified
      in ElevenHack's `tfx/TfxParser.java`/`Section.java`/`ParseUtils.java` - exactly the lead the
      parked item flagged as "most promising, not yet investigated." Read directly from ElevenHack's
      GitLab source (not paraphrased), then independently verified via a hand-written Python
      reimplementation run against both ElevenHack's own `bulkdump.bin` test fixture (decodes to rig
      name "Metal Rythm 1" with every TOC/section byte boundary matching exactly) and our own real
      hardware capture (`docs/samples/bulk-rig-sample-2026-07-24.txt`, decodes to rig name "JCM 800")
      before porting to C++, per this project's "prove it in Python first" discipline (same approach
      used for `SevenBitCodec`).
      - Format: 36-byte header (4-byte version + 4-byte header code, both raw/untransformed, + a
        28-byte rig name built from 7 individually byte-reversed 4-char quadlets, trimmed at the
        first NUL) → one TOC `Section` (id `'A'`) whose `byteSize` field (a multiple of 8) drives how
        many key/value quadlet pairs follow - not a hardcoded count - giving the rig-level globals
        (`RVol`, `Tmpo`, `FXc1`-`FXc4`, etc., plus the oddly-named-but-still-global `WorB`/`WstB`) and
        each of 10 signal-chain slots' (letters `'C'`-`'L'`) `effectId`/`category` via `Wor<letter>`/
        `Wst<letter>` lookups → 10 more `Section`s (ids `'C'`-`'L'`) read back-to-back with no
        padding/count/end-marker, each holding that slot's own raw field-tag → signed-32-bit-value
        map, terminated only by fewer than 12 bytes remaining.
      - Shipped as `Source/Rack/BulkRigParser.h`/`.cpp`, returning a `ParsedRig` (version, header
        code, rig name, rig-globals map, and all 10 `EffectSlot`s with `effectId`/`category`/raw
        `params`). Takes the full F0...F7 reply frame, validates it via `SysExFrame::parse` first.
      - 5 new tests in `BulkRigParserTests.cpp`, all against the real hardware capture (header/name,
        all 16 rig globals including the `WorB`/`WstB` exceptions, all 10 slots'
        `effectId`/`category`, per-slot param maps for a couple of slots, and 2 rejection cases) -
        build-verified (61 test groups pass, both targets compile; the`ElevenRackController`
        link step was skipped only because the app happened to be running and holding its own .exe
        file open, unrelated to this change).
      - **Deliberately NOT done in this pass** (structural decode only, not semantic): the TOC's
        `effectId`/`category` numbering hasn't been reconciled against
        `Rack::EffectDefinitions`'s own `effectId`/`EffectClass` values, the raw field tags (`Driv`,
        `Levl`, etc.) haven't been mapped onto `ParamDefinition::key`, and the signed-32-bit values
        haven't been calibrated against the 0-127 CC-scale values `EffectEditorComponent` already
        uses. None of that is wired into any UI yet - `EffectEditorComponent` still requires manually
        picking each slot's model and every value. That reconciliation is the next real step toward
        the "no live readback" goal the parked item below described, and is intentionally left for a
        separate, focused pass.
- [x] **Wired `BulkRigParser` into `DiagnosticsComponent`, and fixed a real decode bug found while
      doing it (2026-07-26)**.
      - **Bug fix**: `RackController`'s `getBulkTfx`/`setBulkTfx` handler was 7-bit-decoding
        `frame.params`, which `SysExFrame::parse` had already trimmed the trailing `F7` off of -
        NOT the harmless trailing-byte difference it looked like at first glance. A byte-by-byte
        diff of the two decodes proved excluding the `F7` shifts other decoded bytes too (first
        mismatch always landed exactly at the start of the already-unused TOC/section tail padding
        in both real samples checked, so it happened to be harmless in practice, but was still
        objectively wrong and would not be safe to rely on in general). Fixed to decode
        `rawBytes[6:]` (including the `F7`), matching `BulkRigParser`'s own logic and ElevenHack's
        exact `Arrays.copyOfRange(msg, 6, msg.length)`. New regression test in
        `RackControllerTests.cpp` drives the real capture through the actual `RackController`
        pipeline end-to-end and asserts the corrected 978-byte length.
      - **Effect/slot-type names confirmed to reconcile directly** - upgrading the "only checked
        against one real sample" caveat in the parked item below: cross-referencing all 10 of that
        sample's `effectId`/`category` pairs against `EffectDefinitions.cpp`'s registry, every single
        one matches a real, already-documented effect in exactly the slot-type category expected
        (e.g. `effectId 12`/`category 0` -> Amp/Cab, matching `EffectDefinitions.h`'s own existing
        note that 12 is `AMP_CAB`; the two slots categorized FX1/FX2 held Chorus/Vibrato and Dyn3
        Compressor respectively, both members of the real hardware-confirmed FX1/FX2 fixed effect
        list). `BulkRigParser::EffectSlot`'s `effectId` doc comment updated to reflect this.
      - `DiagnosticsComponent::onBulkRigReceived` now calls `BulkRigParser::parseDecoded()` and logs
        the rig name and, per slot, its real effect name (`EffectDefinitions::lookup`) and slot-type
        name (`EffectDefinitions::effectClassName`) instead of raw `effectId`/`category` numbers -
        e.g. `C [Vol]: Volume Pedal (4 raw params)` instead of `C: effectId=38 category=2`.
      - **Still NOT reconciled** (and this pass didn't attempt it): the raw per-param field tags
        (`Dly `, `InLv`, `ChVb`, ...) are a genuinely different key scheme from
        `ParamDefinition::key` (BBD Delay's live-CC key is `"Dely"`/`"Inpt"`/`"Mod "`, but its Bulk
        Rig tag is `"Dly "`/`"InLv"`/`"ChVb"` - confirmed by direct comparison, not assumed) - so
        per-param values are still shown as raw tags, not friendly labels. See the parked item below.
      - Build-verified (both targets compile and link, 62 test groups pass) and manually exercised
        by running the rebuilt app.
- [x] **Signal-chain editor UI ported from the mockup (2026-07-27)** -
      `docs/mockups/signal-chain-editor-concept.html` implemented as a real "Signal Chain" tab
      (`Source/SignalChainComponent.h`/`.cpp`), added alongside "Effect Editor" (not replacing it).
      - **V1 scope, per explicit user decisions**: chain order is fixed, not editable - the mockup
        itself never implemented drag/reorder, and there's no known way to write a new order back
        to the unit anyway, so it would be purely cosmetic; reordering is a follow-up once
        device-side reordering (if it exists at all) is understood. A dedicated preset `ComboBox`
        lives in this tab (its own `requestAllRigNames()` fetch, same mechanism
        `RigBrowserComponent` uses) rather than reusing the Rig Browser tab, accepting a small
        amount of duplicated fetch bookkeeping in exchange for matching the mockup's layout.
      - **Reused, not reinvented**: extracted `SlotConfig`/`slotConfigs()` (the 7-slot CC-mapping
        table: Distortion/Wah/Mod/Reverb/Delay/FX1/FX2) out of `EffectEditorComponent.cpp`'s
        anonymous namespace into shared `Source/SlotConfig.h`/`.cpp`, and extracted the
        "pick a model, edit bypass + params over MIDI CC" widget into a new reusable
        `Source/SlotParamsPanel.h`/`.cpp` component. `EffectEditorComponent` now just wraps its
        `slotSelector` dropdown around one embedded `SlotParamsPanel` - same CC numbers, same
        widget types/ranges/dedup rules, same layout math, purely relocated, not rewritten.
        `SignalChainComponent` embeds its own `SlotParamsPanel` instance, shown when a chain block
        with a real `SlotConfig` entry is clicked (Wah/Distortion/Mod/Delay/Reverb/FX1/FX2 - not
        expanded beyond what `EffectEditorComponent` already covers); Volume/Amp/Cab/FX Loop show a
        "No editable parameters mapped for this block yet" fallback label, honestly reflecting
        today's real coverage gap rather than fabricating placeholder controls.
      - **New, genuine partial win against "no live readback"**: `SlotParamsPanel::setSlot()` now
        takes an optional `preferredEffectId` and `knownBypass`. `SignalChainComponent` listens for
        `onBulkRigReceived`, decodes via `BulkRigParser::parseDecoded()`, and for each chain block
        whose category maps to a `BulkRigParser::EffectSlot` (vol/wah/mod/reverb/delay/disto/fx1/fx2
        by `EffectClass`, ampCab covers both "amp" and "cab" - Cab still shows nothing, same as the
        mockup, since Amp/Cab is one combined effect in `EffectDefinitions`) looks up the real
        effect name for the block's sub-label and passes the decoded `effectId` + `bypa` (a plain
        0/1, needs no calibration unlike per-knob values) into the editor panel when that block is
        clicked - so which model is loaded and its bypass state ARE now readable from real hardware,
        even though per-knob values still are not. Selecting a rig in the preset dropdown calls
        `selectRig()` then `requestBulkRig()` to refresh the chain - noted in code as a known,
        unworked-around race (no confirmation the unit's internal rig-switch finishes before the
        Bulk Rig request goes out right behind it; not patched with a guessed delay).
      - Build-verified (both targets compile and link, 62 test groups pass, main app launches with a
        real window). Manually click-tested against the running app with real hardware connected -
        see the block-order bug this surfaced, fixed immediately below.
- [x] **Fixed: Signal Chain tab showed the wrong block order (2026-07-27)** - manual testing against
      a real rig ("SLO 100") found the chain's order didn't match the unit's real pedal order, even
      though the individual effect names were right. Root cause: `SignalChainComponent` was always
      rendering the V1 fixed guessed order from above, never actually using the real per-rig order
      that was already sitting in the decoded data. Comparing the user's independently-known real
      order against the decoded slot letters confirmed `BulkRigParser::ParsedRig::slots`' existing
      C-to-L order directly encodes real signal-chain position (see "twenty-second round",
      protocol-spec.md) - so `updateBlockDataFromRig()` now rebuilds the chain's block order from
      `rig.slots` directly on every Bulk Rig decode (falling back to the old guessed order,
      `buildDefaultChain()`, only before any real decode arrives), instead of just patching
      sub-labels into a fixed order. Also corrected an earlier modeling assumption this exposed:
      Amp/Cab's overall position isn't fixed, only their adjacency to each other is - `ChainBlock`'s
      `fixed` flag now applies only to Input/Output. Along the way, also fixed a display bug in
      `DiagnosticsComponent`'s Bulk Rig log that was printing each slot's letter as its ASCII code
      (`67` instead of `C`) via `juce::String::charToString`, which is what made the letter order
      hard to read while diagnosing this. Build-verified (62 test groups pass, both targets link) and
      **manually re-confirmed against the real "SLO 100" rig (2026-07-27)** - the Signal Chain tab
      now shows the correct real order (Volume, Wah, FX2, Distortion, Amp, Cab, FX Loop, Mod, Delay,
      FX1, Reverb).
- [x] **Started mapping individual Signal Chain blocks - "Input" first (2026-07-28).** Added a live
      CC sniffer to `DiagnosticsComponent` (any incoming plain 3-byte MIDI CC now gets a clear
      "Incoming MIDI CC: CC n = v" log line, not just raw hex) to hunt for undocumented CCs. Used it
      to test True-Z (`PIGI`, a Rig Params field already fully modeled with all 13 options but
      flagged as having no known CC) - cycling through several settings on real hardware produced no
      varying signal at all, ruling out a live CC for it (see protocol-spec.md "twenty-sixth round").
      Since Input Selector/True-Z can only be read from a Bulk Rig decode, not written live, the
      "Input" block now shows both read-only (via a new `rigParamOptionName()` helper resolving
      `rig.rigGlobals`' `PIGI`/`WorB` against the existing Rig Params option lists) instead of the
      generic "not yet mapped" fallback - the first block mapped with no live-editable control at
      all, a genuinely different case from every other slot done so far. Also removed the
      long-standing restriction that Input/Output blocks couldn't be clicked/selected (Output still
      falls through to the generic fallback, having nothing decodable of its own yet). Build-verified
      (both targets compile/link, 71 test groups pass - unchanged, this is UI/decode-plumbing only).
- [x] **Made Input Selector/True-Z locally-editable dropdowns instead of read-only text
      (2026-07-28).** User request, explicitly scoped: no live CC exists for either field (see
      above), so these can't sync live like every other mapped control - but the user wants them
      editable anyway, with the selection eventually feeding into a real "Save to Unit" write once
      that exists. Replaced `ChainBlock::infoText` with raw decoded values
      (`decodedInputSelectorValue`/`decodedTrueZValue`) and added a small dedicated panel
      (`inputEditorPanel`, two `ComboBox`es populated from the "Rig Params" `EffectDefinition`'s own
      option lists) as a third mutually-exclusive view alongside `paramsPanel`/`noSlotLabel` - not
      built as a `SlotParamsPanel` instance, since that component's whole design is CC-send-oriented
      and nothing here can send anything. The user's picks live in
      `pendingInputSelectorValue`/`pendingTrueZValue`, reset on every fresh Bulk Rig decode (a
      pending edit belongs to whichever rig was loaded when it was made). **Explicitly NOT
      attempted**: actually reaching the unit on save. That needs a Bulk Rig **encoder**
      (`BulkRigParser` only ever decodes) plus sending it via `CMD_SET_BULK_TFX`
      (`SysExFrame::Command::setBulkTfx`, currently only ever *decoded* as an incoming reply, never
      sent - `RackController` has no method for it at all) - a materially larger, genuinely risky
      undertaking (a malformed rig write has no known undo) left as a clearly separate, later
      decision, not started here. `showSaveConfirmPopup()`'s stub gained a pointer comment to this
      effect. Build-verified (both targets compile/link, 71 test groups pass - unchanged).
- [x] **Wired Volume Pedal into a real `SlotConfig` (2026-07-28).** Same pattern as Wah: only
      Position (CC 7) has a confirmed CC, so only that knob is wired; Min Volume and Linear/Log have
      no documented CC and are intentionally omitted rather than guessed (see
      `docs/master-control-map.md` §5). Added the `"Volume Pedal"` entry to `SlotConfig::
      slotConfigs()` (Bypass CC 75, effect IDs 38/72 - "sibling" IDs sharing one definition, same
      dedup handling `SlotParamsPanel` already does for these) and mapped the Signal Chain tab's
      `"vol"` block id to it in `slotConfigNameForBlockId()` - clicking Volume now opens the same
      live-CC editor every other mapped slot uses, instead of the generic fallback. Bypass/Position
      are from the official manual chart, not yet hardware-tested. Build-verified (both targets
      compile/link, 71 test groups pass - unchanged, no new tests needed for a data-only addition).
      **Update (2026-07-28, later same day)**: Min Volume/Linear-Log turned out to have confirmed
      exact raw Bulk Rig tags (`Min `/`Tapr`) after all - see the `SlotParamsPanel` entry directly
      below for how they ended up wired in as local-only (not live-CC) controls instead of staying
      fully omitted.
- [x] **Extended `SlotParamsPanel` to show params beyond a slot's live-CC range as local-only
      controls (2026-07-28).** Prompted by Volume Pedal: a real Bulk Rig decode confirmed Min
      Volume/Linear-Log's raw tags (`Min `/`Tapr`) match their CC-side keys exactly, but a live
      hardware CC scan (every "gap" CC number the manual documents) found no CC for either - so
      there's a confirmed decoded value but nothing to send. `rebuildForSelectedEffect()`'s render
      loop no longer stops once `SlotConfig::settingCc` is exhausted - params beyond it now show too,
      *if* `knownToggleStates`/`knownKnobValues` has a confirmed value for them, labelled "(not
      synced)" and with no `onValueChange`/`onClick` handler (so nothing is ever sent) - otherwise
      skipped exactly as before. Reusable by any future slot with the same "confirmed tag, no CC"
      gap, not a Volume-Pedal-specific hack. Build-verified (both targets compile/link, 71 test
      groups pass - unchanged, this is rendering logic only, no new data to test).
- [x] **Built Amp/Cab's real per-model tone knobs from the official manual (2026-07-28), no
      hardware access available.** Matched our 16 `EffectDefinitions::ampModelOptions()` names
      against the manual's separate 31-model table (year-prefix formatting aside) - 15 of 16 matched
      unambiguously; "67 Black Duo" doesn't cleanly match any of several similarly-named "Black X"
      entries, so it uses "Black Panel Duo"'s layout as an explicitly flagged, unconfirmed guess
      (user's explicit call - same confidence tier as the earlier Vibe Phaser extrapolation). Added
      16 new synthetic-ID `EffectDefinition` entries (1000-1015, `1000 + ampModelOptions()` index -
      see `EffectDefinitions.cpp`'s Amp/Cab section) rather than touching the real wire-level
      `effectId=12` entry (`EffectDefinitionsTests.cpp`'s existing lookup(12) test locks that one
      down unchanged) - solves the "one effect ID, 16 selectable models" shape mismatch that blocked
      this before without needing new UI machinery: the synthetic IDs just slot into `SlotConfig`'s
      existing "list of effectIds" mechanism directly.
      New `"Amp/Cab"` `SlotConfig` entry (Bypass CC 111, Setting 1-14 = CC 13/14/15/16/21/10/112/3/
      84/24/23/22/44/45) and `slotConfigNameForBlockId("amp")` mapping.
      `SignalChainComponent::updateBlockDataFromRig()` resolves the real loaded model from the Bulk
      Rig payload's "sld6" field (`1000 + sld6`, confirmed to match `ampModelOptions()`'s own index
      scale in the "twenty-third round") instead of the always-12 wire effectId, so a live decode
      pre-selects the actual model. Tone-knob VALUE readback explicitly not attempted (no real
      non-default sample to reconcile against). **Not hardware-tested at all** - built entirely from
      the manual while away from the unit; flagged for a real hardware pass whenever it's available
      again. Build-verified (both targets compile/link, 73 test groups pass - 2 new, sanity-checking
      the synthetic ID range resolves correctly and the real `effectId=12` lookup stays untouched).
- [x] **Consolidated Rig Globals into the Signal Chain tab (2026-08-03).** Prototyped first as a
      horizontal "Rig globals" row in `docs/mockups/signal-chain-editor-concept.html` (Main Volume,
      Tuner On/Off, Tap Tempo, FX Loop Bypass/Send/Return/Mix, laid out as four side-by-side groups
      rather than the original vertical stack), then ported directly into `SignalChainComponent`,
      placed above the chain panel. All the logic/wiring (live two-way Main Volume sync via
      `displayToRaw()`/`rawToDisplay()`, Tuner's device-confirmed-only status label, Tap Tempo's
      momentary CC 64 send, FX Loop's fixed Bypass/Send/Return/Mix CCs) moved over unchanged from the
      now-removed `RigGlobalsComponent` - same behaviour, just relaid-out horizontally and hosted in a
      different component. The standalone "Globals" tab in `MainComponent` is now gone (removed
      per the user's choice over keeping a duplicate second copy of these controls);
      `RigGlobalsComponent.h`/`.cpp` deleted entirely and dropped from `CMakeLists.txt` rather than
      left as dead code. Build-verified (both targets compile/link, 73 test groups pass - unchanged,
      this is UI relocation only, no new data to test).
- [x] **Removed the "Rig Browser" and "Effect Editor" tabs entirely (2026-08-03).** Both were fully
      subsumed by the Signal Chain tab: `RigBrowserComponent`'s rig list + double-click-to-load is
      the same `requestAllRigNames()`/`selectRig()` mechanism `SignalChainComponent`'s own preset
      dropdown already used; `EffectEditorComponent`'s per-slot dropdown was already just a thin
      wrapper around `SlotParamsPanel` (see its header comment) covering a strict subset of what
      clicking a chain block now covers (Distortion/Wah/Mod/Reverb/Delay/FX1/FX2, but not Volume
      Pedal/Amp-Cab, which only Signal Chain ever had). One real capability gap was found and
      deliberately dropped, not silently lost: `RigBrowserComponent` tracked `RackController::
      Listener::onCurrentRigReceived` (a live SysEx notification of whatever rig is actually loaded
      on the unit right now, e.g. after a front-panel change) and highlighted that row with a `>`
      marker - `SignalChainComponent`'s preset dropdown only reflects what's picked from the UI, with
      no equivalent "here's what's actually loaded" indicator. Flagged to the user, who chose to drop
      it rather than port it over. `RigBrowserComponent.h`/`.cpp` and `EffectEditorComponent.h`/`.cpp`
      deleted entirely and dropped from `CMakeLists.txt`; `MainComponent` now hosts only "Diagnostics"
      and "Signal Chain". Build-verified (both targets compile/link, 73 test groups pass - unchanged,
      neither removed file was part of the test target).

## Not yet scheduled / parked

- Python protocol-discovery logger (`mido`/`python-rtmidi`) — only needed now for gaps ElevenHack
  and the official CC chart don't cover. May end up largely unnecessary given how much Milestone 0-1
  already resolves from prior art.
- **Reconcile the decoded Bulk Rig payload with `EffectDefinitions`/`SlotParamsPanel` so the
  editor can auto-populate from what's actually loaded on the unit**, instead of requiring the user
  to pick the slot's model and every value by hand. The structural decode itself is now done (see
  `BulkRigParser` above, Milestone 5) - what remains is semantic, not byte-level:
  - ~~Map the TOC's per-slot `effectId`/`category` numbering onto `Rack::EffectDefinitions`'s own
    `effectId`/`EffectClass` values~~ - **done (2026-07-26), reconfirmed (2026-07-27)**: matches
    directly, no conversion needed. Wired into `DiagnosticsComponent` (see above). Checked against
    two real rigs now (the original sample, plus "SLO 100" - see the slot-letter-position finding
    just above), both fully consistent with `EffectDefinitions`.
  - ~~Map each slot's raw field tags (`Driv`, `Levl`, `sld1`, ...) onto the matching
    `ParamDefinition::key` for that effect~~ - **partially done (2026-07-27, "Round 1"): toggles
    only**. Cross-referenced all 3 real decoded samples against every effect in
    `EffectDefinitions.cpp` (see protocol-spec.md, "twenty-third round") and confirmed exact
    tag-to-key matches for several effects' knobs, but since every knob value observed is an
    untouched factory default, wiring knobs up now would mean guessing the 0-127 scale, not reading
    real data — so only **toggle**-kind params were wired (0/1 needs no scale). Exactly one such
    toggle is confirmed: Tape Echo's `Hiss`. Shipped as `SlotConfig::confirmedRawTagForKey()` (a
    deliberately small "confirmed pairs only" table), threaded through
    `SlotParamsPanel::setSlot()`'s new `knownToggleStates` param and
    `SignalChainComponent::updateBlockDataFromRig()`. Build-verified, 5 new tests
    (`SlotConfigTests.cpp`), 67 test groups total.
  - ~~Extend raw-tag reconciliation beyond exact-string matches~~ - **done for Flanger/Multi Chorus
    (2026-07-28, "twenty-fifth round")**: wired up the name-similarity pairs already identified but
    left out of the exact-match table (Flanger's `PreD`/`Rate` -> bulk `PDly`/`Sped`; Multi Chorus's
    `PreD`/`TriS`/`Widt` -> bulk `PDly`/`Wave`/`Wdth`) via a new, separate
    `SlotConfig::bestEffortRawTagForKey()` (falls back from the exact-match table, keeps
    `confirmedRawTagForKey()`'s stricter contract intact for anything that still needs it).
    **Tape Echo's remaining unmatched keys (`Dely`/`Fdbk`/`Mix `/`RecL`/`Head`/`ExpD`) stay parked** -
    no corresponding bulk tag was ever recorded for them, only that the CC key names don't match, so
    there's nothing to extrapolate from without fabricating it. Build-verified, 3 new tests, 71 test
    groups total.
  - ~~Figure out the conversion from the Bulk Rig's signed-32-bit values to the 0-127 CC-scale
    values `SlotParamsPanel`'s controls use~~ - **done for knob-kind params (2026-07-28,
    "Round 2")**: user ran a real full-range sweep of Sine Wah's `Filt` knob (min/mid/max = raw
    `-2147483648`/`0`/`2147483647`) - the standard Q31 fixed-point pattern (full signed 32-bit range,
    linear, zero-centred). See protocol-spec.md "twenty-fourth round" for the formula. Applied to
    every other knob-kind param `SlotConfig::bestEffortRawTagForKey()` has a raw tag for (Distortion's
    knobs, Flanger's `Dpth`/`Fdbk`/`Rate`, Multi Chorus's knobs including `Widt`, Tape Echo's `Wow `,
    Eleven SR's `Tone`, Wah's `VxCr`) as a documented extrapolation - only Wah's `Filt` is individually
    hardware-confirmed, the rest assume one uniform encoding rather than per-effect testing.
    Selector-kind params (`Sync`, `Type`, `sld6`, ...) are explicitly NOT covered - confirmed NOT a
    single uniform formula there (`EffectAmpCab`'s amp selector alone has both a compact 0-15 index
    AND a full-int32-range encoding for the same selector), so those still need individual real
    hardware sweeps, not a guess. Shipped as `SlotConfig::knobRawToCcValue()`, threaded through
    `SlotParamsPanel::setSlot()`'s new `knownKnobValues` param and
    `SignalChainComponent::updateBlockDataFromRig()`. Build-verified, 1 new test, 68 test groups
    total.
  - ~~Whether the lettered slot really is signal-chain position~~ - **confirmed (2026-07-27)**:
    compared a second real rig's ("SLO 100") independently-known real pedal order against its
    decoded slot letters - exact match, position for position (see "twenty-second round",
    protocol-spec.md, and the Signal Chain tab fix in Milestone 5 above). No longer just weakly
    evidenced from one sample.
  - **Also relevant**: the Rig Description tuple structure (11 × 3-byte tuples, `[count byte] +
    tuples`, confirmed via diffing in "fourth round" - see protocol-spec.md) is a separate, much
    smaller SysEx reply already partially decoded - worth checking whether it's a subset/index into
    the same per-slot structure the Bulk Rig payload uses, or a wholly independent structure.
    ElevenHack's own `CMD_RIG_DESC` handler (`ElevenReceiver.java`) is a dead stub with no real
    parsing logic, so this can't be answered from their source either.
  - Not yet scoped into concrete steps - see protocol-spec.md Open Items for the same entry.
- ~~Wire Volume Pedal, Amp/Cab, and FX Loop into `SlotParamsPanel`/`slotConfigs()`~~ - **Volume Pedal
  done (2026-07-28)**, see Milestone 5 above (Bypass=75, Position=7; Min Volume/Linear-Log have no
  documented CC, omitted). Amp doesn't fit `SlotConfig`'s "pick an effect ID from a list" shape (one
  effect ID with 16 selectable models, not several effect IDs) and Cab has no independently-known
  parameters at all; FX Loop already has its own dedicated controls in the "Rig globals" row above
  the chain (fixed CCs, not a "Setting N" slot - see the "Consolidate Rig Globals into the Signal
  Chain tab" entry below) that could potentially be reused/duplicated here instead of built fresh.
  **Update**: Amp/Cab's tone knobs ARE now wired too, via the synthetic-ID approach - see the
  "Built Amp/Cab tone knobs" entry in Milestone 5 above. Cab and FX Loop's own params remain as
  described.
- **Add chain-reordering UI to the Signal Chain tab.** The chain now shows each rig's real order
  (see the "Signal-chain editor UI" and its order-bug fix, both Milestone 5 above) but isn't
  user-editable - dragging a block around wouldn't do anything. Blocked on the same open question as
  before: whether the unit exposes writing a new order via MIDI/SysEx at all (see protocol-spec.md
  Open Items on the Bulk Rig/Rig Description relationship) - if not, this may end up staying
  display-only, or become a purely local/cosmetic reorder with no real effect.
- ~~Amp/Cab tone-knob editor~~ - **done (2026-07-28)**, see Milestone 5 below: 15 of our 16
  `ampModelOptions()` names reconciled unambiguously against the manual's 31-model table; "67 Black
  Duo" uses a flagged, unconfirmed name-similarity guess. **Still parked**: cabinet/mic-position
  mapping, which isn't documented in either source at all - Cab still has zero known parameters.
- **Para EQ's "L Type"/"H Type" controls have no known CC.** The one candidate tested (CC 60 /
  Setting3) was ruled out for both. Deferred - doesn't block anything else in FX1/FX2. See
  protocol-spec.md Open Items ("twentieth round").
- **Delay's "Fine" control has no known CC**, across all three delay effects (BBD Delay, Tape Echo,
  Dyn Delay). CC 59 was tested and ruled out (it's actually Dyn Delay's own "Env Mod Rate"). May have
  its own CC outside the numbering scheme entirely, or may not be MIDI-addressable at all (the manual
  describes it as toggled by a physical "SW2" switch). See protocol-spec.md Open Items.
- **Hardware-verification debt on already-shipped Signal Chain tab slots/controls.** Only
  Distortion's knobs and Chorus/Vibrato's knobs/Sync/Bypass/Mode toggle are independently confirmed
  against real hardware so far. Still untested: Wah, the other 5 Mod-slot effects, Reverb, all of
  FX1/FX2, BBD Delay's and Dyn Delay's individual knobs (only Tape Echo is fully confirmed), the
  Multi Chorus Width/Lo Cut reorder fix, and the "CC Setting N maps positionally" hypothesis for
  non-knob params (toggles/selectors) in general - only confirmed for knobs (Distortion) so far. See
  protocol-spec.md Open Items for the full per-item breakdown.
- **Rig-switching mechanism isn't fully understood.** Whether Program Change or the CC32
  "User/Factory Bank Change" toggle can drive a rig switch on their own, vs. only the SysEx
  `CMD_CURR_RIG_NUM` write `RackController::selectRig()` already uses (and `SignalChainComponent`'s
  preset dropdown now calls) - relevant to how reliable rig-switching-then-decoding really is. See
  protocol-spec.md Open Items.
- **Minor unresolved protocol curiosities, none currently blocking anything**: what `CMD_TUNER_A`
  (`0x41`) represents, what effect index 0 ("Eleven"/"DigiElvnELVu") actually is, what the unhandled
  `ASYNCSET 0x03` command means, whether Rig Description's per-tuple middle byte is a stable identity
  or just a monotonic reload counter, and which of the 2 MIDI inputs/3 MIDI outputs is "the" real
  control port. See protocol-spec.md Open Items for each.
