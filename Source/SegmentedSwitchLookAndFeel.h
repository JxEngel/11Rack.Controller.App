#pragma once
#include <JuceHeader.h>

// TwoOptionSwitchStyle::segmentedSplit ("A") - a horizontal split of RockerSwitchLookAndFeel's own
// paddle (same dark bezel/gold-gradient/seam visual DNA), rotated 90 degrees: left half is the
// "off" option, right half is "on", whichever is active gets the gold fill. The two option words
// (e.g. "Chorus"/"Vibrato") are per-instance text read via TwoOptionSwitchLabels.h, not fixed
// ON/OFF text like RockerSwitchLookAndFeel - see TwoOptionSwitchStyle.h for why this is a separate
// control from the plain on/off rocker.
class SegmentedSwitchLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                            bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};
