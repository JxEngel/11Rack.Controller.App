#pragma once

// Shared between DisplayOptionsComponent (the "Display Options" tab's picker) and SlotParamsPanel/
// SignalChainComponent (the consumers, via their setToggleStyle()) - mirrors KnobStyle.h's exact
// pattern (own standalone header, one enumerator today, room for more later) for the same reason:
// both a picker and a consumer need the type, and neither should include the other just for it.
//
// "rockerSwitch" is RockerSwitchLookAndFeel's hand-drawn on/off rocker - see its own doc comment.
// There is deliberately no plain-checkbox fallback, same reasoning as KnobStyle's "always a knob,
// never flat": every toggle-kind control always renders as an actual switch.
//
// Adding a new style later means: add an enumerator here, add its juce::LookAndFeel_V4 subclass
// (see RockerSwitchLookAndFeel.h for the pattern), add one case in SlotParamsPanel::
// applyToggleStyle()'s switch (the compiler warns on a missing case), add one item to
// DisplayOptionsComponent's toggleStyleSelector - no other code changes.
enum class ToggleStyle { rockerSwitch };
