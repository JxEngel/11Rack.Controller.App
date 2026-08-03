#include "SlotParamsPanel.h"

using Rack::EffectDefinitions::ParamKind;

SlotParamsPanel::SlotParamsPanel (Rack::RackController& controllerToUse)
    : controller (controllerToUse)
{
    addAndMakeVisible (effectChooserLabel);
    addAndMakeVisible (effectSelector);
    addAndMakeVisible (noteLabel);
    addAndMakeVisible (paramsViewport);
    paramsViewport.setViewedComponent (&paramsContent, false); // false: we own paramsContent

    effectSelector.onChange = [this] { rebuildForSelectedEffect(); };
    noteLabel.setJustificationType (juce::Justification::topLeft);
}

void SlotParamsPanel::setKnobStyle (KnobStyle style)
{
    if (style == currentKnobStyle)
        return;

    currentKnobStyle = style;
    rebuildPreservingCurrentValues();
}

void SlotParamsPanel::setToggleStyle (ToggleStyle style)
{
    if (style == currentToggleStyle)
        return;

    currentToggleStyle = style;
    rebuildPreservingCurrentValues();
}

SlotParamsPanel::~SlotParamsPanel() = default;

void SlotParamsPanel::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto topRow = area.removeFromTop (30);
    effectChooserLabel.setBounds (topRow.removeFromLeft (50).reduced (2));
    effectSelector.setBounds (topRow.reduced (2));

    area.removeFromTop (6);
    noteLabel.setBounds (area.removeFromTop (90));

    area.removeFromTop (12);

    // paramsViewport gets whatever's left; paramsContent is sized to fit its actual row count
    // (bypass + one row per param) so it scrolls instead of clipping/overlapping when an effect
    // has more rows than the visible area holds (e.g. Para EQ's 14 real params).
    paramsViewport.setBounds (area);

    // Rotary sliders (the gold knob look, see goldKnobLookAndFeel's doc comment) and rocker
    // switches (see rockerSwitchLookAndFeel's doc comment) both need more room than the flat
    // linear sliders/plain comboboxes every other row uses - 90/46px were picked by eye to fit a
    // rotary dial or a legible switch (with its ON/OFF labels) comfortably, not derived from
    // anything.
    constexpr int spacing = 6;
    constexpr int flatRowHeight = 30;
    constexpr int rotaryRowHeight = 90;
    constexpr int toggleRowHeight = 46;
    auto rowHeightFor = [flatRowHeight, rotaryRowHeight, toggleRowHeight] (const ParamControl& pc)
    {
        if (pc.slider != nullptr && pc.slider->isRotary())
            return rotaryRowHeight;
        if (pc.toggle != nullptr)
            return toggleRowHeight;
        return flatRowHeight;
    };

    int contentHeight = bypassToggle != nullptr ? (toggleRowHeight + spacing) : 0;
    for (auto& pc : paramControls)
        contentHeight += rowHeightFor (pc) + spacing;

    int contentWidth = paramsViewport.getWidth() - paramsViewport.getScrollBarThickness();
    paramsContent.setSize (juce::jmax (contentWidth, 0), contentHeight);

    auto contentArea = paramsContent.getLocalBounds();

    if (bypassToggle != nullptr)
    {
        auto row = contentArea.removeFromTop (toggleRowHeight);
        bypassToggle->setBounds (row.reduced (2));
        contentArea.removeFromTop (spacing);
    }

    for (auto& pc : paramControls)
    {
        auto row = contentArea.removeFromTop (rowHeightFor (pc));
        pc.label->setBounds (row.removeFromLeft (150).reduced (2));

        if (pc.slider != nullptr)
            pc.slider->setBounds (row.reduced (2));
        else if (pc.toggle != nullptr)
            pc.toggle->setBounds (row.reduced (2));
        else if (pc.combo != nullptr)
            pc.combo->setBounds (row.reduced (2));

        contentArea.removeFromTop (spacing);
    }
}

void SlotParamsPanel::setSlot (const SlotConfig& slot, int preferredEffectId, std::optional<bool> knownBypass,
                                const std::map<juce::String, bool>& knownToggleStates,
                                const std::map<juce::String, int>& knownKnobValues)
{
    currentSlot = &slot;
    rebuildEffectList();

    // `preferredEffectId` may be a "sibling" ID not itself listed as an effectSelector item (see
    // the dedup note in rebuildEffectList()) - EffectDefinitions::lookup() resolves any sibling ID
    // to the same shared definition, so look up its name and select whichever item shares it.
    if (preferredEffectId >= 0)
    {
        auto preferredDef = Rack::EffectDefinitions::lookup (preferredEffectId);
        if (preferredDef)
        {
            for (int i = 0; i < effectSelector.getNumItems(); ++i)
            {
                auto itemId = effectSelector.getItemId (i);
                auto itemDef = Rack::EffectDefinitions::lookup (itemId);
                if (itemDef && itemDef->name == preferredDef->name)
                {
                    effectSelector.setSelectedId (itemId, juce::dontSendNotification);
                    break;
                }
            }
        }
    }

    rebuildForSelectedEffect (knownBypass, knownToggleStates, knownKnobValues);
}

void SlotParamsPanel::clear()
{
    currentSlot = nullptr;
    effectSelector.clear (juce::dontSendNotification);
    noteLabel.setText ({}, juce::dontSendNotification);
    bypassToggle.reset();
    paramControls.clear();
    resized();
}

void SlotParamsPanel::rebuildEffectList()
{
    if (currentSlot == nullptr)
        return;

    effectSelector.clear (juce::dontSendNotification);

    // `slot->effectIds` can list several ElevenHack effectIds that all share one definition name
    // (e.g. "sibling" IDs like Volume Pedal's 38/72 - see EffectDefinitions.cpp) - these are NOT
    // distinct selectable models, just multiple underlying IDs for the same real effect (most
    // likely one per rack slot it can be placed into). Listing every raw ID produced a dropdown
    // with visibly duplicate-looking entries (confirmed on real hardware 2026-07-24: Reverb showed
    // 5 entries - 2x "Blackpanel Spring Reverb", 3x "Eleven SR" - when the unit only has 2 actual
    // Reverb options). Deduplicate by name, keeping the first effectId seen as that entry's ID.
    juce::StringArray seenNames;
    for (int id : currentSlot->effectIds)
    {
        auto def = Rack::EffectDefinitions::lookup (id);
        if (! def || seenNames.contains (def->name))
            continue;

        seenNames.add (def->name);
        effectSelector.addItem (def->name, id);
    }

    noteLabel.setText (currentSlot->note, juce::dontSendNotification);
    effectSelector.setSelectedId (currentSlot->defaultEffectId, juce::dontSendNotification);
}

void SlotParamsPanel::rebuildPreservingCurrentValues()
{
    std::optional<bool> bypass;
    if (bypassToggle != nullptr)
        bypass = bypassToggle->getToggleState();

    std::map<juce::String, bool> toggleStates;
    std::map<juce::String, int> knobValues;
    for (auto& pc : paramControls)
    {
        if (pc.kind == ParamKind::toggle && pc.toggle != nullptr)
            toggleStates[pc.paramKey] = pc.toggle->getToggleState();
        else if (pc.kind == ParamKind::knob && pc.slider != nullptr)
            knobValues[pc.paramKey] = (int) pc.slider->getValue();
    }

    rebuildForSelectedEffect (bypass, toggleStates, knobValues);
}

void SlotParamsPanel::applyKnobStyle (juce::Slider& slider)
{
    // Every knob-kind control always renders as an actual knob - see KnobStyle.h for why there's
    // no "flat"/plain-slider fallback. A switch (not if/else) so adding a new enumerator without a
    // matching case here warns at compile time instead of silently doing nothing.
    switch (currentKnobStyle)
    {
        case KnobStyle::goldMetallic:
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
            slider.setLookAndFeel (&goldKnobLookAndFeel);
            break;
    }
}

void SlotParamsPanel::applyToggleStyle (juce::ToggleButton& toggle)
{
    // Every toggle-kind control always renders as an actual switch - see ToggleStyle.h for why
    // there's no plain-checkbox fallback. A switch (not if/else) so adding a new enumerator
    // without a matching case here warns at compile time instead of silently doing nothing.
    switch (currentToggleStyle)
    {
        case ToggleStyle::rockerSwitch:
            toggle.setLookAndFeel (&rockerSwitchLookAndFeel);
            break;
    }
}

void SlotParamsPanel::rebuildForSelectedEffect (std::optional<bool> knownBypass,
                                                 const std::map<juce::String, bool>& knownToggleStates,
                                                 const std::map<juce::String, int>& knownKnobValues)
{
    bypassToggle.reset();
    paramControls.clear();

    auto effectId = effectSelector.getSelectedId();
    auto def = currentSlot != nullptr ? Rack::EffectDefinitions::lookup (effectId) : std::nullopt;
    if (currentSlot == nullptr || ! def)
    {
        resized();
        return;
    }

    currentBypassCc = currentSlot->bypassCc;
    bypassToggle = std::make_unique<juce::ToggleButton> ("Bypass");
    paramsContent.addAndMakeVisible (*bypassToggle);
    applyToggleStyle (*bypassToggle);
    if (knownBypass)
        bypassToggle->setToggleState (*knownBypass, juce::dontSendNotification);
    bypassToggle->onClick = [this]
    {
        bool bypassed = bypassToggle->getToggleState();
        // 0-63 = Off, 64-127 = On, per the official CC chart.
        controller.sendMidiCc (currentBypassCc, bypassed ? 0 : 127);
    };

    size_t settingIndex = 0;
    // params[0] is always Bypass (handled above) - start from params[1]. Params within
    // currentSlot->settingCc's range get the usual live-CC treatment; params BEYOND it have no
    // known CC at all, so they only get shown if there's an actual confirmed decoded value for them
    // (knownToggleStates/knownKnobValues - see SlotConfig.h's confirmedRawTagForKey()/
    // bestEffortRawTagForKey()) - local-only controls, not wired to send anything, matching
    // SignalChainComponent's Input block treatment. No known value AND no live CC means nothing
    // honest to show, so that param is simply skipped, same as always.
    for (size_t i = 1; i < def->params.size(); ++i)
    {
        const auto& param = def->params[i];
        bool hasLiveCc = settingIndex < currentSlot->settingCc.size();
        uint8_t cc = hasLiveCc ? currentSlot->settingCc[settingIndex] : 0;

        auto knownToggleIt = knownToggleStates.find (juce::String (param.key));
        auto knownKnobIt = knownKnobValues.find (juce::String (param.key));
        bool hasKnownValue = knownToggleIt != knownToggleStates.end() || knownKnobIt != knownKnobValues.end();

        if (! hasLiveCc && ! hasKnownValue)
            continue;

        ParamControl pc;
        pc.kind = param.kind;
        pc.paramKey = juce::String (param.key);
        pc.ccNumber = cc;
        pc.label = std::make_unique<juce::Label> (juce::String(),
                                                    hasLiveCc ? param.label : (param.label + " (not synced)"));
        paramsContent.addAndMakeVisible (*pc.label);

        if (param.kind == ParamKind::knob)
        {
            pc.slider = std::make_unique<juce::Slider>();
            paramsContent.addAndMakeVisible (*pc.slider);

            pc.slider->setRange (param.minValue, param.maxValue, param.step);

            // Only seeded when SlotConfig.h's confirmedRawTagForKey() has an actual confirmed raw
            // tag for this param (and, for knobs, knobRawToCcValue()'s Q31 conversion applied to
            // it) - every other knob just keeps the neutral mid-range default, no guessing (see the
            // class doc comment above).
            if (knownKnobIt != knownKnobValues.end())
                pc.slider->setValue (knownKnobIt->second, juce::dontSendNotification);
            else
                pc.slider->setValue ((param.minValue + param.maxValue) / 2.0, juce::dontSendNotification);

            applyKnobStyle (*pc.slider);

            // Local-only (no live CC): the slider just holds whatever you drag it to - no
            // onValueChange handler, so nothing gets sent anywhere.
            if (hasLiveCc)
            {
                auto* sliderPtr = pc.slider.get();
                pc.slider->onValueChange = [this, cc, sliderPtr]
                {
                    controller.sendMidiCc (cc, static_cast<uint8_t> (static_cast<int> (sliderPtr->getValue())));
                };
            }
        }
        else if (param.kind == ParamKind::toggle)
        {
            pc.toggle = std::make_unique<juce::ToggleButton>();
            paramsContent.addAndMakeVisible (*pc.toggle);
            applyToggleStyle (*pc.toggle);

            // Only seeded when SlotConfig.h's confirmedRawTagForKey() has an actual confirmed raw
            // tag for this param - every other toggle just keeps its default (unchecked) state, no
            // guessing (see the class doc comment above).
            if (knownToggleIt != knownToggleStates.end())
                pc.toggle->setToggleState (knownToggleIt->second, juce::dontSendNotification);

            // Local-only (no live CC): just toggles visually, nothing sent - see the knob branch.
            if (hasLiveCc)
            {
                auto* togglePtr = pc.toggle.get();
                pc.toggle->onClick = [this, cc, togglePtr]
                {
                    // 0-63 = Off, 64-127 = On - the same convention as the official on/off CCs.
                    controller.sendMidiCc (cc, togglePtr->getToggleState() ? 127 : 0);
                };
            }
        }
        else // selector
        {
            pc.combo = std::make_unique<juce::ComboBox>();
            paramsContent.addAndMakeVisible (*pc.combo);

            // ComboBox item IDs must be > 0, but option values (e.g. Sync's "None" = 0) can be 0,
            // so item IDs are the option's value shifted up by 1 and shifted back down on read.
            for (const auto& opt : param.options)
                pc.combo->addItem (opt.name, opt.value + 1);

            if (! param.options.empty())
                pc.combo->setSelectedId (param.options.front().value + 1, juce::dontSendNotification);

            if (hasLiveCc)
            {
                auto* comboPtr = pc.combo.get();
                pc.combo->onChange = [this, cc, comboPtr]
                {
                    auto rawValue = comboPtr->getSelectedId() - 1;
                    controller.sendMidiCc (cc, static_cast<uint8_t> (rawValue));
                };
            }
        }

        paramControls.push_back (std::move (pc));
        if (hasLiveCc)
            ++settingIndex;
    }

    resized();
}
