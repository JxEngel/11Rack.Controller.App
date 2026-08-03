#pragma once

// Shared between DisplayOptionsComponent (the "Display Options" tab's picker, 2026-08-03) and
// SlotParamsPanel (the consumer, via its setKnobStyle()) - a standalone header rather than a
// nested enum in either class, since both need the type and neither should have to include the
// other just to get it.
//
// Every knob-kind control always renders as an actual knob - there is deliberately no "flat"/
// plain-slider option (removed 2026-08-03, per the user's explicit call: knobs should always look
// like knobs, this enum is for choosing WHICH knob look, not whether to have one at all).
// "goldMetallic" is GoldKnobLookAndFeel's hand-drawn rotary knob - see its own doc comment for how
// it was designed and how to extend it with a real image-based style later.
//
// Adding a new style later means: add an enumerator here, add its juce::LookAndFeel_V4 subclass
// (see GoldKnobLookAndFeel.h for the pattern), add one case in SlotParamsPanel::applyKnobStyle()'s
// switch (the compiler warns on a missing case), add one item to DisplayOptionsComponent's
// knobStyleSelector - no other code changes.
enum class KnobStyle { goldMetallic };
