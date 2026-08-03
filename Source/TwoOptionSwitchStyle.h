#pragma once

// Mirrors KnobStyle.h/ToggleStyle.h's own reasoning (2026-08-03) - a standalone header shared
// between DisplayOptionsComponent (the picker) and SlotParamsPanel (the consumer, via
// setTwoOptionSwitchStyle()).
//
// Distinct from ToggleStyle.h's rocker switch: a rocker is for a genuine on/off state (Bypass,
// "Vibrato On/Off") where "OFF" is always the right word. A two-option switch is for a param that
// picks between two named alternatives that are never really "off" (e.g. Chorus/Vibrato's Mode,
// Multi Chorus's Tri/Sine, Volume Pedal's Linear/Log) - it shows the two real option words instead
// of a generic ON/OFF.
//
// Three visual candidates were reviewed via an interactive mockup - rather than pick one, the user
// asked to keep all three as selectable options, in this order (segmentedSplit is shown first and
// is the default):
//   A. segmentedSplit - a horizontal split of the rocker switch's own paddle, whichever word is
//      active lit gold (same visual DNA as RockerSwitchLookAndFeel, rotated 90 degrees)
//   B. slidingTrack - a pill-shaped track with a gold thumb sliding to whichever word is active
//   C. leverDot - a thin track with a small gold position dot, the active word's text lit gold
//
// Adding a later fourth style means: add an enumerator here, add its juce::LookAndFeel_V4
// subclass (see SegmentedSwitchLookAndFeel.h for the pattern), add one case in SlotParamsPanel::
// applyTwoOptionSwitchStyle()'s switch (the compiler warns on a missing case), add one item to
// DisplayOptionsComponent's twoOptionSwitchStyleSelector - no other code changes.
enum class TwoOptionSwitchStyle { segmentedSplit, slidingTrack, leverDot };
