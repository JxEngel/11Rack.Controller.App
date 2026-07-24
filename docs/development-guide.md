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
- What you'll see right now: a small MIDI utility, not the final editor. It lists available MIDI
  input/output devices, lets you open one of each, logs all incoming MIDI traffic as hex, and has
  a button to send a Universal SysEx Identity Request — a basic first connectivity test. This is
  the seed of the interface layer described in
  [implementation-plan.md](implementation-plan.md) Milestone 3, not the UI in Milestone 5.

## Debugging

- Set breakpoints directly in `Source/*.cpp`.
- Press **F5** — this uses the `Debug Eleven Rack Controller` configuration in
  `.vscode/launch.json`, which builds first (`preLaunchTask`) and then launches under the MSVC
  debugger (`cppvsdbg`).
- Alternatively, Command Palette > **"CMake: Debug"**.

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
  the one screen that exists so far.
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
