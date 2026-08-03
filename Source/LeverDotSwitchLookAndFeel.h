#pragma once
#include <JuceHeader.h>

// TwoOptionSwitchStyle::leverDot ("C") - a thin horizontal track with a small gold position dot
// that sits at the left end ("off") or right end ("on"); the two option words (read via
// TwoOptionSwitchLabels.h) sit below each end, the active one lit gold. See
// SegmentedSwitchLookAndFeel.h for how the two-option/on-off split works and why this is a
// separate control from RockerSwitchLookAndFeel's plain on/off rocker.
class LeverDotSwitchLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                            bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};
