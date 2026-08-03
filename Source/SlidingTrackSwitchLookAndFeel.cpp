#include "SlidingTrackSwitchLookAndFeel.h"
#include "TwoOptionSwitchLabels.h"

namespace
{
    // Matched against the reviewed mockup's own pixel proportions (a 220x40 switch, 3px border,
    // 100px-wide thumb over the remaining 214px of travel).
    constexpr float kSwitchAspect = 220.0f / 40.0f;

    const juce::Colour kHousingColour (0xff050607);
    const juce::Colour kTrackColour (0xff2c2f32);
    const juce::Colour kThumbHighlightColour (0xfff6dfa0);
    const juce::Colour kThumbShadowColour (0xff7a5a1c);
    const juce::Colour kLabelLitColour (0xff1c1f22);
    const juce::Colour kLabelDimColour (0xff6b7178);
}

void SlidingTrackSwitchLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                                       bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat();
    float switchHeight = bounds.getHeight();
    float switchWidth = juce::jmin (bounds.getWidth(), switchHeight * kSwitchAspect);
    auto switchBounds = bounds.withWidth (switchWidth);

    bool isOn = button.getToggleState();

    g.setColour (kHousingColour);
    g.fillRoundedRectangle (switchBounds, switchHeight * 0.5f);

    float border = switchHeight * 0.075f;
    auto trackBounds = switchBounds.reduced (border);
    g.setColour (kTrackColour);
    g.fillRoundedRectangle (trackBounds, trackBounds.getHeight() * 0.5f);

    float thumbWidth = trackBounds.getWidth() * 0.46f;
    auto thumbBounds = trackBounds.withWidth (thumbWidth);
    if (isOn)
        thumbBounds = thumbBounds.withX (trackBounds.getRight() - thumbWidth);

    juce::ColourGradient thumbGradient (kThumbHighlightColour, thumbBounds.getX(), thumbBounds.getY(),
                                         kThumbShadowColour, thumbBounds.getRight(), thumbBounds.getBottom(), true);
    g.setGradientFill (thumbGradient);
    g.fillRoundedRectangle (thumbBounds, thumbBounds.getHeight() * 0.5f);

    // Both words sit at their own end of the track at all times - the thumb slides on top of
    // whichever one is currently active, so that word's colour flips dark-on-gold while the other
    // stays a dim grey against the plain track.
    auto labelFont = juce::Font (juce::FontOptions (juce::jmax (9.0f, switchHeight * 0.3f))).boldened();
    g.setFont (labelFont);
    auto offLabelBounds = trackBounds.withWidth (thumbWidth);
    auto onLabelBounds = trackBounds.withX (trackBounds.getRight() - thumbWidth).withWidth (thumbWidth);
    g.setColour (isOn ? kLabelDimColour : kLabelLitColour);
    g.drawText (getTwoOptionOffLabel (button), offLabelBounds.toNearestInt(), juce::Justification::centred);
    g.setColour (isOn ? kLabelLitColour : kLabelDimColour);
    g.drawText (getTwoOptionOnLabel (button), onLabelBounds.toNearestInt(), juce::Justification::centred);
}
