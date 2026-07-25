#include "EffectEditorComponent.h"

namespace
{
    using Rack::EffectDefinitions::ParamKind;

    // One entry per editable slot. CC numbers are from the official MIDI CC chart
    // (docs/protocol-spec.md); `settingCc` is the slot's "Setting N" list in order, positionally
    // mapped to `EffectDefinition::params[1..]` (params[0] is always Bypass, handled separately via
    // `bypassCc`). Where a slot has fewer settingCc entries than a given effect has params (e.g.
    // Wah), the extra params are simply not rendered - not guessed.
    struct SlotConfig
    {
        juce::String name;
        uint8_t bypassCc = 0;
        std::vector<uint8_t> settingCc;
        std::vector<int> effectIds; // EffectDefinitions effect IDs to offer in the dropdown
        int defaultEffectId = 0;
        juce::String note;
    };

    const std::vector<SlotConfig>& slotConfigs()
    {
        static const std::vector<SlotConfig> configs = {
            {
                "Distortion",
                25, // "Distortion On/Off"
                { 27, 78, 79, 80, 81, 82, 83 }, // "Distortion Setting 1-7"
                { 29, 30, 31, 87, 91 },
                31, // Green JRC Disto - the model loaded during the 2026-07-24 hardware confirmation
                "No live readback - MIDI CC has no query mechanism, so these controls only reflect "
                "what YOU set here, not the unit's actual current values. Pick the model that "
                "matches what's really loaded in the unit's Distortion slot before adjusting "
                "anything. Hardware-confirmed 2026-07-24 (Bypass + all 3 knobs).",
            },
            {
                "Wah",
                43, // "Wah On/Off"
                { 4 }, // "Wah Pedal" - the chart has no "Setting N" scheme for Wah, just this one CC
                { 36, 55 },
                36, // Sine Wah
                "Only the Position knob (\"Wah Pedal\", CC 4) has a confirmed CC mapping - the "
                "second knob (VxCr) has no known CC in the official chart and is intentionally "
                "omitted rather than guessed. Not yet hardware-tested. No live readback.",
            },
            {
                "Mod",
                50, // "Modulation On/Off"
                // "Modulation Setting 1-7", plus 2 extra CCs (Lo Cut/Width) Multi Chorus alone uses
                // beyond the officially-named Setting-N list - see EffectDefinitions.cpp.
                { 61, 52, 53, 54, 57, 51, 56, 89, 90 },
                // Order matches the unit's own front-panel effect list exactly (confirmed
                // 2026-07-24): C1 Chor/Vib, Multi Chorus, Flanger, Vibe Phaser, Orange Phaser,
                // Roto Speaker.
                { 11, 39, 40, 88, 89, 90, 69, 70, 35, 46, 34, 71, 75, 76, 77 },
                11, // C1 Chor/Vib
                "All 6 Mod-slot effects the unit itself offers are listed here, in the unit's own "
                "on-screen order. CC data for Chorus/Vibrato, Vibe Phaser, Flanger, Multi Chorus, "
                "and Roto Speaker is sourced directly from the official Eleven Rack User Guide "
                "(Chapter 9); Orange Phaser's Rate knob is hardware-confirmed (see the Distortion "
                "slot for the positional mapping), but its Sync was recently corrected from a "
                "toggle to a tempo-sync list, same as the other Sync controls here - not yet "
                "re-tested. Roto Speaker's Type option list/order is hardware-confirmed, but the "
                "specific CC value per option is only a range midpoint guess, not independently "
                "verified (see EffectDefinitions.cpp). Confirmed 2026-07-24: Chorus/Vibrato's "
                "knobs, Sync, Bypass, and Mode toggle all work correctly. The rest of this slot is "
                "not yet hardware-tested. No live readback.",
            },
            {
                "Reverb",
                36, // "Reverb On/Off"
                { 18, 38, 40, 39, 76, 41 }, // "Reverb Setting 1-6"
                { 37, 47, 51, 52, 53 }, // Blackpanel Spring Reverb (37,47), Eleven SR (51,52,53)
                37, // Blackpanel Spring Reverb
                "Real parameters sourced from the official Eleven Rack User Guide (Chapter 9) - "
                "Mix/Decay/Tone for Blackpanel Spring Reverb, plus Pre-Delay and a 25-option Type "
                "list (CC 76) for Eleven SR. Type's option list/order is hardware-confirmed, but "
                "the specific CC value per option is only a range midpoint guess, not "
                "independently verified (see EffectDefinitions.cpp). Confirmed 2026-07-24: the "
                "unit only has these 2 real Reverb models (a dropdown dedup bug briefly made it "
                "look like 5). No live readback.",
            },
        };
        return configs;
    }

    const SlotConfig* selectedSlot (const juce::ComboBox& slotSelector)
    {
        auto index = slotSelector.getSelectedId() - 1;
        const auto& slots = slotConfigs();
        if (index < 0 || index >= (int) slots.size())
            return nullptr;
        return &slots[(size_t) index];
    }
}

EffectEditorComponent::EffectEditorComponent (Rack::RackController& controllerToUse)
    : controller (controllerToUse)
{
    addAndMakeVisible (slotChooserLabel);
    addAndMakeVisible (slotSelector);
    addAndMakeVisible (effectChooserLabel);
    addAndMakeVisible (effectSelector);
    addAndMakeVisible (noteLabel);

    const auto& slots = slotConfigs();
    for (int i = 0; i < (int) slots.size(); ++i)
        slotSelector.addItem (slots[(size_t) i].name, i + 1);

    slotSelector.onChange = [this] { rebuildEffectList(); };
    effectSelector.onChange = [this] { rebuildForSelectedEffect(); };

    noteLabel.setJustificationType (juce::Justification::topLeft);

    slotSelector.setSelectedId (1, juce::dontSendNotification); // defaults to Distortion
    rebuildEffectList();
}

EffectEditorComponent::~EffectEditorComponent() = default;

void EffectEditorComponent::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto topRow = area.removeFromTop (30);
    slotChooserLabel.setBounds (topRow.removeFromLeft (40).reduced (2));
    slotSelector.setBounds (topRow.removeFromLeft (150).reduced (2));
    topRow.removeFromLeft (10);
    effectChooserLabel.setBounds (topRow.removeFromLeft (50).reduced (2));
    effectSelector.setBounds (topRow.reduced (2));

    area.removeFromTop (6);
    noteLabel.setBounds (area.removeFromTop (90));

    area.removeFromTop (12);

    if (bypassToggle != nullptr)
    {
        auto row = area.removeFromTop (30);
        bypassToggle->setBounds (row.reduced (2));
        area.removeFromTop (6);
    }

    for (auto& pc : paramControls)
    {
        auto row = area.removeFromTop (30);
        pc.label->setBounds (row.removeFromLeft (150).reduced (2));

        if (pc.slider != nullptr)
            pc.slider->setBounds (row.reduced (2));
        else if (pc.toggle != nullptr)
            pc.toggle->setBounds (row.reduced (2));
        else if (pc.combo != nullptr)
            pc.combo->setBounds (row.reduced (2));

        area.removeFromTop (6);
    }
}

void EffectEditorComponent::rebuildEffectList()
{
    auto* slot = selectedSlot (slotSelector);
    if (slot == nullptr)
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
    for (int id : slot->effectIds)
    {
        auto def = Rack::EffectDefinitions::lookup (id);
        if (! def || seenNames.contains (def->name))
            continue;

        seenNames.add (def->name);
        effectSelector.addItem (def->name, id);
    }

    noteLabel.setText (slot->note, juce::dontSendNotification);

    effectSelector.setSelectedId (slot->defaultEffectId, juce::dontSendNotification);
    rebuildForSelectedEffect();
}

void EffectEditorComponent::rebuildForSelectedEffect()
{
    bypassToggle.reset();
    paramControls.clear();

    auto* slot = selectedSlot (slotSelector);
    auto effectId = effectSelector.getSelectedId();
    auto def = slot != nullptr ? Rack::EffectDefinitions::lookup (effectId) : std::nullopt;
    if (slot == nullptr || ! def)
    {
        resized();
        return;
    }

    auto bypassCc = slot->bypassCc;
    bypassToggle = std::make_unique<juce::ToggleButton> ("Bypass");
    addAndMakeVisible (*bypassToggle);
    bypassToggle->onClick = [this, bypassCc]
    {
        bool bypassed = bypassToggle->getToggleState();
        // 0-63 = Off, 64-127 = On, per the official CC chart.
        controller.sendMidiCc (bypassCc, bypassed ? 0 : 127);
    };

    size_t settingIndex = 0;
    // params[0] is always Bypass (handled above) - start from params[1] for the "Setting N" params.
    for (size_t i = 1; i < def->params.size() && settingIndex < slot->settingCc.size(); ++i)
    {
        const auto& param = def->params[i];
        auto cc = slot->settingCc[settingIndex];

        ParamControl pc;
        pc.kind = param.kind;
        pc.ccNumber = cc;
        pc.label = std::make_unique<juce::Label> (juce::String(), param.label);
        addAndMakeVisible (*pc.label);

        if (param.kind == ParamKind::knob)
        {
            pc.slider = std::make_unique<juce::Slider>();
            addAndMakeVisible (*pc.slider);

            pc.slider->setRange (param.minValue, param.maxValue, param.step);
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
            addAndMakeVisible (*pc.toggle);

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
            addAndMakeVisible (*pc.combo);

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
