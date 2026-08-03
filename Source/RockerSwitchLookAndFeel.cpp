#include "RockerSwitchLookAndFeel.h"

namespace
{
    // Proportions matched against the reviewed mockup (a 52px-wide, 70px-tall switch) - width is
    // derived from whatever height the button ends up with, not a fixed pixel size, so it scales
    // with the row height SlotParamsPanel/SignalChainComponent give it.
    constexpr float kSwitchAspect = 52.0f / 70.0f;

    const juce::Colour kHousingColour (0xff050607);
    const juce::Colour kPaddleUnlitColour (0xff2c2f32);
    const juce::Colour kPaddleLitTopColour (0xfff6dfa0);
    const juce::Colour kPaddleLitBottomColour (0xffd9a83f);
    const juce::Colour kSeamColour (0xff0b0d0e);
    const juce::Colour kLabelLitColour (0xff1c1f22);   // dark text, reads against the gold fill
    const juce::Colour kLabelDimColour (0xff6b7178);   // muted - the half NOT currently selected
    const juce::Colour kLabelSelectedColour (0xffe8e8e8); // bright - selected but not gold (OFF state)
}

void RockerSwitchLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                                 bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat();
    float switchHeight = bounds.getHeight();
    float switchWidth = switchHeight * kSwitchAspect;
    auto switchBounds = bounds.withWidth (switchWidth);

    bool isOn = button.getToggleState();

    // Dark bezel housing.
    g.setColour (kHousingColour);
    g.fillRoundedRectangle (switchBounds, switchWidth * 0.18f);

    // Paddle, split into ON (top) / OFF (bottom) halves - whichever is active gets the gold fill;
    // the other stays the same plain grey regardless of state, since there's no separate pilot
    // light (an earlier reviewed revision had one, removed per the user's explicit call) - state
    // reads purely from which half is gold.
    auto paddleBounds = switchBounds.reduced (switchWidth * 0.12f, switchHeight * 0.06f);
    auto onHalf = paddleBounds.withHeight (paddleBounds.getHeight() * 0.5f);
    auto offHalf = paddleBounds.withY (onHalf.getBottom()).withHeight (paddleBounds.getHeight() - onHalf.getHeight());
    float cornerSize = switchWidth * 0.14f;

    g.setColour (kPaddleUnlitColour);
    g.fillRoundedRectangle (paddleBounds, cornerSize);

    if (isOn)
    {
        juce::ColourGradient onGradient (kPaddleLitTopColour, onHalf.getX(), onHalf.getY(),
                                          kPaddleLitBottomColour, onHalf.getX(), onHalf.getBottom(), false);
        g.setGradientFill (onGradient);
        g.fillRoundedRectangle (onHalf, cornerSize);
    }

    // Thin seam between the two halves, matching a real rocker's physical split.
    g.setColour (kSeamColour);
    g.fillRect (juce::Rectangle<float> (paddleBounds.getX(), onHalf.getBottom() - 0.5f, paddleBounds.getWidth(), 1.0f));

    // "ON"/"OFF" labels - only drawn once the row is tall enough to make them legible (see
    // SlotParamsPanel/SignalChainComponent's toggle row height); skipped below that rather than
    // rendering illegible noise.
    if (switchHeight >= 36.0f)
    {
        auto labelFont = juce::Font (juce::FontOptions (juce::jmax (7.0f, switchHeight * 0.16f))).boldened();
        g.setFont (labelFont);
        g.setColour (isOn ? kLabelLitColour : kLabelDimColour);
        g.drawText ("ON", onHalf.toNearestInt(), juce::Justification::centred);
        g.setColour (isOn ? kLabelDimColour : kLabelSelectedColour);
        g.drawText ("OFF", offHalf.toNearestInt(), juce::Justification::centred);
    }

    // Button text (e.g. "Bypass") - some ToggleButtons have it, most toggle-kind params don't and
    // use a separate juce::Label instead (see the class doc comment) - drawn to the right of the
    // switch, mimicking juce::LookAndFeel_V4's own default layout.
    if (button.getButtonText().isNotEmpty())
    {
        auto textBounds = bounds.withTrimmedLeft (switchWidth + 8.0f);
        g.setColour (button.findColour (juce::ToggleButton::textColourId));
        g.setFont (juce::Font (juce::FontOptions (juce::jmin (15.0f, switchHeight * 0.5f))));
        g.drawText (button.getButtonText(), textBounds.toNearestInt(), juce::Justification::centredLeft);
    }
}
