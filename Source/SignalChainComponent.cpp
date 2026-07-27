#include "SignalChainComponent.h"
#include "SlotConfig.h"

using Rack::RackController;
using Rack::EffectDefinitions::EffectClass;

namespace
{
    constexpr int kBlockWidth = 92;
    constexpr int kBlockHeight = 48;
    constexpr int kArrowWidth = 14;

    // Best-guess order shown before any real Bulk Rig decode - NOT confirmed against any real rig
    // (see SignalChainComponent::buildDefaultChain()). Once a decode arrives, the real per-rig order
    // from BulkRigParser::ParsedRig::slots replaces this entirely - see updateBlockDataFromRig().
    struct ChainBlockTemplate
    {
        const char* id;
        const char* label;
    };

    constexpr ChainBlockTemplate kDefaultChainTemplate[] = {
        { "wah", "Wah" },
        { "vol", "Volume" },
        { "disto", "Distortion" },
        { "amp", "Amp" },
        { "cab", "Cab" },
        { "mod", "Mod" },
        { "delay", "Delay" },
        { "reverb", "Reverb" },
        { "fxloop", "FX Loop" },
        { "fx1", "FX1" },
        { "fx2", "FX2" },
    };

    // The block id(s)/label(s) a given BulkRigParser::EffectSlot::category expands to, in chain
    // order. ampCab is the one case that expands to TWO blocks (Amp then Cab, always adjacent - they
    // share one lettered slot because they're one combined effect in EffectDefinitions, effectId 12)
    // - only the first ("amp") gets the slot's real decoded data; "cab" has none to show. Every other
    // category maps to exactly one block. Returns an empty vector for a category that shouldn't
    // appear among the 10 Bulk Rig slots (input/rigParams/etc.) - skipped rather than guessed.
    std::vector<std::pair<juce::String, juce::String>> blockTemplatesForCategory (int category)
    {
        switch (static_cast<EffectClass> (category))
        {
            case EffectClass::ampCab: return { { "amp", "Amp" }, { "cab", "Cab" } };
            case EffectClass::fxLoop: return { { "fxloop", "FX Loop" } };
            case EffectClass::vol:    return { { "vol", "Volume" } };
            case EffectClass::wah:    return { { "wah", "Wah" } };
            case EffectClass::mod:    return { { "mod", "Mod" } };
            case EffectClass::reverb: return { { "reverb", "Reverb" } };
            case EffectClass::delay:  return { { "delay", "Delay" } };
            case EffectClass::disto:  return { { "disto", "Distortion" } };
            case EffectClass::fx1:    return { { "fx1", "FX1" } };
            case EffectClass::fx2:    return { { "fx2", "FX2" } };
            default: return {};
        }
    }

    // Which SlotConfig (see SlotConfig.cpp) a chain block should open in the editor panel - only
    // the 7 slots EffectEditorComponent already covers. Volume/Amp/Cab/FX Loop have no SlotConfig
    // yet (same real gap EffectEditorComponent has today) - clicking those shows the fallback label.
    juce::String slotConfigNameForBlockId (const juce::String& id)
    {
        if (id == "disto") return "Distortion";
        if (id == "wah") return "Wah";
        if (id == "mod") return "Mod";
        if (id == "delay") return "Delay";
        if (id == "reverb") return "Reverb";
        if (id == "fx1") return "FX1";
        if (id == "fx2") return "FX2";
        return {};
    }
}

void SignalChainComponent::Block::setInfo (const ChainBlock& info, bool isSelected)
{
    label = info.label;
    sub = info.subLabel;
    fixed = info.fixed;
    isIo = info.isIo;
    selected = isSelected;
    repaint();
}

void SignalChainComponent::Block::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    juce::Colour borderColour = fixed ? juce::Colours::grey : juce::Colour (0xff7f77dd);
    if (selected)
    {
        g.setColour (juce::Colours::dodgerblue.withAlpha (0.15f));
        g.fillRoundedRectangle (bounds, 6.0f);
        borderColour = juce::Colours::dodgerblue;
    }
    g.setColour (borderColour);
    g.drawRoundedRectangle (bounds, 6.0f, selected ? 1.5f : 0.75f);

    auto textArea = getLocalBounds().reduced (5, 3);
    g.setColour (selected ? juce::Colours::dodgerblue : juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
    g.drawFittedText (label, textArea.removeFromTop (18), juce::Justification::centredLeft, 1);

    if (sub.isNotEmpty())
    {
        g.setColour (juce::Colours::lightgrey);
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.drawFittedText (sub, textArea, juce::Justification::centredLeft, 1);
    }
}

void SignalChainComponent::Block::mouseUp (const juce::MouseEvent&)
{
    if (! isIo && onClick)
        onClick();
}

SignalChainComponent::SignalChainComponent (Rack::RackController& controllerToUse)
    : paramsPanel (controllerToUse), controller (controllerToUse)
{
    controller.addListener (this);

    addAndMakeVisible (presetLabel);
    addAndMakeVisible (presetSelector);
    addAndMakeVisible (refreshRigListButton);
    addAndMakeVisible (rigStatusLabel);
    addAndMakeVisible (chainLabel);
    addAndMakeVisible (chainViewport);
    addAndMakeVisible (paramsPanel);
    addAndMakeVisible (noSlotLabel);

    chainViewport.setViewedComponent (&chainContent, false);
    chainViewport.setScrollBarsShown (false, true);

    rigEntries.resize ((size_t) (RackController::kNumBanks * RackController::kRigsPerBank));
    rigStatusLabel.setText ("Click Refresh Rig List to fetch real rig names.", juce::dontSendNotification);

    refreshRigListButton.onClick = [this] { refreshRigList(); };
    presetSelector.onChange = [this] { presetSelected(); };

    noSlotLabel.setText ("No editable parameters mapped for this block yet.", juce::dontSendNotification);
    noSlotLabel.setJustificationType (juce::Justification::centred);

    chain = buildDefaultChain();
    rebuildChainUi();

    // Default selection matches the mockup's own default (Distortion) rather than an IO block.
    for (int i = 0; i < (int) chain.size(); ++i)
        if (chain[(size_t) i].id == "disto")
        {
            selectBlock (i);
            break;
        }
}

SignalChainComponent::~SignalChainComponent()
{
    controller.removeListener (this);
}

std::vector<SignalChainComponent::ChainBlock> SignalChainComponent::buildDefaultChain()
{
    std::vector<ChainBlock> result;

    ChainBlock input;
    input.id = "input";
    input.label = "Input";
    input.fixed = true;
    input.isIo = true;
    result.push_back (input);

    for (const auto& t : kDefaultChainTemplate)
    {
        ChainBlock block;
        block.id = t.id;
        block.label = t.label;
        result.push_back (block);
    }

    ChainBlock output;
    output.id = "output";
    output.label = "Output";
    output.fixed = true;
    output.isIo = true;
    result.push_back (output);

    return result;
}

void SignalChainComponent::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto presetRow = area.removeFromTop (30);
    presetLabel.setBounds (presetRow.removeFromLeft (50).reduced (2));
    presetSelector.setBounds (presetRow.removeFromLeft (260).reduced (2));
    presetRow.removeFromLeft (8);
    refreshRigListButton.setBounds (presetRow.removeFromLeft (140).reduced (2));
    rigStatusLabel.setBounds (presetRow.reduced (2));

    area.removeFromTop (10);
    chainLabel.setBounds (area.removeFromTop (18));
    area.removeFromTop (4);
    chainViewport.setBounds (area.removeFromTop (kBlockHeight + 12));

    area.removeFromTop (12);
    paramsPanel.setBounds (area);
    noSlotLabel.setBounds (area);
}

void SignalChainComponent::rebuildChainUi()
{
    blockComponents.clear();
    arrowLabels.clear();
    chainContent.removeAllChildren();

    int x = 0;
    for (int i = 0; i < (int) chain.size(); ++i)
    {
        auto block = std::make_unique<Block>();
        block->setInfo (chain[(size_t) i], i == selectedIndex);
        block->setBounds (x, 4, kBlockWidth, kBlockHeight);
        block->onClick = [this, i] { selectBlock (i); };
        chainContent.addAndMakeVisible (*block);
        blockComponents.push_back (std::move (block));
        x += kBlockWidth;

        if (i < (int) chain.size() - 1)
        {
            auto arrow = std::make_unique<juce::Label> (juce::String(), juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92")));
            arrow->setJustificationType (juce::Justification::centred);
            arrow->setColour (juce::Label::textColourId, juce::Colours::grey);
            arrow->setBounds (x, 4, kArrowWidth, kBlockHeight);
            chainContent.addAndMakeVisible (*arrow);
            arrowLabels.push_back (std::move (arrow));
            x += kArrowWidth;
        }
    }

    chainContent.setSize (x, kBlockHeight + 8);
}

void SignalChainComponent::selectBlock (int index)
{
    if (index < 0 || index >= (int) chain.size() || chain[(size_t) index].isIo)
        return;

    selectedIndex = index;
    rebuildChainUi();

    const auto& block = chain[(size_t) selectedIndex];
    auto slotName = slotConfigNameForBlockId (block.id);
    const auto* slot = slotName.isNotEmpty() ? findSlotByName (slotName) : nullptr;

    if (slot != nullptr)
    {
        paramsPanel.setSlot (*slot, block.decodedEffectId, block.decodedBypass);
        paramsPanel.setVisible (true);
        noSlotLabel.setVisible (false);
    }
    else
    {
        paramsPanel.clear();
        paramsPanel.setVisible (false);
        noSlotLabel.setVisible (true);
    }
}

void SignalChainComponent::updateBlockDataFromRig (const Rack::BulkRigParser::ParsedRig& rig)
{
    // BulkRigParser::ParsedRig::slots is already in real signal-chain position order (letters
    // C..L = chain position, confirmed against a real rig 2026-07-27 - see the class doc comment) -
    // so the chain's block order is rebuilt directly from it, replacing the previous
    // default/previous-rig order entirely, rather than patching sub-labels in place.
    juce::String previouslySelectedId = (selectedIndex >= 0 && selectedIndex < (int) chain.size())
                                             ? chain[(size_t) selectedIndex].id
                                             : juce::String();

    std::vector<ChainBlock> newChain;

    ChainBlock input;
    input.id = "input";
    input.label = "Input";
    input.fixed = true;
    input.isIo = true;
    newChain.push_back (input);

    for (const auto& slot : rig.slots)
    {
        auto templates = blockTemplatesForCategory (slot.category);
        for (size_t t = 0; t < templates.size(); ++t)
        {
            ChainBlock block;
            block.id = templates[t].first;
            block.label = templates[t].second;

            // Only the first block a slot expands to gets that slot's real data - "cab" (the
            // second template for ampCab) has no independent data, since Amp/Cab is one combined
            // effect in EffectDefinitions.
            if (t == 0)
            {
                auto def = Rack::EffectDefinitions::lookup (slot.effectId);
                block.subLabel = def ? juce::String (def->name) : ("Unknown effectId " + juce::String (slot.effectId));
                block.decodedEffectId = slot.effectId;

                auto bypassIt = slot.params.find ("bypa");
                block.decodedBypass = bypassIt != slot.params.end() ? std::optional<bool> (bypassIt->second != 0) : std::nullopt;
            }

            newChain.push_back (block);
        }
    }

    ChainBlock output;
    output.id = "output";
    output.label = "Output";
    output.fixed = true;
    output.isIo = true;
    newChain.push_back (output);

    chain = std::move (newChain);

    // Try to keep the same block selected (by id, not index - the order may have just changed);
    // fall back to the first clickable block if it's gone, or clear the panel if none exist.
    int newIndex = -1;
    for (int i = 0; i < (int) chain.size(); ++i)
        if (chain[(size_t) i].id == previouslySelectedId)
        {
            newIndex = i;
            break;
        }
    if (newIndex < 0)
        for (int i = 0; i < (int) chain.size(); ++i)
            if (! chain[(size_t) i].isIo)
            {
                newIndex = i;
                break;
            }

    if (newIndex >= 0)
        selectBlock (newIndex);
    else
    {
        selectedIndex = -1;
        rebuildChainUi();
        paramsPanel.clear();
        paramsPanel.setVisible (false);
        noSlotLabel.setVisible (true);
    }
}

void SignalChainComponent::refreshRigList()
{
    for (auto& entry : rigEntries)
    {
        entry.known = false;
        entry.name = {};
    }
    namesReceivedCount = 0;
    fetchingRigNames = true;
    presetSelector.clear (juce::dontSendNotification);
    rigStatusLabel.setText ("Fetching rig names... 0/" + juce::String ((int) rigEntries.size()), juce::dontSendNotification);

    controller.requestAllRigNames();
}

void SignalChainComponent::presetSelected()
{
    auto id = presetSelector.getSelectedId();
    if (id < 1)
        return;

    int index = id - 1;
    int bank = index / RackController::kRigsPerBank;
    int rig = index % RackController::kRigsPerBank;

    controller.selectRig ({ (uint8_t) bank, (uint8_t) rig });

    // Known limitation: selectRig() is a write with no confirmation the unit has actually finished
    // switching rigs before this request goes out right behind it - if the real switch takes longer
    // than the round-trip to the next request, the decode below may still reflect the PREVIOUS rig.
    // Not worked around with an artificial delay, since the real timing isn't measured - a guessed
    // delay could be wrong either way. Re-select the same preset if the chain looks stale.
    controller.requestBulkRig();
}

void SignalChainComponent::updatePresetSelectorItems()
{
    presetSelector.clear (juce::dontSendNotification);
    for (int i = 0; i < (int) rigEntries.size(); ++i)
    {
        const auto& entry = rigEntries[(size_t) i];
        if (! entry.known)
            continue;

        int bank = i / RackController::kRigsPerBank;
        int rig = i % RackController::kRigsPerBank;
        auto label = "Bank " + juce::String (bank) + " " + rigLocationLabel (rig) + ": " + entry.name;
        presetSelector.addItem (label, i + 1);
    }
}

juce::String SignalChainComponent::rigLocationLabel (int rigWithinBank)
{
    int letterIndex = rigWithinBank / 4;
    int number = (rigWithinBank % 4) + 1;
    auto letter = static_cast<juce::juce_wchar> ('A' + letterIndex);
    return juce::String::charToString (letter) + juce::String (number);
}

void SignalChainComponent::onRigNameReceived (RackController::RigId rig, const std::string& name)
{
    int index = rig.bank * RackController::kRigsPerBank + rig.rig;
    if (index < 0 || index >= (int) rigEntries.size())
        return;

    rigEntries[(size_t) index].name = juce::String (name);
    rigEntries[(size_t) index].known = true;
    ++namesReceivedCount;

    if (fetchingRigNames)
        rigStatusLabel.setText ("Fetching rig names... " + juce::String (namesReceivedCount) + "/"
                                     + juce::String ((int) rigEntries.size()),
                                 juce::dontSendNotification);
}

void SignalChainComponent::onRigNameFetchComplete()
{
    fetchingRigNames = false;
    rigStatusLabel.setText ("Rig list loaded (" + juce::String (namesReceivedCount) + ").", juce::dontSendNotification);
    updatePresetSelectorItems();
}

void SignalChainComponent::onBulkRigReceived (const std::vector<uint8_t>& decodedTfxBytes)
{
    auto rig = Rack::BulkRigParser::parseDecoded (decodedTfxBytes);
    if (! rig)
    {
        rigStatusLabel.setText ("Bulk Rig reply could not be parsed.", juce::dontSendNotification);
        return;
    }

    rigStatusLabel.setText ("Showing rig \"" + juce::String (rig->rigName) + "\".", juce::dontSendNotification);
    updateBlockDataFromRig (*rig);
}
