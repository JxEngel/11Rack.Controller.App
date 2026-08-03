#pragma once

#include <JuceHeader.h>

// Custom rotary "gold knob" look (2026-08-03) - hand-drawn, not from a third-party library. A Gin
// (FigBug/Gin) spike was tried first for this (see docs/implementation-plan.md), but its
// GinLookAndFeel only draws a thin progress-arc, not an actual knob body - dropped once that
// became clear. Modeled loosely after a common brushed-brass rotary knob style (dark bezel, warm
// gold radial-gradient face, a ring of small LED-style dots that light up progressively as the
// value rises, a light pointer line) - direction reviewed and approved via an interactive mockup
// before this was written (see the conversation this was built from).
//
// Only overrides drawRotarySlider() - every other control (buttons, combo boxes, linear sliders
// elsewhere in the app) keeps the default juce::LookAndFeel_V4 look untouched.
//
// Future extension point: if a real image-based knob asset is ever wanted instead of this
// hand-drawn version, drawRotarySlider() is the one place to swap - e.g. g.drawImageTransformed()
// with a rotated bitmap, or picking a frame from a filmstrip image based on sliderPos - the rest
// of this class and every caller stays the same.
class GoldKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                            float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override;

private:
    // Number of dot indicators around the rim, and every other proportion below - all matched
    // directly against the reviewed mockup's pixel ratios (e.g. dot ring radius was 66/180 of the
    // full knob diameter there), not independently re-derived.
    static constexpr int kNumDots = 13;
};
