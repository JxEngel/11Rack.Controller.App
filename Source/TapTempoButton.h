#pragma once

#include <JuceHeader.h>

// Custom circular "Tap Tempo" push button (2026-08-03) - hand-drawn, same visual family as
// GoldKnobLookAndFeel/RockerSwitchLookAndFeel but NOT a LookAndFeel override: a momentary LED
// flash needs its own per-instance timed state (how long since it was last tapped), which a
// shared, stateless LookAndFeel object can't hold - drawn directly in its own Component instead.
//
// Dark neutral metal face (matching RockerSwitchLookAndFeel's bezel/paddle palette, not the gold
// knob face) with a small center LED that flashes amber briefly on each press, then fades back to
// its dim off colour - a gold face and a whole-rim-glow-instead-of-a-dot variant were both
// reviewed and passed over via an interactive mockup first, see the conversation this was built
// from. Purely a visual "yes, that tap was sent" acknowledgement - there's no way to know or
// display the actual tempo the unit ends up at (see docs/master-control-map.md), so this never
// claims to be a synced tempo indicator, just a one-shot flash per tap.
//
// Fires on mouseDown, not the more typical mouseUp-inside-bounds a plain juce::Button uses - a
// physical tap is registered the instant you make contact, not only once you lift off again.
class TapTempoButton : public juce::Component,
                        private juce::Timer
{
public:
    TapTempoButton();
    ~TapTempoButton() override;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    // Fired immediately on mouseDown - the actual CC send lives with the caller (see
    // SignalChainComponent), this component only owns the visual flash.
    std::function<void()> onTap;

private:
    void timerCallback() override;

    bool ledLit = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapTempoButton)
};
