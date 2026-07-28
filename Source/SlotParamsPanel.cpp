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

    constexpr int rowHeight = 36; // 30 row + 6 spacing, matching the row layout below
    int rowCount = (bypassToggle != nullptr ? 1 : 0) + (int) paramControls.size();
    int contentWidth = paramsViewport.getWidth() - paramsViewport.getScrollBarThickness();
    paramsContent.setSize (juce::jmax (contentWidth, 0), rowCount * rowHeight);

    auto contentArea = paramsContent.getLocalBounds();

    if (bypassToggle != nullptr)
    {
        auto row = contentArea.removeFromTop (30);
        bypassToggle->setBounds (row.reduced (2));
        contentArea.removeFromTop (6);
    }

    for (auto& pc : paramControls)
    {
        auto row = contentArea.removeFromTop (30);
        pc.label->setBounds (row.removeFromLeft (150).reduced (2));

        if (pc.slider != nullptr)
            pc.slider->setBounds (row.reduced (2));
        else if (pc.toggle != nullptr)
            pc.toggle->setBounds (row.reduced (2));
        else if (pc.combo != nullptr)
            pc.combo->setBounds (row.reduced (2));

        contentArea.removeFromTop (6);
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
    if (knownBypass)
        bypassToggle->setToggleState (*knownBypass, juce::dontSendNotification);
    bypassToggle->onClick = [this]
    {
        bool bypassed = bypassToggle->getToggleState();
        // 0-63 = Off, 64-127 = On, per the official CC chart.
        controller.sendMidiCc (currentBypassCc, bypassed ? 0 : 127);
    };

    size_t settingIndex = 0;
    // params[0] is always Bypass (handled above) - start from params[1] for the "Setting N" params.
    for (size_t i = 1; i < def->params.size() && settingIndex < currentSlot->settingCc.size(); ++i)
    {
        const auto& param = def->params[i];
        auto cc = currentSlot->settingCc[settingIndex];

        ParamControl pc;
        pc.kind = param.kind;
        pc.ccNumber = cc;
        pc.label = std::make_unique<juce::Label> (juce::String(), param.label);
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
            auto knownIt = knownKnobValues.find (juce::String (param.key));
            if (knownIt != knownKnobValues.end())
                pc.slider->setValue (knownIt->second, juce::dontSendNotification);
            else
                pc.slider->setValue ((param.minValue + param.maxValue) / 2.0, juce::dontSendNotification);

            pc.slider->setSliderStyle (juce::Slider::LinearHorizontal);
            pc.slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 24);

            auto* sliderPtr = pc.slider.get();
            pc.slider->onValueChange = [this, cc, sliderPtr]
            {
                controller.sendMidiCc (cc, static_cast<uint8_t> (static_cast<int> (sliderPtr->getValue())));
            };
        }
        else if (param.kind == ParamKind::toggle)
        {
            pc.toggle = std::make_unique<juce::ToggleButton>();
            paramsContent.addAndMakeVisible (*pc.toggle);

            // Only seeded when SlotConfig.h's confirmedRawTagForKey() has an actual confirmed raw
            // tag for this param - every other toggle just keeps its default (unchecked) state, no
            // guessing (see the class doc comment above).
            auto knownIt = knownToggleStates.find (juce::String (param.key));
            if (knownIt != knownToggleStates.end())
                pc.toggle->setToggleState (knownIt->second, juce::dontSendNotification);

            auto* togglePtr = pc.toggle.get();
            pc.toggle->onClick = [this, cc, togglePtr]
            {
                // 0-63 = Off, 64-127 = On - the same convention as the official on/off CCs.
                controller.sendMidiCc (cc, togglePtr->getToggleState() ? 127 : 0);
            };
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

            auto* comboPtr = pc.combo.get();
            pc.combo->onChange = [this, cc, comboPtr]
            {
                auto rawValue = comboPtr->getSelectedId() - 1;
                controller.sendMidiCc (cc, static_cast<uint8_t> (rawValue));
            };
        }

        paramControls.push_back (std::move (pc));
        ++settingIndex;
    }

    resized();
}
