# Signal chain editor mockup — design notes

Companion notes for [signal-chain-editor-concept.html](signal-chain-editor-concept.html). The HTML
file itself is meant to look like the actual finished editor UI - no status text, dates, or
"still open" caveats cluttering it. Anything about the mockup's own history, the UI/UX interaction
decisions it embodies, what it does/doesn't prove yet, or open design questions lives here instead.

## History

- Saved 2026-07-26, iterated across several turns: uniform block sizing, single-row layout with no
  scroller, then a preset dropdown.
- Ported into the real app as `Source/SignalChainComponent.h`/`.cpp` (2026-07-27) - see
  docs/implementation-plan.md Milestone 5. The HTML mockup was kept around as a design reference,
  not wired to any real data itself.
- The visual/UX redesign iterated on above (dark theme, hard-corner blocks with divider+tooltip,
  Amp/Cab wrapping rectangle, bordered chain panel, label-less preset row, rename popover, Save to
  Unit confirm popover) was ported into the real `SignalChainComponent` (2026-07-28). One behaviour
  is deliberately NOT ported yet: clicking rename-Save or Save-to-Unit-Overwrite does not call
  `RackController::setRigName()`/`saveRig()` - those are still flagged "NOT YET HARDWARE-VALIDATED"
  in `RackController.h`, so the buttons currently only update local UI state and status text. Wiring
  them to real hardware writes is a deliberate, separate decision for later.

## UI/UX design decisions

Every decision here drives real interaction/visual logic the eventual C++ port needs to reproduce
- not just cosmetic choices. Add to this list whenever a new one comes up while iterating on the
mockup, so it's all in one place to review before/during implementation, rather than needing to be
reverse-engineered from the HTML/JS later.

- **Visual style matches the real app's actual dark `LookAndFeel_V4` theme**, confirmed directly
  against a screenshot of the compiled app - not a generic/invented dark palette.
- **Chain blocks**: hard/square corners (not rounded); title pinned top-left with a lighter divider
  line underneath it (not vertically centered); the sub-label (decoded effect name) is shown
  ellipsis-truncated to fit the fixed-width slot, but the full untruncated value is always available
  via a native tooltip (`title` attribute) on hover - so a long name never actually loses
  information, just visible space.
- **Amp/Cab pairing**: wrapped in their own shared bordered rectangle, distinct from each block's
  individual border, to visually communicate the fixed-adjacency constraint (Amp always immediately
  before Cab) - while each block keeps its own independent border/selection state and stays
  independently clickable/selectable inside the group.
- **Chain row panel**: the whole row sits inside one bordered panel filled with a background
  matching the app's dropdown/`ComboBox` surface colour; each individual slot then fills its own
  explicit darker background (matching the app's base window colour) so it visually "sits on top
  of" that panel instead of blending into it. Row items are vertically centered, not top-aligned,
  since the Amp/Cab group's outer border makes it taller than a single plain block.
- **Section labels sit directly above their content** with a tight margin (e.g. "Signal chain"), not
  floated with extra breathing room.
- **"Preset" has no separate label at all** - the dropdown's own placeholder text ("Select a
  preset...") carries that meaning instead of a redundant label sitting above or beside it.
- **Renaming a preset is a deliberate, separate action, not inline-editable or auto-saved on
  blur.** A dedicated pencil-icon button opens a small popover with explicit Cancel/Save buttons.
  Reasoning: saving a new name is a real write to the hardware (`RackController::setRigName()`,
  not yet hardware-validated - see docs/protocol-spec.md/implementation-plan.md), so it should
  require an unambiguous, confirmable moment before committing - consistent with how "Select Rig"
  is already treated as its own deliberate action elsewhere in the app (`RigBrowserComponent`/
  Diagnostics tab), not a side effect of some other interaction.
- **Rename popover shows the bank/slot location prefix (e.g. "Bank 0 A3") as fixed, non-editable
  static text - only the actual rig name is ever in the editable field.** The location comes from
  the rig's physical `RackController::RigId` (bank/slot position), which can never be changed;
  only `setRigName()`'s actual name argument is what's editable. Pre-filling the whole combined
  label ("Bank 0 A3: SLO 100") into one editable field, as an earlier version of this mockup did,
  would have made it look like the location itself was editable too, which it isn't.
- **"Save to Unit" (writes the currently-shown settings back to the loaded rig's slot) gets its own
  confirmation popover, styled deliberately differently from the rename Save.** The confirm button
  reads "Overwrite" (not "Save") and is coloured as a warning (red, not the plain blue accent),
  because the underlying action - `RackController::saveRig()` - overwrites whatever is currently
  stored at that rig slot on the real unit, with **no undo**, and per the project's own existing
  code comments isn't hardware-validated yet. This is a higher-stakes action than renaming (which
  only ever changes a label), so it needed a visually distinct "this is different/riskier" treatment,
  not just the same popover pattern reused verbatim. The button is disabled under the same condition
  as rename (no real preset selected, only the "Select a preset..." placeholder).
- **Chain slots use a fixed width per block (68px) - they do NOT grow/stretch to fill the chain
  panel.** Tried and explicitly reverted (2026-07-27): first attempt was uncapped flex-grow (slots
  fill whatever width is available), which broke because `.chain` itself never stretched to fill
  `.chain-panel` in the first place, leaving a dead gap on one side. Fixed that, then tried capping
  growth at `7vmin` (scales with the smaller of the viewport's width/height) with the row centered
  once every slot hit that cap - visually this looked worse than the plain fixed-width row, so the
  whole stretch-to-fill approach was abandoned rather than iterated on further. **Do not re-attempt
  this without discussing it again** - fixed-width, left-aligned blocks (matching the real app's
  `kBlockWidth`) is the settled design.
- **Every button's disabled state looks visually disabled** (dimmed via `opacity`, default cursor) -
  generalized from a rule that originally only applied to the pencil icon button to apply to every
  `<button>` in this UI (including "Save to Unit"), so any future disabled control gets this for
  free instead of needing a one-off rule added each time.
- **Chain blocks are drag-and-drop reorderable (2026-07-28, implemented directly in the real app -
  not prototyped in this HTML mockup first), with three fixed rules**: Input is always first, Output
  is always last, and neither can be picked up at all. Amp/Cab can't be picked up either, but their
  combined position among the other blocks CAN still shift as a side effect of some OTHER block
  being dragged past them - dropping a block anywhere before or after the Amp/Cab pair is fine, just
  never in between Amp and Cab, since they must stay adjacent (one combined effect in
  `EffectDefinitions`). Implemented via `juce::DragAndDropContainer`/`DragAndDropTarget` rather than
  making every block hit-test its own left/right half: `SignalChainComponent`'s single
  `ChainDropArea` (wrapping the whole row) already knows every arrow-gap's exact x-position from
  laying out the row, and simply never records the one gap between an "amp" and "cab" block as a
  valid drop point - the pair's atomicity falls out of the existing layout code instead of needing
  special-cased logic. **This is a local/UI-only reorder** - it rearranges the in-memory chain order
  and repaints, with no `RackController` call and no effect on the real unit (see "Still open" below
  for the still-unresolved question of whether the unit even supports writing a new order back).

## Confirmed real-hardware structure (as of 2026-07-27)

- Input is always first, Output is always last - both fixed, and neither appears in the Bulk Rig
  payload at all (so this is the one part of the structure not independently confirmed the same way
  as everything else below - it's what the unit's own manual describes).
- Amp and Cab are separate blocks, but always adjacent - Amp immediately before Cab. **Correction**:
  an earlier version of this mockup (and these notes) said this pair's overall *position* was fixed
  too - that turned out to be wrong. Only the adjacency is fixed; the Amp/Cab pair's position among
  the other blocks varies per rig just like everything else (confirmed against a real rig, "SLO
  100" - see docs/protocol-spec.md "twenty-second round").
- Every other block (Wah, Volume Pedal, Distortion, Mod, Delay, Reverb, FX Loop, FX1, FX2) can sit
  anywhere else in the chain, and real rigs do arrange them differently from each other.
- The chain's real per-rig order is fully decodable now: `BulkRigParser::ParsedRig::slots` is
  already in real signal-chain position order (letters C through L) - confirmed against two real
  rigs, not just inferred. See `SignalChainComponent::updateBlockDataFromRig()` for how the real app
  uses this.

## Still open

- Whether the unit exposes chain *reordering* via MIDI/SysEx at all, or whether this stays an
  app-only concept with no real effect on the unit - the UI side (drag-and-drop reordering) now
  exists (2026-07-28, see "UI/UX design decisions" above), but it's purely local; the underlying
  protocol question is unchanged - see docs/implementation-plan.md "Not yet scheduled / parked".
- Whether Cab has independently configurable parameters (cabinet type, mic model, mic position)
  beyond what's currently modeled - Amp/Cab is one combined effect in `EffectDefinitions`, so Cab has
  no independent data to show today.
- Volume Pedal, Amp, Cab, and FX Loop have no `SlotConfig` entry yet in the real app, so clicking
  those blocks shows a "not yet editable" fallback rather than real controls - see
  docs/implementation-plan.md "Not yet scheduled / parked".
- This mockup's block styling/interaction pattern reuses the same control types (knob/toggle/
  selector) the real `SlotParamsPanel` uses - not yet reconciled with how reordering itself would be
  triggered (drag handles, arrows, etc.), since reordering isn't implemented in either the mockup or
  the real app yet.
