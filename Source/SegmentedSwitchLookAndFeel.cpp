#include "SegmentedSwitchLookAndFeel.h"
#include "TwoOptionSwitchLabels.h"

namespace
{
    // Matched against the reviewed mockup's own pixel proportions (a 220x44 switch) - width is
    // capped by whatever's actually available, not forced to this ratio, so a narrow row still
    // fits (see drawToggleButton()'s jmin below).
    constexpr float kSwitchAspect = 220.0f / 44.0f;

    const juce::Colour kHousingColour (0xff050607);
    const juce::Colour kPaddleUnlitColour (0xff2c2f32);
    const juce::Colour kPaddleLitLeftColour (0xfff6dfa0);
    const juce::Colour kPaddleLitRightColour (0xffd9a83f);
    const juce::Colour kSeamColour (0xff0b0d0e);
    const juce::Colour kLabelLitColour (0xff1c1f22);
    const juce::Colour kLabelDimColour (0xff6b7178);
}

void SegmentedSwitchLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                                    bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat();
    float switchHeight = bounds.getHeight();
    float switchWidth = juce::jmin (bounds.getWidth(), switchHeight * kSwitchAspect);
    auto switchBounds = bounds.withWidth (switchWidth);

    bool isOn = button.getToggleState();

    g.setColour (kHousingColour);
    g.fillRoundedRectangle (switchBounds, switchHeight * 0.22f);

    // Paddle, split into OFF (left) / ON (right) halves - matches RockerSwitchLookAndFeel's own
    // "only the active half is gold, the other stays plain grey" state model.
    auto paddleBounds = switchBounds.reduced (switchWidth * 0.02f, switchHeight * 0.12f);
    auto offHalf = paddleBounds.withWidth (paddleBounds.getWidth() * 0.5f);
    auto onHalf = paddleBounds.withX (offHalf.getRight()).withWidth (paddleBounds.getWidth() - offHalf.getWidth());
    float cornerSize = switchHeight * 0.16f;

    g.setColour (kPaddleUnlitColour);
    g.fillRoundedRectangle (paddleBounds, cornerSize);

    auto& litHalf = isOn ? onHalf : offHalf;
    juce::ColourGradient litGradient (kPaddleLitLeftColour, litHalf.getX(), litHalf.getY(),
                                       kPaddleLitRightColour, litHalf.getRight(), litHalf.getY(), false);
    g.setGradientFill (litGradient);
    g.fillRoundedRectangle (litHalf, cornerSize);

    // Thin seam between the two halves, matching RockerSwitchLookAndFeel's own physical-split seam.
    g.setColour (kSeamColour);
    g.fillRect (juce::Rectangle<float> (offHalf.getRight() - 0.5f, paddleBounds.getY(), 1.0f, paddleBounds.getHeight()));

    auto labelFont = juce::Font (juce::FontOptions (juce::jmax (9.0f, switchHeight * 0.28f))).boldened();
    g.setFont (labelFont);
    g.setColour (isOn ? kLabelDimColour : kLabelLitColour);
    g.drawText (getTwoOptionOffLabel (button), offHalf.toNearestInt(), juce::Justification::centred);
    g.setColour (isOn ? kLabelLitColour : kLabelDimColour);
    g.drawText (getTwoOptionOnLabel (button), onHalf.toNearestInt(), juce::Justification::centred);
}
