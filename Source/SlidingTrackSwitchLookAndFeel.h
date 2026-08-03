#pragma once
#include <JuceHeader.h>

// TwoOptionSwitchStyle::slidingTrack ("B") - a pill-shaped track with a gold thumb that slides
// between the two option words (e.g. "Chorus"/"Vibrato", read via TwoOptionSwitchLabels.h). See
// SegmentedSwitchLookAndFeel.h for how the two-option/on-off split works and why this is a
// separate control from RockerSwitchLookAndFeel's plain on/off rocker.
class SlidingTrackSwitchLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                            bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};
