#pragma once

#include <JuceHeader.h>

// Custom on/off rocker switch look (2026-08-03) - hand-drawn, same approach as
// GoldKnobLookAndFeel: a real physical-switch silhouette (dark bezel housing, a paddle split into
// ON/OFF halves, whichever is active lit gold) rather than a plain checkbox. Reviewed via an
// interactive mockup (three candidate styles, then a revision) before this was written - the
// approved design deliberately has NO separate pilot light (an earlier revision had one, removed
// per the user's explicit call) - state is shown purely by which half of the paddle is lit.
//
// Only overrides drawToggleButton() - every juce::ToggleButton in the app (Bypass toggles and
// every other toggle-kind param) renders this way once styled via SlotParamsPanel::
// setToggleStyle()/SignalChainComponent::setToggleStyle(), the same "one LookAndFeel instance,
// applied everywhere, change it once and it's everywhere" pattern GoldKnobLookAndFeel established
// for knobs (see ToggleStyle.h).
//
// The switch's own button text (e.g. "Bypass" - some ToggleButtons have it, most toggle-kind
// params don't and use a separate juce::Label instead, see SlotParamsPanel.cpp) is drawn to the
// right of the switch glyph, mimicking juce::LookAndFeel_V4's own default layout, so callers don't
// need to change how they set up their ToggleButton's text.
class RockerSwitchLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                            bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};
