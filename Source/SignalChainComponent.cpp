#include "SignalChainComponent.h"
#include "SlotConfig.h"

#include <limits>

using Rack::RackController;
using Rack::EffectDefinitions::EffectClass;

namespace
{
    // Sizing matches docs/mockups/signal-chain-editor-concept.html's settled fixed-width design
    // (68x40 blocks, no scrollbar-stretching - see signal-chain-editor-concept-notes.md "Chain
    // slots use a fixed width"). Height is a few px taller than the mock's CSS used since JUCE's
    // font metrics need slightly more room than the equivalent 10px/9px web fonts did.
    constexpr int kBlockWidth = 68;
    constexpr int kBlockHeight = 44;
    constexpr int kArrowWidth = 10;
    constexpr int kGroupPadding = 3; // matches .amp-cab-group's CSS padding
    constexpr int kChainPanelPadding = 10; // matches .chain-panel's CSS padding

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

    // CallOutBox content for the rename pencil button - see
    // docs/mockups/signal-chain-editor-concept-notes.md "Renaming a preset is a deliberate,
    // separate action". `location` (e.g. "Bank 0 A1") is fixed, non-editable static text - only
    // the name itself is ever in the editable field, so the location can never look editable.
    //
    // NOTE: `onSave` currently does NOT call RackController::setRigName() - that method is
    // explicitly flagged "NOT YET HARDWARE-VALIDATED... do not wire to a UI control without a
    // deliberate, separate decision" (see RackController.h). This just reports the would-be new
    // name back to the caller, which for now only updates local UI state - see selectBlock()'s
    // caller, showRenamePopup(), in SignalChainComponent.cpp.
    class RenamePopupContent : public juce::Component
    {
    public:
        RenamePopupContent (const juce::String& location, const juce::String& currentName,
                             std::function<void (const juce::String&)> onSaveIn)
            : onSave (std::move (onSaveIn))
        {
            locationLabel.setText (location + ":", juce::dontSendNotification);
            locationLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
            locationLabel.setMinimumHorizontalScale (1.0f);
            addAndMakeVisible (locationLabel);

            nameEditor.setText (currentName, juce::dontSendNotification);
            addAndMakeVisible (nameEditor);

            addAndMakeVisible (cancelButton);
            addAndMakeVisible (saveButton);

            cancelButton.onClick = [this] { dismiss(); };
            saveButton.onClick = [this] { commitAndDismiss(); };
            nameEditor.onReturnKey = [this] { commitAndDismiss(); };
            nameEditor.onEscapeKey = [this] { dismiss(); };

            setSize (280, 74);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (8);
            auto fieldRow = area.removeFromTop (26);
            int locationWidth = juce::GlyphArrangement::getStringWidthInt (locationLabel.getFont(), locationLabel.getText()) + 6;
            locationLabel.setBounds (fieldRow.removeFromLeft (locationWidth));
            fieldRow.removeFromLeft (4);
            nameEditor.setBounds (fieldRow);

            area.removeFromTop (8);
            auto buttonRow = area.removeFromTop (26);
            saveButton.setBounds (buttonRow.removeFromRight (70));
            buttonRow.removeFromRight (6);
            cancelButton.setBounds (buttonRow.removeFromRight (70));
        }

        void visibilityChanged() override
        {
            if (isVisible())
            {
                nameEditor.grabKeyboardFocus();
                nameEditor.selectAll();
            }
        }

    private:
        void dismiss()
        {
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
        }

        void commitAndDismiss()
        {
            auto text = nameEditor.getText().trim();
            if (text.isNotEmpty() && onSave)
                onSave (text);
            dismiss();
        }

        juce::Label locationLabel;
        juce::TextEditor nameEditor;
        juce::TextButton cancelButton { "Cancel" };
        juce::TextButton saveButton { "Save" };
        std::function<void (const juce::String&)> onSave;
    };

    // CallOutBox content for "Save to Unit" - see
    // docs/mockups/signal-chain-editor-concept-notes.md "Save to Unit ... gets its own confirmation
    // popover, styled deliberately differently from the rename Save". Same not-yet-wired caveat as
    // RenamePopupContent above applies to `onConfirm` and RackController::saveRig().
    class SaveConfirmPopupContent : public juce::Component
    {
    public:
        SaveConfirmPopupContent (const juce::String& message, std::function<void()> onConfirmIn)
            : onConfirm (std::move (onConfirmIn))
        {
            messageLabel.setText (message, juce::dontSendNotification);
            messageLabel.setJustificationType (juce::Justification::topLeft);
            addAndMakeVisible (messageLabel);

            addAndMakeVisible (cancelButton);
            addAndMakeVisible (confirmButton);

            // Warning colour, distinct from the plain accent used for the rename Save button -
            // this overwrites whatever's on the unit with no undo, see the note above.
            confirmButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffc0392b));

            cancelButton.onClick = [this] { dismiss(); };
            confirmButton.onClick = [this]
            {
                if (onConfirm)
                    onConfirm();
                dismiss();
            };

            setSize (280, 120);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (10);
            messageLabel.setBounds (area.removeFromTop (70));
            area.removeFromTop (6);
            auto buttonRow = area.removeFromTop (26);
            confirmButton.setBounds (buttonRow.removeFromRight (80));
            buttonRow.removeFromRight (6);
            cancelButton.setBounds (buttonRow.removeFromRight (70));
        }

    private:
        void dismiss()
        {
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
        }

        juce::Label messageLabel;
        juce::TextButton cancelButton { "Cancel" };
        juce::TextButton confirmButton { "Overwrite" };
        std::function<void()> onConfirm;
    };
}

void SignalChainComponent::Block::setInfo (const ChainBlock& info, bool isSelected)
{
    label = info.label;
    sub = info.subLabel;
    id = info.id;
    fixed = info.fixed;
    isIo = info.isIo;
    selected = isSelected;

    // Input/Output can never move at all; Amp/Cab can't be picked up themselves either, even
    // though their position can still shift as a side effect of some OTHER block being dragged
    // past them - see ChainDropArea's doc comment in SignalChainComponent.h.
    draggable = ! isIo && id != "amp" && id != "cab";

    // Full untruncated value always available on hover, even when the visible text is
    // ellipsis-truncated to fit the block's fixed width - see the class doc comment.
    setTooltip (sub);

    repaint();
}

void SignalChainComponent::Block::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Hard/square corners, block sits visually on top of the chain panel behind it - see
    // docs/mockups/signal-chain-editor-concept-notes.md "Chain blocks"/"Chain row panel".
    g.setColour (findColour (juce::ResizableWindow::backgroundColourId));
    g.fillRect (bounds);

    juce::Colour borderColour = fixed ? juce::Colours::grey : juce::Colour (0xff7f77dd);
    if (selected)
    {
        g.setColour (juce::Colours::dodgerblue.withAlpha (0.15f));
        g.fillRect (bounds);
        borderColour = juce::Colours::dodgerblue;
    }
    g.setColour (borderColour);
    g.drawRect (bounds, selected ? 2 : 1);

    auto textArea = bounds.reduced (5, 3);
    juce::Colour textColour = selected ? juce::Colours::dodgerblue : juce::Colours::white;

    g.setColour (textColour);
    g.setFont (juce::Font (juce::FontOptions (10.0f)).boldened());
    auto titleArea = textArea.removeFromTop (13);
    g.drawText (label, titleArea, juce::Justification::centredLeft, true);

    // Lighter divider line directly under the title, then the sub-label below it - both pinned
    // top-left rather than vertically centred, matching the mock's ".block .divider"/".block .sub".
    textArea.removeFromTop (2);
    auto dividerArea = textArea.removeFromTop (1);
    g.setColour (juce::Colours::grey.withAlpha (selected ? 0.4f : 0.7f));
    g.fillRect (dividerArea);
    textArea.removeFromTop (2);

    if (sub.isNotEmpty())
    {
        g.setColour (selected ? textColour : juce::Colours::lightgrey);
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        g.drawText (sub, textArea, juce::Justification::centredLeft, true);
    }
}

void SignalChainComponent::Block::mouseDown (const juce::MouseEvent& e)
{
    // No extra bookkeeping needed here - just lets JUCE track the drag-start position so
    // MouseEvent::getDistanceFromDragStart() works in mouseDrag() below.
    juce::Component::mouseDown (e);
}

void SignalChainComponent::Block::mouseDrag (const juce::MouseEvent& e)
{
    constexpr int dragThreshold = 6;
    if (! draggable || e.getDistanceFromDragStart() < dragThreshold)
        return;

    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        if (! container->isDragAndDropActive())
            container->startDragging (id, this);
}

void SignalChainComponent::Block::mouseUp (const juce::MouseEvent& e)
{
    // A completed drag shouldn't also re-select the source block as a click.
    if (! isIo && ! e.mouseWasDraggedSinceMouseDown() && onClick)
        onClick();
}

void SignalChainComponent::GroupBorder::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::grey);
    g.drawRect (getLocalBounds(), 1);
}

void SignalChainComponent::ChainDropArea::paintOverChildren (juce::Graphics& g)
{
    if (hoveredGapIndex < 0 || hoveredGapIndex >= (int) dropGaps.size())
        return;

    // Drawn OVER children (not just behind) so the insertion indicator stays visible regardless of
    // what block/arrow/group border happens to be underneath it.
    int x = dropGaps[(size_t) hoveredGapIndex].first;
    g.setColour (juce::Colours::dodgerblue);
    g.fillRect (x - 1, 0, 2, getHeight());
}

bool SignalChainComponent::ChainDropArea::isInterestedInDragSource (const SourceDetails& dragSourceDetails)
{
    return dragSourceDetails.description.isString();
}

void SignalChainComponent::ChainDropArea::itemDragEnter (const SourceDetails& dragSourceDetails)
{
    updateHoveredGap (dragSourceDetails.localPosition);
}

void SignalChainComponent::ChainDropArea::itemDragMove (const SourceDetails& dragSourceDetails)
{
    updateHoveredGap (dragSourceDetails.localPosition);
}

void SignalChainComponent::ChainDropArea::itemDragExit (const SourceDetails&)
{
    hoveredGapIndex = -1;
    repaint();
}

void SignalChainComponent::ChainDropArea::itemDropped (const SourceDetails& dragSourceDetails)
{
    if (hoveredGapIndex >= 0 && hoveredGapIndex < (int) dropGaps.size() && onDropped)
        onDropped (dragSourceDetails.description.toString(), dropGaps[(size_t) hoveredGapIndex].second);

    hoveredGapIndex = -1;
    repaint();
}

void SignalChainComponent::ChainDropArea::updateHoveredGap (juce::Point<int> localPosition)
{
    int closestIndex = -1;
    int closestDistance = std::numeric_limits<int>::max();

    for (int i = 0; i < (int) dropGaps.size(); ++i)
    {
        int distance = std::abs (dropGaps[(size_t) i].first - localPosition.x);
        if (distance < closestDistance)
        {
            closestDistance = distance;
            closestIndex = i;
        }
    }

    if (closestIndex != hoveredGapIndex)
    {
        hoveredGapIndex = closestIndex;
        repaint();
    }
}

SignalChainComponent::SignalChainComponent (Rack::RackController& controllerToUse)
    : paramsPanel (controllerToUse), controller (controllerToUse)
{
    controller.addListener (this);

    addAndMakeVisible (presetSelector);
    addAndMakeVisible (renameButton);
    addAndMakeVisible (saveToUnitButton);
    addAndMakeVisible (refreshRigListButton);
    addAndMakeVisible (rigStatusLabel);
    addAndMakeVisible (chainLabel);
    addAndMakeVisible (chainViewport);
    addAndMakeVisible (paramsPanel);
    addAndMakeVisible (noSlotLabel);

    chainViewport.setViewedComponent (&chainContent, false);
    chainViewport.setScrollBarsShown (false, true);
    chainContent.onDropped = [this] (const juce::String& draggedId, int insertBeforeChainIndex)
    {
        handleChainReorder (draggedId, insertBeforeChainIndex);
    };

    rigEntries.resize ((size_t) (RackController::kNumBanks * RackController::kRigsPerBank));
    rigStatusLabel.setText ("Click Refresh Rig List to fetch real rig names.", juce::dontSendNotification);

    // No separate "Preset" label - the dropdown's own placeholder text carries that meaning
    // instead, matching docs/mockups/signal-chain-editor-concept-notes.md.
    presetSelector.setTextWhenNothingSelected ("Select a preset...");

    refreshRigListButton.onClick = [this] { refreshRigList(); };
    presetSelector.onChange = [this] { presetSelected(); };

    renameButton.setTooltip ("Rename preset");
    renameButton.onClick = [this] { showRenamePopup(); };
    saveToUnitButton.onClick = [this] { showSaveConfirmPopup(); };
    updateActionButtonsEnabled();

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

void SignalChainComponent::paint (juce::Graphics& g)
{
    // Panel the chain sits inside, filled with the "surface" colour real ComboBoxes/dropdowns use -
    // each block then fills its OWN background with the base window colour so it visually sits on
    // top of this panel instead of blending into it. See
    // docs/mockups/signal-chain-editor-concept-notes.md "Chain row panel".
    auto panelBounds = chainViewport.getBounds();
    g.setColour (findColour (juce::ComboBox::backgroundColourId));
    g.fillRect (panelBounds);
    g.setColour (findColour (juce::ComboBox::outlineColourId));
    g.drawRect (panelBounds, 1);
}

void SignalChainComponent::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto presetRow = area.removeFromTop (30);
    presetSelector.setBounds (presetRow.removeFromLeft (260).reduced (2));
    renameButton.setBounds (presetRow.removeFromLeft (30).reduced (2));
    presetRow.removeFromLeft (4);
    saveToUnitButton.setBounds (presetRow.removeFromLeft (110).reduced (2));
    presetRow.removeFromLeft (8);
    refreshRigListButton.setBounds (presetRow.removeFromLeft (140).reduced (2));
    rigStatusLabel.setBounds (presetRow.reduced (2));

    area.removeFromTop (10);
    chainLabel.setBounds (area.removeFromTop (18));
    area.removeFromTop (4);

    // Tall enough for the Amp/Cab group (block height + its own outer padding, the tallest row
    // item - see rebuildChainUi() for how shorter items are vertically centred within this) PLUS
    // the panel's own padding on top and bottom, matching .chain-panel's CSS padding - kept exactly
    // equal to chainContent's own height (see rebuildChainUi()) so the row doesn't look stuck to
    // the panel's top edge when it's not wide enough to need the horizontal scrollbar.
    constexpr int rowHeight = kBlockHeight + 2 * kGroupPadding;
    constexpr int panelHeight = rowHeight + 2 * kChainPanelPadding;
    chainViewport.setBounds (area.removeFromTop (panelHeight));

    area.removeFromTop (12);
    paramsPanel.setBounds (area);
    noSlotLabel.setBounds (area);
}

void SignalChainComponent::rebuildChainUi()
{
    blockComponents.clear();
    arrowLabels.clear();
    groupBorders.clear();
    chainContent.removeAllChildren();
    chainContent.dropGaps.clear();

    // Every block/arrow sits at the same y/height, vertically centred within the tallest row item
    // (the Amp/Cab group, which is taller due to its own outer border+padding) - see
    // docs/mockups/signal-chain-editor-concept-notes.md "Chain row panel". The whole row is also
    // inset by the panel's own padding on every side (matching .chain-panel's CSS padding), instead
    // of blocks sitting flush against the panel's border.
    constexpr int rowHeight = kBlockHeight + 2 * kGroupPadding;
    constexpr int blockY = kChainPanelPadding + kGroupPadding;

    int x = kChainPanelPadding;
    for (int i = 0; i < (int) chain.size(); ++i)
    {
        // Amp and Cab must always stay adjacent (Amp immediately before Cab - see the class doc
        // comment) - wrap both, plus the arrow between them, in one shared bordered rectangle so
        // that fixed-adjacency constraint reads visually, not just via the arrow between them. See
        // "Amp/Cab pairing" in signal-chain-editor-concept-notes.md.
        bool startsAmpCabGroup = chain[(size_t) i].id == "amp" && i + 1 < (int) chain.size()
                                 && chain[(size_t) i + 1].id == "cab";
        int groupStartX = x;

        auto block = std::make_unique<Block>();
        block->setInfo (chain[(size_t) i], i == selectedIndex);
        block->setBounds (x, blockY, kBlockWidth, kBlockHeight);
        block->onClick = [this, i] { selectBlock (i); };
        chainContent.addAndMakeVisible (*block);
        blockComponents.push_back (std::move (block));
        x += kBlockWidth;

        if (i < (int) chain.size() - 1)
        {
            auto arrow = std::make_unique<juce::Label> (juce::String(), juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92")));
            arrow->setJustificationType (juce::Justification::centred);
            arrow->setColour (juce::Label::textColourId, juce::Colours::grey);
            arrow->setBounds (x, blockY, kArrowWidth, kBlockHeight);
            chainContent.addAndMakeVisible (*arrow);
            arrowLabels.push_back (std::move (arrow));

            // Every arrow-gap is a valid drop point EXCEPT the one between an "amp" block and its
            // "cab" successor - simply not recorded, so a drop can never land between them and
            // split the pair. See ChainDropArea's doc comment in SignalChainComponent.h.
            if (! startsAmpCabGroup)
                chainContent.dropGaps.push_back ({ x + kArrowWidth / 2, i + 1 });

            x += kArrowWidth;
        }

        if (startsAmpCabGroup)
        {
            // Cab (chain[i + 1]) renders on the next loop iteration as normal - this just needs to
            // know it'll end up one more block width past here. Added behind everything added so
            // far via toBack(); anything added later (Cab itself) still lands on top of it too.
            auto group = std::make_unique<GroupBorder>();
            int groupWidth = (x - groupStartX) + kBlockWidth + 2 * kGroupPadding;
            group->setBounds (groupStartX - kGroupPadding, kChainPanelPadding, groupWidth, rowHeight);
            chainContent.addAndMakeVisible (*group);
            group->toBack();
            groupBorders.push_back (std::move (group));
        }
    }

    // Matching trailing padding on the right, and top+bottom (rowHeight already covers the row
    // itself) - kept exactly equal to chainViewport's own height in resized() so the content fills
    // the panel evenly instead of leaving empty space below when no horizontal scrolling is needed.
    chainContent.setSize (x + kChainPanelPadding, rowHeight + 2 * kChainPanelPadding);
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
        paramsPanel.setSlot (*slot, block.decodedEffectId, block.decodedBypass, block.decodedToggleStates,
                             block.decodedKnobValues);
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

void SignalChainComponent::handleChainReorder (const juce::String& draggedId, int insertBeforeChainIndex)
{
    // Pure in-memory reorder - no RackController call at all, see the class doc comment. Only
    // ordinary blocks are ever drag sources (Block::draggable is false for Input/Output/Amp/Cab),
    // so draggedId always matches exactly one chain entry here.
    int draggedIndex = -1;
    for (int i = 0; i < (int) chain.size(); ++i)
        if (chain[(size_t) i].id == draggedId)
        {
            draggedIndex = i;
            break;
        }

    // Not found, or dropped adjacent to its own current position - either way, a no-op.
    if (draggedIndex < 0 || draggedIndex == insertBeforeChainIndex || draggedIndex + 1 == insertBeforeChainIndex)
        return;

    juce::String previouslySelectedId = (selectedIndex >= 0 && selectedIndex < (int) chain.size())
                                             ? chain[(size_t) selectedIndex].id
                                             : juce::String();

    ChainBlock moved = chain[(size_t) draggedIndex];
    chain.erase (chain.begin() + draggedIndex);

    // insertBeforeChainIndex was computed against the array BEFORE the erase above - shift it down
    // by one if the removed entry was before it.
    int adjustedInsertIndex = insertBeforeChainIndex > draggedIndex ? insertBeforeChainIndex - 1 : insertBeforeChainIndex;
    chain.insert (chain.begin() + adjustedInsertIndex, moved);

    // Same by-id selection-preservation pattern as updateBlockDataFromRig() below - the order just
    // changed, so the previously selected block's INDEX is stale, but its id still identifies it.
    int newIndex = -1;
    for (int i = 0; i < (int) chain.size(); ++i)
        if (chain[(size_t) i].id == previouslySelectedId)
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

                // Any OTHER toggle- or knob-kind param bestEffortRawTagForKey() has a raw tag for -
                // exact-confirmed (e.g. Tape Echo's "Hiss", Wah's "Filt") or a plausible name-
                // similarity match against real data (e.g. Flanger's "Rate") - see SlotConfig.h.
                // Selector-kind params (Sync, Type) have no confirmed encoding, so they're skipped
                // here; params with no recorded correspondence at all (e.g. Tape Echo's remaining
                // unmatched keys) are left alone too - there is no guessing beyond what's recorded.
                if (def)
                {
                    for (const auto& param : def->params)
                    {
                        auto rawTag = bestEffortRawTagForKey (def->name, param.key);
                        if (! rawTag)
                            continue;

                        auto valueIt = slot.params.find (rawTag->toStdString());
                        if (valueIt == slot.params.end())
                            continue;

                        if (param.kind == Rack::EffectDefinitions::ParamKind::toggle)
                            block.decodedToggleStates[juce::String (param.key)] = valueIt->second != 0;
                        else if (param.kind == Rack::EffectDefinitions::ParamKind::knob)
                            block.decodedKnobValues[juce::String (param.key)] = knobRawToCcValue (valueIt->second);
                    }
                }
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
    updateActionButtonsEnabled();
    rigStatusLabel.setText ("Fetching rig names... 0/" + juce::String ((int) rigEntries.size()), juce::dontSendNotification);

    controller.requestAllRigNames();
}

void SignalChainComponent::presetSelected()
{
    updateActionButtonsEnabled();

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

void SignalChainComponent::updateActionButtonsEnabled()
{
    // Renaming/saving only makes sense once a real preset (not the "Select a preset..." placeholder)
    // is selected - see docs/mockups/signal-chain-editor-concept-notes.md.
    bool hasSelection = presetSelector.getSelectedId() > 0;
    renameButton.setEnabled (hasSelection);
    saveToUnitButton.setEnabled (hasSelection);
}

std::optional<RackController::RigId> SignalChainComponent::currentRigId() const
{
    auto id = presetSelector.getSelectedId();
    if (id < 1)
        return std::nullopt;

    int index = id - 1;
    return RackController::RigId { (uint8_t) (index / RackController::kRigsPerBank),
                                    (uint8_t) (index % RackController::kRigsPerBank) };
}

juce::String SignalChainComponent::currentRigDisplayName() const
{
    auto rig = currentRigId();
    if (! rig)
        return {};

    int index = rig->bank * RackController::kRigsPerBank + rig->rig;
    if (index < 0 || index >= (int) rigEntries.size())
        return {};

    return rigEntries[(size_t) index].name;
}

void SignalChainComponent::showRenamePopup()
{
    auto rig = currentRigId();
    if (! rig)
        return;

    auto location = "Bank " + juce::String (rig->bank) + " " + rigLocationLabel (rig->rig);
    auto currentName = currentRigDisplayName();
    int index = rig->bank * RackController::kRigsPerBank + rig->rig;

    auto content = std::make_unique<RenamePopupContent> (location, currentName,
        [this, index] (const juce::String& newName)
        {
            // Deliberately NOT calling RackController::setRigName() - see RenamePopupContent's doc
            // comment above and RackController.h's "NOT YET HARDWARE-VALIDATED" note on that method.
            // This just updates local UI state, so the dropdown reflects what a real write would
            // show, without actually writing anything to the unit yet.
            if (index >= 0 && index < (int) rigEntries.size())
            {
                rigEntries[(size_t) index].name = newName;
                updatePresetSelectorItems();
                presetSelector.setSelectedId (index + 1, juce::dontSendNotification);
            }
            rigStatusLabel.setText ("Renamed locally to \"" + newName + "\" (not yet written to the unit).",
                                     juce::dontSendNotification);
        });

    juce::CallOutBox::launchAsynchronously (std::move (content), renameButton.getScreenBounds(), nullptr);
}

void SignalChainComponent::showSaveConfirmPopup()
{
    auto rig = currentRigId();
    if (! rig)
        return;

    auto location = "Bank " + juce::String (rig->bank) + " " + rigLocationLabel (rig->rig);
    auto name = currentRigDisplayName();
    auto message = "Overwrite \"" + location + ": " + name
                       + "\" on the unit with the current settings? This cannot be undone.";

    auto content = std::make_unique<SaveConfirmPopupContent> (message,
        [this, location, name]
        {
            // Deliberately NOT calling RackController::saveRig() - see SaveConfirmPopupContent's
            // doc comment above and RackController.h's "NOT YET HARDWARE-VALIDATED" note on that
            // method.
            rigStatusLabel.setText ("(not yet saved to unit) Would overwrite \"" + location + ": "
                                         + name + "\".",
                                     juce::dontSendNotification);
        });

    juce::CallOutBox::launchAsynchronously (std::move (content), saveToUnitButton.getScreenBounds(), nullptr);
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
