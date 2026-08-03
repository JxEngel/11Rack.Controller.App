#include "LeverDotSwitchLookAndFeel.h"
#include "TwoOptionSwitchLabels.h"

namespace
{
    constexpr float kSwitchAspect = 220.0f / 42.0f;

    const juce::Colour kTrackColour (0xff2c2f32);
    const juce::Colour kDotHighlightColour (0xfff6dfa0);
    const juce::Colour kDotShadowColour (0xff7a5a1c);
    const juce::Colour kLabelLitColour (0xfff2c14e);
    const juce::Colour kLabelDimColour (0xff6b7178);
}

void LeverDotSwitchLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                                   bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat();
    float switchHeight = bounds.getHeight();
    float switchWidth = juce::jmin (bounds.getWidth(), switchHeight * kSwitchAspect);
    auto switchBounds = bounds.withWidth (switchWidth);

    bool isOn = button.getToggleState();

    float trackAreaHeight = switchHeight * 0.45f;
    auto trackArea = switchBounds.withHeight (trackAreaHeight);
    auto labelArea = switchBounds.withY (trackArea.getBottom()).withHeight (switchHeight - trackAreaHeight);

    float dotDiameter = trackAreaHeight * 0.85f;
    auto trackLine = trackArea.withSizeKeepingCentre (trackArea.getWidth() - dotDiameter, trackArea.getHeight() * 0.22f);
    g.setColour (kTrackColour);
    g.fillRoundedRectangle (trackLine, trackLine.getHeight() * 0.5f);

    float dotCentreX = isOn ? trackLine.getRight() : trackLine.getX();
    float dotCentreY = trackArea.getCentreY();
    auto dotBounds = juce::Rectangle<float> (dotDiameter, dotDiameter).withCentre ({ dotCentreX, dotCentreY });

    juce::ColourGradient dotGradient (kDotHighlightColour, dotBounds.getX(), dotBounds.getY(),
                                       kDotShadowColour, dotBounds.getRight(), dotBounds.getBottom(), true);
    g.setGradientFill (dotGradient);
    g.fillEllipse (dotBounds);

    // Active word lit gold, matching the dot's own end; inactive stays a dim grey.
    auto labelFont = juce::Font (juce::FontOptions (juce::jmax (9.0f, switchHeight * 0.26f))).boldened();
    g.setFont (labelFont);
    auto offLabelBounds = labelArea.withWidth (labelArea.getWidth() * 0.5f);
    auto onLabelBounds = labelArea.withX (offLabelBounds.getRight()).withWidth (labelArea.getWidth() - offLabelBounds.getWidth());
    g.setColour (isOn ? kLabelDimColour : kLabelLitColour);
    g.drawText (getTwoOptionOffLabel (button), offLabelBounds.toNearestInt(), juce::Justification::centred);
    g.setColour (isOn ? kLabelLitColour : kLabelDimColour);
    g.drawText (getTwoOptionOnLabel (button), onLabelBounds.toNearestInt(), juce::Justification::centred);
}
