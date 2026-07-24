# Eleven Rack Controller App

Living design doc. We'll keep editing this as we talk through the project.

## Background / Problem

- Hardware: AVID Eleven Rack (digital guitar amp/modeler with USB connection to a host PC).
- AVID's official editor/librarian software (used to edit rig/program settings) no longer runs on
  current Windows.
- The licensing component bundled with that software causes unrecoverable blue-screen crashes on
  the user's current machine — so the official tooling is effectively unusable, not just outdated.
- USB packet capture has confirmed the Eleven Rack does send/receive standard USB traffic when
  connected (need to confirm class — see Open Questions).

## Goal

Build a replacement application that can:
1. Read and adjust the Eleven Rack's amp/effect settings.
2. Store and manage programs/presets.
3. Do this without depending on AVID's original software or its problematic license driver.

## Phased Plan (draft)

### Phase 1 — Protocol discovery
Map out the actual communication protocol between host and unit over USB: what gets sent when a
knob is turned, a preset is saved/loaded, a patch is selected, etc. **Largely de-risked — see
Prior Art Found below.** Revised sub-steps:

1. Study the ElevenHack source (already cloned/read) and write up our own protocol spec doc from
   it — message format, command list, init sequence, 7-bit encoding scheme — independent of having
   to keep re-reading Java.
2. Port/adapt the protocol layer into C++/JUCE using ElevenHack as the reference implementation
   (Apache-2.0 — attribution required, see Licensing Notes).
3. Validate against the real unit: send known ElevenHack messages (request effect count, request
   rig name, etc.) and confirm the replies match what the Java code expects. This proves the port
   is correct on current hardware/firmware and surfaces any drift since ElevenHack was written
   (2013).
4. Check the official User Guide's MIDI CC chart (pages ~95-98) for real-time parameter control —
   may cover knob/parameter changes via plain MIDI CC rather than SysEx, which would need no
   reverse engineering at all if so (not yet confirmed — the specific PDF mirror found returned
   403; need another source).
5. Only for anything not covered by ElevenHack or the CC chart: fall back to the original
   from-scratch plan (passive front-panel monitoring + active Universal SysEx probing + AI-assisted
   diffing), using the Python logger.

### Phase 2 — Interface / communication layer
Build a library that can open a connection to the unit and send/receive the messages mapped out in
Phase 1 (read current state, change a parameter, load/save a program).

### Phase 3 — UI application
Build the actual editor UI on top of the Phase 2 interface layer.

## Working Assumption

Treating this as a **MIDI SysEx** reverse-engineering problem (USB-MIDI class-compliant device),
not a raw/custom USB protocol. **Confirmed (2026-07-24)**: the Milestone 2 skeleton app enumerated
the unit as plain OS-level MIDI ports (no special driver) and successfully round-tripped a
Universal SysEx Identity Request/Reply over it — see
[docs/protocol-spec.md](protocol-spec.md#hardware-validation-log) for the decoded reply, which also
independently confirmed ElevenHack's vendor/device ID constants against real hardware.

## Constraint: original AVID software cannot be used at all

The original Eleven Rack Editor/Librarian cannot be installed anywhere, including in a VM. Its
licensing component is pulled from the internet during install and is no longer compatible with
modern Windows — it causes an unrecoverable blue screen requiring a full Windows reset. This rules
out the standard reverse-engineering approach of "run the real app, sniff the traffic it sends."

**Protocol discovery has to happen without ever running the original software.** Revised plan:

1. **Search for surviving documentation first.** AVID/Digidesign may have published a MIDI
   Implementation Chart or SysEx spec for the Eleven Rack at some point. Check archive.org, old
   AVID knowledge-base mirrors, gearspace/forums, and whether anyone has already reverse-engineered
   this (existing GitHub projects, hobbyist tools). This is the highest-leverage thing to check
   before doing any of our own reverse engineering.
2. **Passive monitoring via the unit's own front panel.** The Eleven Rack has physical controls
   (knobs, footswitches, screen) and can be operated standalone, with no host software at all.
   Connect it over USB, run a generic MIDI monitor (e.g. MIDI-OX, or a small custom logger we
   write early on) against its USB-MIDI port, and just operate the front panel directly —
   selecting programs, tweaking parameters, saving. Whatever MIDI/SysEx traffic the unit emits on
   its own is real protocol data, with no dependency on AVID's software.
3. **Active probing with standard MIDI conventions.** Many MIDI devices respond to the Universal
   SysEx Identity Request (`F0 7E <ch> 06 01 F7`) and similar generic dump-request patterns. Try
   these against the unit and see what comes back — may reveal manufacturer/model/version info or
   even trigger a full patch dump, giving us real data to decode without guessing blind.
4. **AI-assisted decoding.** Once we have captured SysEx dumps (from step 2 and/or 3), an AI
   assistant can help spot structure across many captures — e.g. diffing dumps taken before/after
   a single isolated change to infer which bytes correspond to which parameter, proposing likely
   header/checksum/terminator structure based on common SysEx conventions, and helping write the
   decoder as patterns emerge.
No Mac/legacy-OS fallback is available (confirmed) — see Decisions Log. Discovery has to work
entirely through steps 1–4 above.

## Open Questions

- **Is the Eleven Rack USB-MIDI class-compliant?** (see Working Assumption above — needs
  confirming via Device Manager.)
- **Documentation** — gather what AVID/official material still exists (manuals, MIDI
  implementation charts, SysEx specs if published) to seed this repo before reverse engineering.
- ~~Is live/real-time parameter control plain MIDI CC, separate from the bulk-SysEx rig save/load
  path?~~ **Resolved (2026-07-23):** yes — confirmed via the official User Guide's MIDI CC chart
  (Chapter 11, Table 12). See [docs/protocol-spec.md](protocol-spec.md) for the full table and
  remaining open items this raised (positional CC mapping hypothesis, rig-switching mechanism still
  unconfirmed, one apparent duplicate/error in the manual's table).

## Related Docs

- [docs/protocol-spec.md](protocol-spec.md) — the technical reference: official MIDI CC table +
  reverse-engineered SysEx protocol details, kept up to date as we learn/verify more.
- [docs/implementation-plan.md](implementation-plan.md) — trackable task breakdown/checklist for
  actually building this, kept in sync with the decisions and findings recorded here.
- [docs/development-guide.md](development-guide.md) — environment setup, build/run/debug flow for
  the JUCE/CMake project skeleton (VS Code + Windows).

## Tech Stack

**Decision: C++ with JUCE for the production app; Python for Phase 1 discovery only.**

Rationale: JUCE is the de facto standard framework for professional audio software — it's what a
large share of the industry (VST/AU plugin makers, hardware companion/editor apps in this exact
category) builds on. It provides native MIDI I/O with real SysEx support and a component/graphics
system purpose-built for the "knobs, sliders, meters, patch browser" visual vocabulary of hardware
editors — so the look/feel naturally aligns with tools like Pro Tools/Reaper/Cubase, rather than
a web UI reskinned to resemble one. It also means the interface/protocol layer (Phase 2) and the
UI (Phase 3) can live in one C++ codebase with no IPC or backend/frontend split.

- **Phase 1 — protocol discovery tool stays Python** (`mido` + `python-rtmidi` for MIDI I/O,
  `construct` for binary/SysEx parsing). This is throwaway research tooling, not shipped code, so
  it doesn't need to match the production stack — Python's fast REPL/notebook iteration is still
  the right tool for probing an unknown protocol.
- **Phase 2 & 3 — JUCE (C++).** `juce::MidiInput`/`MidiOutput` for the interface layer, JUCE's
  component system for the editor UI. One codebase, cross-platform (Windows/Mac/Linux) for free.
- **Qt (C++)** was considered as the alternative cross-platform toolkit but rejected — it's a
  general-purpose GUI toolkit with no built-in audio/MIDI awareness (would need pairing with
  something like RtMidi), whereas JUCE is audio-native and a tighter fit.

### Considered and rejected: web-based UI (React/Angular)

Initially explored building the production UI in React, with the interface layer either living
directly in the browser via the **Web MIDI API** (`navigator.requestMIDIAccess({ sysex: true })`)
or in an Electron-wrapped Node backend. Rejected for two reasons:
1. Web MIDI only works in Chromium-based browsers (no Firefox/Safari) — a real limitation for a
   plain "open it in your browser" web app.
2. More importantly, once the goal became matching the look/feel of standard DAW software, C++/
   JUCE turned out to be the actual industry-standard answer for this category of app, not a web
   stack pretending to be one.
- **Not using C# or a separate native interface library**, per the above.

## Decisions Log

- **No Mac/legacy-OS fallback available.** Protocol discovery must work entirely through passive
  monitoring (front panel) and active probing (Universal SysEx) — see Phase 1 plan above. The
  "old Mac" fallback option is off the table; removed from active consideration.
- **No existing MIDI monitoring tool on hand.** We'll need to build a small MIDI logger ourselves
  as an early, standalone deliverable before any real protocol decoding can start. This makes "MIDI
  monitor/logger" the first concrete piece of code in this repo, built in Python (see Tech Stack).
- **Production stack: C++/JUCE**, chosen for alignment with the DAW/audio-hardware-editor industry
  standard and its native MIDI/SysEx support, over an earlier React/Web-MIDI/Electron plan (see
  Tech Stack for full reasoning).
- **Using ElevenHack as reference/prior art, not as a base to fork.** Its own live-editing GUI is
  unfinished/missing, and it's a different stack (Java/Swing) than our JUCE/C++ decision. Plan is
  to port its protocol layer and effect/parameter model into our own JUCE codebase (with required
  Apache-2.0 attribution), building our own editing UI from scratch — see
  [docs/implementation-plan.md](implementation-plan.md) for the tracked task breakdown.

## Prior Art Found (2026-07-23)

Searched the web (I have WebSearch/WebFetch tools available directly in this session — no special
access setup needed from the user) for existing documentation or reverse-engineering work. Found:

- **ElevenHack** — https://gitlab.com/schmidg/elevenhack (Apache-2.0, Guillaume Schmid, 2013-2020).
  A real, working Java implementation that reverse-engineered the Eleven Rack's USB/SysEx protocol
  from scratch (author states it was "developed without any help or specifications about the
  device") and built a full rig/preset manager on top of it. Cloned locally and read the core
  files. Confirmed protocol details:
  - SysEx frame: `F0 13 0B 0F <msg-type> <command> [params...] F7`
    (`0x13`/`0x0B` = vendor/device ID — `0x13` matches Digidesign's registered MIDI manufacturer
    ID, a good cross-check that this is genuinely MIDI SysEx as assumed; occasionally `0x0E`
    appears instead of `0x0F` for model ID, noted as "strange, happens sometimes" in the source).
  - Message types: `SNDSET (0x00)` = set a value, `REQU (0x01)` = request/query, `ASYNCSET (0x02)`
    = async/unsolicited update, `RESPOND (0x12)` = reply to a query.
  - Command IDs found: bulk tfx get/set, current rig number, save rig, get/set rig name, effect
    description, rig description, effect count, main volume, tuner on/off.
  - Bulk data (e.g. a full rig dump) is packed through a 7-bit encoding scheme (SysEx data bytes
    must be 0–127), with matching encode/decode routines in `SysEx.java`.
  - Device init sequence (`ElevenInit.java`): request main volume → request effect count → request
    each effect description in turn → request every rig name in bank 0, then bank 1 → request
    current rig number → done.
  - Checksum/CRC handling looks unfinished/experimental in the source (`CrcCalc.java` tries CRC32,
    Adler32, and XOR at various offsets against a captured bulk dump) — this may be an area where
    we still need to do our own verification work.
  - Repo also includes a full `.tfx` preset **file format parser** (`tfx/TfxParser.java`), separate
    from the live SysEx protocol — useful for import/export compatibility with existing rig files.
- **`ere`** — https://github.com/yumazak/ere (Rust). Investigated but **not relevant** — it's a
  small utility that maps nanoPAD2 MIDI notes to simulated mouse clicks on the official Eleven Rack
  Editor's window (next/prev patch buttons). It automates the AVID GUI; it does not talk to the
  rack's protocol directly.

### ElevenHack deeper review (2026-07-23)

Read further into the codebase to assess whether it's a ready-to-use replacement editor, and how
much of the actual effect/parameter set it maps. Findings:

**Not a finished editor app.** `Main.java`'s real GUI entry point (`MainFrame`) is commented out
everywhere it's referenced, its `-sendtfx` command is an unimplemented stub, and `MainFrame.java`
does not exist anywhere in the repo (confirmed via full-repo search) — it was never finished,
abandoned, or deliberately left out of this release. What does work and is fully wired up:
- **`RigLoaderGui`/`RigShow`** — a functional rig-file *librarian*: browse a folder of `.tfx`
  files, preview them, load a selected one onto the device as a whole, including a "slideshow"
  auto-advance mode. Real, working code.
- **Console mode** — manual raw-SysEx send for debugging, not an app.

**Effect/parameter coverage is much broader than first assessed.** `Effect.java`'s `mBuildEffect()`
has detailed, named parameter definitions (knobs/switches/selectors, some with explicit ranges)
for nearly every effect slot the unit has: 5 distortion pedal models, 2 wahs, chorus/vibrato, 2
phaser types, graphic EQ (with real per-band dB ranges, e.g. "100 Hz (-12 to +12)"), parametric EQ,
2 compressor types, 2 reverb types, 3 delay types, flanger, roto speaker, volume pedal, and
rig-level globals (input selector, guitar input impedance selector, expression-pedal assignment,
tempo range). `EffectAmpCab` (a dedicated subclass) additionally maps all 16 amp models by ID —
it's the exception, not the rule, needed only because of its name-lookup table. **Known gap:** no
cabinet/mic-position mapping was found alongside the amp-model selector — worth confirming or
filling in ourselves.

**Listen/send API shape confirmed** — this maps directly onto "listen for hardware changes,
send updated values":
- **Listen**: `ElevenReceiver.parseMessage()` distinguishes `ASYNCSET` (unsolicited — the unit
  reporting a change made on its own, e.g. front-panel action) from `RESPOND` (reply to our
  request), dispatching both to callbacks (`callbackMainVolumeSet`, `callbackSwitchRig`,
  `callbackSetRigName`, `callbackTunerSwitch`, etc.). Confirms the unit does self-report state
  changes, as assumed in the original passive-monitoring plan.
- **Send**: `ElevenTransmitter` — `requestXxx()` methods query state; `setMainVolume()`,
  `setRigName()`, `setTunerOn()`, `selectCurrentRig()` push changes.

**Real gap found — no granular per-parameter SysEx set message** — but this turned out to be by
design, not a limitation. There is no SysEx wire message for "set this one effect parameter to
this value"; changing an effect parameter via SysEx requires re-encoding and resending the *entire*
rig as one bulk transfer (`ElevenRack.loadRigStream()`). **Resolved:** the official MIDI CC chart
(see [docs/protocol-spec.md](protocol-spec.md)) confirms real-time single-parameter control is a
separate mechanism entirely — plain MIDI CC, not SysEx. So the two mechanisms split cleanly by
purpose: CC for live tweaking, bulk SysEx for preset load/save/browse. Both are needed in our
interface layer.

**Other quirks noted — treat nothing here as ground truth without hardware validation:**
- `CrcCalc.java`'s checksum handling looks experimental/unfinished (tries CRC32, Adler32, and XOR
  at multiple offsets against one captured dump, not a settled algorithm).
- `ElevenRack.loadRigStream()` builds its bulk-set message with a hardcoded command byte that
  doesn't match the `SysEx.CMD_SET_BULK_TFX` constant as expected — either a bug in the original
  code or a nuance in how set/get commands are actually numbered. Needs clarifying against real
  hardware traffic, not assumed from the source alone.
- **Eleven Rack User Guide (PDF)** — official manual, reportedly documents MIDI CC numbers 3-119
  for real-time control on pages ~95-98 per forum discussion. Have not yet successfully fetched the
  full text (one mirror returned 403) — worth retrying from another source, since if real-time
  parameter control is plain MIDI CC (not SysEx), that part needs no reverse engineering at all.
- **Avid Pro Audio Community forum thread** ("Sysex MIDI command list") — confirms other users have
  also gone looking for an official SysEx spec and not found one; no additional protocol details
  beyond what ElevenHack already gave us.

### Licensing note
ElevenHack is Apache-2.0. If we port or closely derive code from it, we need to retain its
copyright notice/license (see its `NOTICE` file) per the license terms — straightforward, but a
real obligation, not just a courtesy. **Done (2026-07-24)**: root [`NOTICE`](../NOTICE) file and
[`docs/third-party-licenses/ElevenHack-APACHE-2.0-LICENSE.txt`](third-party-licenses/ElevenHack-APACHE-2.0-LICENSE.txt)
added, now that real ElevenHack-derived code exists in `Source/Rack/` (`SysExFrame`,
`SevenBitCodec`, `EffectDefinitions`).

## Reference Material To Collect

- [ ] Eleven Rack Owner's Manual (PDF)
- [ ] Eleven Rack MIDI/SysEx implementation chart, if AVID ever published one
- [ ] Eleven Rack Editor/Librarian software (old installers, for reference/VM use only)
- [ ] Any AVID knowledge-base articles / forum threads with protocol details
