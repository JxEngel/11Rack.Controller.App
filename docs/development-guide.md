# Development Guide

How to set up a Windows + VS Code environment to build, run, and debug this app. Written for
anyone picking this repo up cold.

## Prerequisites

Install these once, in order:

1. **Git** — https://git-scm.com/download/win (if you don't already have it).
2. **Visual Studio Build Tools 2022 (or newer), or full Visual Studio Community** —
   https://visualstudio.microsoft.com/downloads/. During install, make sure the
   **"Desktop development with C++"** workload is checked. This is the part that's easy to miss —
   Build Tools alone, without that workload selected, will not give you a C++ compiler.
   This workload bundles everything needed to build: the MSVC compiler, the Windows SDK, and a
   copy of CMake + Ninja (you do **not** need to separately install CMake).
3. **VS Code** — https://code.visualstudio.com/.
4. **VS Code extensions**: open this folder in VS Code and it will prompt you to install the
   recommended extensions (`.vscode/extensions.json`) — accept that prompt. If it doesn't appear,
   install manually from the Extensions panel:
   - `ms-vscode.cpptools` (C/C++)
   - `ms-vscode.cmake-tools` (CMake Tools)

You do **not** need to install JUCE yourself — the build pulls it automatically (see below).

## First-time setup

1. Clone the repo and open the folder in VS Code (`code .` from the repo root, or File > Open
   Folder).
2. Open the Command Palette (`Ctrl+Shift+P`) and run **"CMake: Scan for Kits"** the first time (or
   whenever your compiler installation changes).
3. Command Palette > **"CMake: Select a Kit"** — pick the detected Visual Studio kit, e.g.
   *"Visual Studio Build Tools 2022 Release - amd64"*. If nothing shows up here, the C++ workload
   likely isn't installed — see Prerequisites step 2.
4. Command Palette > **"CMake: Select Variant"** — choose **Debug** for day-to-day development.
5. Command Palette > **"CMake: Configure"** (this also runs automatically on folder-open, per
   `.vscode/settings.json`).
   - **The first configure will take a while** — it clones JUCE from GitHub via CMake's
     `FetchContent` (a few hundred MB, one-time cost, cached under `build/_deps`). Needs an
     internet connection; if you're behind a restrictive corporate proxy/firewall, plain
     `git clone` access to `github.com` needs to be allowed.
6. Command Palette > **"CMake: Build"** (or `Ctrl+Shift+B`, or the Build button in the status bar
   at the bottom of the window).
   - The **first build compiles JUCE itself** as well as this app, so it's noticeably slower than
     every build after it. Subsequent builds are incremental.

## Running it

- Status bar has a **Run** (▷) button, or Command Palette > **"CMake: Run Without Debugging"**.
- The window has MIDI input/output device pickers and a Refresh button at the top (shared across
  everything below), and four tabs:
  - **Diagnostics** — the original protocol test harness: a known-command picker, the one
    validated write (Select Rig), a raw-send button for generic diagnostics (Universal SysEx
    Identity Request), a MIDI CC test tool, an Effect Index spinner + Request Effect Description
    button, and a log of decoded, human-readable info for anything `RackController` recognizes
    ("Effect Count: 65") — raw hex only for messages it doesn't recognize yet.
  - **Rig Browser** — the first real piece of the actual editor UI (Milestone 5): click "Refresh
    Rig List" to fetch all 208 rig names from the device (one at a time — takes a little while),
    see them listed with their "A1"–"Z4" location labels, and double-click one to load it.
  - **Globals** — rig-level utilities that aren't effect-selectable (there's only ever one of
    each on the unit): Main Volume (a live two-way-synced slider — drag it and it sends live,
    and it moves on its own if the unit's own front panel changes it), Tuner (explicit On/Off
    buttons — no state query exists, so the status label only updates from a real
    device-confirmed reply, not an optimistic guess), Tap Tempo (a single momentary button — one
    click, one tap), and FX Loop (Bypass + Send/Return/Mix knobs).
  - **Effect Editor** — live per-effect parameter editing over MIDI CC, driven by
    `EffectDefinitions`. A **Slot** picker (Distortion / Wah / Mod / Reverb) followed by an **Effect**
    picker scoped to whichever models actually have real decoded parameters for that slot, then
    a Bypass toggle and one control per knob/switch/selector, sent live as you move them. No live
    readback (MIDI CC has no query mechanism) and no auto-detection of what's actually loaded in
    the unit — you tell it via the dropdowns. Deliberately doesn't cover every slot; see
    [implementation-plan.md](implementation-plan.md) Milestone 5 for exactly what's covered and
    why (Reverb, Delay, Amp tone knobs, and FX1/FX2 are all currently missing real parameter data
    to build a meaningful editor from).
  All four tabs share the same `RackController`/connection — connect once at the top, use any tab.

## Debugging

- Set breakpoints directly in `Source/*.cpp`.
- Press **F5** — this uses the `Debug Eleven Rack Controller` configuration in
  `.vscode/launch.json`, which builds first (`preLaunchTask`) and then launches under the MSVC
  debugger (`cppvsdbg`).
- Alternatively, Command Palette > **"CMake: Debug"**.

## Running Tests

The `Source/Rack/` codec/service layer (see [implementation-plan.md](implementation-plan.md)
Milestone 3) is built with a matching test file alongside every source file from the start —
e.g. `SevenBitCodec.cpp` gets `SevenBitCodecTests.cpp` in the same change, not added later. This
matters here specifically because the codec deals in exact byte sequences (SysEx frames, the 5-byte
encoded-int scheme) where a subtle bug is easy to introduce and easy to silently get wrong by eye —
tests pin down "these exact bytes decode to this exact value," often using real hardware-captured
samples from [protocol-spec.md](protocol-spec.md) as known-good test cases.

**Framework**: JUCE's own built-in `juce::UnitTest`/`juce::UnitTestRunner` (part of `juce_core`) —
no extra dependency to fetch, since JUCE is already required. Tests build into a separate console
app target, `RackControllerTests`, wired into CMake's `ctest` so they can run headlessly or from
VS Code without opening the GUI app.

**To run them in VS Code:**
1. Command Palette > **"CMake: Set Build Target"** (or the target picker in the status bar) —
   choose `RackControllerTests`.
2. Command Palette > **"CMake: Build"** to build just that target.
3. Command Palette > **"CMake: Run Tests"** (or **"Test: Run All Tests"**, depending on CMake Tools
   version) — this runs `ctest` and reports pass/fail in the VS Code UI.

**From a terminal**, after building: `ctest --test-dir build --output-on-failure` (adjust the
`build` path if your configure preset/binary dir differs) — prints each `juce::UnitTest`'s
individual assertions and stops on the first failure detail if something breaks.

**"Cannot find file: .../build/DartConfiguration.tcl" when running tests** — harmless, look at
the actual pass/fail summary instead (`100% tests passed...`, exit code 0). That file is only
generated by CMake's full `CTest`/CDash dashboard module; some tooling checks for it out of habit.
Fixed (2026-07-24) by using `include(CTest)` instead of the more minimal `enable_testing()`.

**A test fails but `ctest --output-on-failure` shows no detail about which assertion broke** —
fixed (2026-07-24): `Tests/TestMain.cpp` now prints failure detail via plain `std::cout` instead of
`juce::Logger`, since JUCE's default logger on Windows can end up writing only to the debugger
output (`OutputDebugString`) rather than the console, which `ctest` has nothing to capture. If you
still don't see detail after pulling this fix, run the built `RackControllerTests` executable
directly from a terminal (bypassing `ctest` entirely) to rule out `ctest` itself swallowing output.

**Current test coverage**: `SysExFrame` (frame envelope build/parse), `SevenBitCodec` (the 7-bit
encoding schemes), `EffectDefinitions` (the effect/parameter registry), `MidiTransport` (the JUCE
MIDI I/O wrapper — hardware-independent edge cases only), and `RackController` (the service-layer
facade — real parsing/dispatch verified against every reply captured from actual hardware this
session) — see [implementation-plan.md](implementation-plan.md) Milestone 3 for what's covered so far.

## Testing against the actual hardware

1. Connect the Eleven Rack via USB.
2. In the running app, click **"Refresh Devices"**. If the unit doesn't show up in either
   dropdown, check Windows Device Manager to see how it's actually enumerating — see the open
   question about USB-MIDI class compliance in
   [project-overview.md](project-overview.md).
3. Select it in both the MIDI Input and MIDI Output dropdowns.
4. Click **"Send Identity Request"** and watch the log pane — a reply (or the absence of one)
   is real protocol data, feeding back into [protocol-spec.md](protocol-spec.md).

## Project layout

- `CMakeLists.txt` — build configuration; pulls in JUCE via `FetchContent`, no manual JUCE
  install needed.
- `Source/` — application source. `Main.cpp` is the JUCE app entry point; `MainComponent.*` is
  the one screen that exists so far. `Source/Rack/` (Milestone 3, in progress) holds the codec and
  service layer, each file paired with a `*Tests.cpp` file.
- `Tests/` — the `RackControllerTests` console app that runs every `juce::UnitTest` under
  `Source/Rack/` via CTest — see "Running Tests" above.
- `docs/` — all project documentation; start with `project-overview.md` if you're new here.

## Updating the JUCE version

The JUCE version is pinned in `CMakeLists.txt`'s `FetchContent_Declare` (`GIT_TAG`). Check
https://github.com/juce-framework/JUCE/releases for newer stable releases before bumping it —
this should be a deliberate choice, not an automatic latest-tracking pin.

## Troubleshooting

- **"CMake: Scan for Kits" finds nothing usable** — the "Desktop development with C++" workload
  isn't installed; re-run the Visual Studio Installer and add it.
- **First configure fails to fetch JUCE** — check internet connectivity; corporate networks
  sometimes block plain `git://`/`https://` clones of `github.com` — try from a different network
  to confirm before troubleshooting further.
- **Builds but the MIDI device list is empty** — either nothing is plugged in, or the device is
  enumerating as something other than a standard MIDI class device (see the open question in
  project-overview.md — this is exactly the kind of thing this tool exists to help figure out).
- **App throws/locks up with `_com_error` / "No such interface supported" / `warppal.cpp`** — this
  is JUCE 8's default Direct2D Windows renderer hitting a WARP (software D3D fallback)
  incompatibility on some systems/GPU drivers, not a bug in this app's own code. Fixed
  (2026-07-24) by forcing the software renderer at runtime in `MainComponent::
  parentHierarchyChanged()` (`getPeer()->setCurrentRenderingEngine(0)`), per JUCE core developer
  guidance (JUCE intentionally provides no compile-time flag to disable Direct2D — it must be
  switched at runtime once a window peer exists). If you still see this after pulling the fix, it
  may be worth checking your GPU driver is current.
- **Device shows fine in Device Manager but the app doesn't find/open it, especially after a crash
  or forced-close** — confirmed (2026-07-24) that a **full reboot** resolves this. Consistent with
  the app crash (from the Direct2D issue above) leaving a Windows-level MIDI handle stuck open,
  which blocks a fresh instance from claiming the port even though the hardware itself is fine.
  Try a reboot before assuming it's a hardware/driver problem. (Unplug/replug and killing a leftover
  process in Task Manager are worth trying first, as cheaper options — not confirmed whether either
  alone would have been enough.)

## Known limitation of this guide

This skeleton has not yet been build-verified end-to-end in an automated environment (no
`cmake`/`ninja` were available in the sandbox this was written in, only the MSVC compiler itself)
— you will be the first real build. If configure or build fails in a way not covered above, that's
useful signal, not just an inconvenience — capture the exact error and we'll fix the CMake setup.
