#include "GoldKnobLookAndFeel.h"

namespace
{
    // All ratios below are fractions of the knob's full diameter, matched against the reviewed
    // mockup's own pixel proportions (180px-diameter preview: bezel r=46, face r=42, dot ring
    // r=66, dot r=3, pointer length=32 from centre) - see GoldKnobLookAndFeel.h.
    constexpr float kBezelRadiusRatio = 46.0f / 180.0f;
    constexpr float kFaceRadiusRatio = 42.0f / 180.0f;
    constexpr float kDotRingRadiusRatio = 66.0f / 180.0f;
    constexpr float kDotRadiusRatio = 3.0f / 180.0f;
    constexpr float kPointerLengthRatio = 32.0f / 180.0f;
    constexpr float kPointerStrokeRatio = 3.0f / 180.0f;

    const juce::Colour kDotLitColour (0xfff2c14e);
    const juce::Colour kDotDimColour (0xff4a4f54);
    const juce::Colour kBezelColour (0xff15181b);
    const juce::Colour kFaceHighlightColour (0xfff6dfa0);
    const juce::Colour kFaceMidColour (0xffd9a83f);
    const juce::Colour kFaceShadowColour (0xff7a5a1c);
    const juce::Colour kPointerColour (0xfffdf6e3);
}

void GoldKnobLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                             float rotaryStartAngle, float rotaryEndAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    auto diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto centre = bounds.getCentre();

    // Dot ring - each dot lit (warm gold) or dim (muted grey) depending on whether it's at or
    // before the current value's position in the sweep, giving an at-a-glance sense of the value
    // without reading the text box (confirmed with the user - progressive lighting, not purely
    // decorative dots).
    float dotRingRadius = diameter * kDotRingRadiusRatio;
    float dotRadius = diameter * kDotRadiusRatio;
    int litCount = juce::roundToInt (sliderPos * (float) (kNumDots - 1));

    for (int i = 0; i < kNumDots; ++i)
    {
        float t = (float) i / (float) (kNumDots - 1);
        float angle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
        juce::Point<float> dotCentre (centre.x + dotRingRadius * std::sin (angle),
                                       centre.y - dotRingRadius * std::cos (angle));

        g.setColour (i <= litCount ? kDotLitColour : kDotDimColour);
        g.fillEllipse (juce::Rectangle<float> (dotRadius * 2.0f, dotRadius * 2.0f).withCentre (dotCentre));
    }

    // Dark bezel ring, then the gold brushed-metal face on top of it - gives the face a subtle
    // raised/inset look against the surrounding panel, matching the reviewed mockup.
    float bezelRadius = diameter * kBezelRadiusRatio;
    g.setColour (kBezelColour);
    g.fillEllipse (juce::Rectangle<float> (bezelRadius * 2.0f, bezelRadius * 2.0f).withCentre (centre));

    float faceRadius = diameter * kFaceRadiusRatio;
    juce::Rectangle<float> faceBounds (faceRadius * 2.0f, faceRadius * 2.0f);
    faceBounds.setCentre (centre);

    // Gradient centre offset up-left from the face's true centre (matching the mockup's
    // cx="35%" cy="30%" SVG radial gradient) to suggest a light source catching brushed metal -
    // point2 is placed directly below point1 so the distance between them (the gradient's radius)
    // is exact, rather than derived from an offset diagonal that would need trig to get right.
    juce::Point<float> gradientCentre (centre.x - faceRadius * 0.35f, centre.y - faceRadius * 0.4f);
    float gradientRadius = faceRadius * 1.3f;
    juce::ColourGradient goldGradient (kFaceHighlightColour, gradientCentre.x, gradientCentre.y,
                                        kFaceShadowColour, gradientCentre.x, gradientCentre.y + gradientRadius,
                                        true);
    goldGradient.addColour (0.45, kFaceMidColour);
    g.setGradientFill (goldGradient);
    g.fillEllipse (faceBounds);

    // Pointer - a short light line from the centre toward the rim, rotated to the current value's
    // angle, showing rotation at a glance the same way a real knob's painted indicator line would.
    float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    float pointerLength = diameter * kPointerLengthRatio;
    juce::Point<float> pointerEnd (centre.x + pointerLength * std::sin (angle),
                                    centre.y - pointerLength * std::cos (angle));

    g.setColour (kPointerColour);
    g.drawLine (juce::Line<float> (centre, pointerEnd), diameter * kPointerStrokeRatio);
}
