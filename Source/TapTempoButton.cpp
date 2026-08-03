#include "TapTempoButton.h"

namespace
{
    constexpr int kFlashMs = 150;

    // Dark neutral metal, matching RockerSwitchLookAndFeel's own bezel/paddle palette - not the
    // gold knob face - see the class doc comment for why.
    const juce::Colour kBezelColour (0xff050607);
    const juce::Colour kFaceHighlightColour (0xff5a5f64);
    const juce::Colour kFaceMidColour (0xff3a3d40);
    const juce::Colour kFaceShadowColour (0xff26292b);
    const juce::Colour kLedOffColour (0xff4a2f10);
    const juce::Colour kLedOnColour (0xffffb347);
}

TapTempoButton::TapTempoButton()
{
    setRepaintsOnMouseActivity (true); // for the pressed-darken overlay in paint(), see below
}

TapTempoButton::~TapTempoButton() = default;

void TapTempoButton::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto circleBounds = bounds.withSizeKeepingCentre (diameter, diameter);

    g.setColour (kBezelColour);
    g.fillEllipse (circleBounds);

    auto faceBounds = circleBounds.reduced (diameter * 0.065f);

    // Gradient centre offset up-left for a subtle 3D highlight, same construction
    // GoldKnobLookAndFeel uses for its own face - point2 placed directly below point1 so the
    // distance between them (the gradient's radius) is exact, no trig needed.
    juce::Point<float> gradientCentre (faceBounds.getX() + faceBounds.getWidth() * 0.35f,
                                        faceBounds.getY() + faceBounds.getHeight() * 0.3f);
    float gradientRadius = faceBounds.getWidth() * 1.3f;
    juce::ColourGradient faceGradient (kFaceHighlightColour, gradientCentre.x, gradientCentre.y,
                                        kFaceShadowColour, gradientCentre.x, gradientCentre.y + gradientRadius,
                                        true);
    faceGradient.addColour (0.55, kFaceMidColour);
    g.setGradientFill (faceGradient);
    g.fillEllipse (faceBounds);

    // Darken slightly while actually pressed, for a bit of tactile feedback beyond just the LED.
    if (isMouseButtonDown())
    {
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillEllipse (faceBounds);
    }

    float ledDiameter = diameter * 0.21f;
    auto ledBounds = circleBounds.withSizeKeepingCentre (ledDiameter, ledDiameter);

    if (ledLit)
    {
        // Soft glow halo around the LED itself, on top of the face.
        g.setColour (kLedOnColour.withAlpha (0.35f));
        g.fillEllipse (ledBounds.expanded (ledDiameter * 0.4f));
    }

    g.setColour (ledLit ? kLedOnColour : kLedOffColour);
    g.fillEllipse (ledBounds);
}

void TapTempoButton::mouseDown (const juce::MouseEvent&)
{
    ledLit = true;
    repaint();
    startTimer (kFlashMs);

    if (onTap)
        onTap();
}

void TapTempoButton::mouseUp (const juce::MouseEvent&)
{
    repaint(); // clears the pressed-darken overlay
}

void TapTempoButton::timerCallback()
{
    stopTimer();
    ledLit = false;
    repaint();
}
