#pragma once

#include <JuceHeader.h>
#include "Rack/RackController.h"

// Main Volume, Tuner, Tap Tempo, and FX Loop controls - rig-level utilities that (unlike
// Distortion/Wah/Mod/Reverb) aren't effect-selectable: there's exactly one Main Volume, one Tuner,
// one Tap Tempo, and one FX Loop on the whole unit, so none of them need
// `EffectEditorComponent`'s "pick which model is loaded" dropdown. See docs/implementation-plan.md
// Milestone 5.
//
// Main Volume is live two-way sync, proving the pattern later per-effect parameter controls reuse:
// dragging the slider sends setMainVolume() on every change (onValueChange), and a
// device-confirmed onMainVolumeReceived() moves the slider to match - using
// juce::dontSendNotification when doing so, so that programmatic update doesn't loop back around
// and re-trigger onValueChange (which would just re-send the same value pointlessly).
//
// The slider shows the unit's own 0.0-10.0 display scale, not the raw signed wire value - confirmed
// against real hardware (2026-07-24): raw 0 = the unit's displayed "5.0" (center), raw 127 = "10.0"
// (max). displayToRaw()/rawToDisplay() convert between the two; see SevenBitCodec.h for why the
// wire value is signed at all.
//
// There's no query for tuner state in the protocol (only a set, and async notifications if the
// unit's own front panel changes it) - so the tuner status label only ever reflects a real,
// device-confirmed onTunerStateReceived callback, never an optimistic guess from clicking a
// button. Two explicit buttons (On/Off) rather than a single toggle, for the same reason: we
// can't reliably track "current state" to toggle from.
//
// Tap Tempo (CC 64) is momentary, not a persistent value - clicking the button sends one "tap"
// (value 127, "64-127 = a tap" per the official CC chart); there's nothing to read back or sync.
//
// FX Loop (Bypass=107, Send=19, Return=108, Mix=88 - see EffectDefinitions.cpp's "Fx Loop" entry)
// uses fixed, direct CCs, not a "Setting N" positional scheme - unlike every effect in
// EffectEditorComponent, so it's exposed here instead as plain Bypass + 3 knobs. No live readback,
// same limitation as the per-effect editor screens.
class RigGlobalsComponent : public juce::Component,
                            private Rack::RackController::Listener
{
public:
    explicit RigGlobalsComponent (Rack::RackController& controllerToUse);
    ~RigGlobalsComponent() override;

    void resized() override;

private:
    // Rack::RackController::Listener
    void onMainVolumeReceived (int volume) override;
    void onTunerStateReceived (bool isOn) override;

    // Linear map confirmed against real hardware: raw 0 <-> display 5.0 (center), raw 127 <->
    // display 10.0 (max); symmetric down to raw -127 <-> display 0.0 (min, not yet independently
    // confirmed since this range was unreachable before the encodeValue sign fix, but consistent
    // with the confirmed points and the wire format being a signed byte).
    static int8_t displayToRaw (double display);
    static double rawToDisplay (int raw);

    juce::Label volumeLabel { {}, "Main Volume" };
    juce::Slider volumeSlider;

    juce::Label tunerLabel { {}, "Tuner" };
    juce::TextButton tunerOnButton  { "Tuner On" };
    juce::TextButton tunerOffButton { "Tuner Off" };
    juce::Label tunerStatusLabel;

    juce::Label tapTempoLabel { {}, "Tap Tempo" };
    juce::TextButton tapTempoButton { "Tap" };

    juce::Label fxLoopLabel { {}, "FX Loop" };
    juce::ToggleButton fxLoopBypassToggle { "Bypass" };
    juce::Label fxLoopSendLabel { {}, "Send" };
    juce::Slider fxLoopSendSlider;
    juce::Label fxLoopReturnLabel { {}, "Return" };
    juce::Slider fxLoopReturnSlider;
    juce::Label fxLoopMixLabel { {}, "Mix" };
    juce::Slider fxLoopMixSlider;

    Rack::RackController& controller;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RigGlobalsComponent)
};
