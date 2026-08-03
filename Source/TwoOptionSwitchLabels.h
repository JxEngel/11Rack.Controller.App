#pragma once
#include <JuceHeader.h>

// Shared by all three TwoOptionSwitchStyle LookAndFeel classes (SegmentedSwitchLookAndFeel,
// SlidingTrackSwitchLookAndFeel, LeverDotSwitchLookAndFeel, see TwoOptionSwitchStyle.h). Unlike
// RockerSwitchLookAndFeel's fixed "ON"/"OFF" text, each two-option switch shows two words that
// differ per param instance (e.g. "Chorus"/"Vibrato" vs "Tri"/"Sine") - a shared, stateless
// LookAndFeel object can't hold that, so it travels as properties on the juce::ToggleButton itself
// (set once in SlotParamsPanel via setTwoOptionSwitchLabels(), alongside setLookAndFeel()) - the
// same "per-instance data lives on the component, not the LookAndFeel" principle TapTempoButton's
// own doc comment explains for its timed LED state.
inline void setTwoOptionSwitchLabels (juce::ToggleButton& button, const juce::String& offLabel, const juce::String& onLabel)
{
    button.getProperties().set ("twoOptionOffLabel", offLabel);
    button.getProperties().set ("twoOptionOnLabel", onLabel);
}

// Non-const to match juce::Component::getProperties() (no const overload exists) - matches how
// drawToggleButton() itself always receives a non-const juce::ToggleButton&.
inline juce::String getTwoOptionOffLabel (juce::ToggleButton& button)
{
    return button.getProperties().getWithDefault ("twoOptionOffLabel", "Off").toString();
}

inline juce::String getTwoOptionOnLabel (juce::ToggleButton& button)
{
    return button.getProperties().getWithDefault ("twoOptionOnLabel", "On").toString();
}
