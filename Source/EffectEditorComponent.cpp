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
                { 61, 52, 53, 54, 57, 51, 56 }, // "Modulation Setting 1-7"
                { 11, 39, 40, 34, 71 }, // Chorus/Vibrato (11,39,40), Orange Phaser (34,71)
                11, // Chorus/Vibrato
                "Only Mod-slot effects with real decoded parameters are listed here (Chorus/"
                "Vibrato, Orange Phaser) - others this slot can hold (Vibe Phaser, Multi Chorus, "
                "Flanger, Roto Speaker) have no decoded knobs in EffectDefinitions and are "
                "omitted. The positional \"Setting N\" mapping is hardware-confirmed for knobs "
                "(see the Distortion slot), but applying it here to the Mode switch and Sync "
                "selector is an untested extension of that hypothesis. Not yet hardware-tested. "
                "No live readback.",
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
    for (int id : slot->effectIds)
        if (auto def = Rack::EffectDefinitions::lookup (id))
            effectSelector.addItem (def->name, id); // effectId used directly as the item ID

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
