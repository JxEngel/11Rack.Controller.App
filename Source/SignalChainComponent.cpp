#include "SignalChainComponent.h"
#include "SlotConfig.h"

#include <cmath>
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

    // Rig globals (Main Volume/Tuner/Tap Tempo/FX Loop) - ported from the now-removed
    // RigGlobalsComponent, see the class doc comment.
    constexpr double kDisplayCenter = 5.0;
    constexpr double kDisplayHalfRange = 5.0;
    constexpr double kRawHalfRange = 127.0;

    // Fixed CCs, per the official MIDI CC chart (docs/protocol-spec.md) - not part of any
    // per-effect "Setting N" positional scheme, since there's only one Tap Tempo and one FX Loop.
    constexpr uint8_t kTapTempoCc = 64;      // "Tap Tempo" - 64-127 = a tap
    constexpr uint8_t kFxLoopBypassCc = 107; // "FX Loop On/Off"
    constexpr uint8_t kFxLoopSendCc = 19;    // "FX Loop Send"
    constexpr uint8_t kFxLoopReturnCc = 108; // "FX Loop Return"
    constexpr uint8_t kFxLoopMixCc = 88;     // "FX Loop Mix"

    void setupGlobalsKnob (juce::Slider& slider)
    {
        slider.setRange (0, 127, 1);
        slider.setValue (64, juce::dontSendNotification);
        // Style/LookAndFeel applied separately via SignalChainComponent::applyGlobalsKnobStyle() -
        // this only sets up the range/value every knob style shares.
    }

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

    // Which SlotConfig (see SlotConfig.cpp) a chain block should open in the editor panel. Cab/FX
    // Loop still have no SlotConfig (no independently-known parameters for Cab at all; FX Loop
    // already has its own dedicated controls in the "Rig globals" row above the chain) - clicking
    // those shows the fallback label.
    juce::String slotConfigNameForBlockId (const juce::String& id)
    {
        if (id == "disto") return "Distortion";
        if (id == "wah") return "Wah";
        if (id == "mod") return "Mod";
        if (id == "delay") return "Delay";
        if (id == "reverb") return "Reverb";
        if (id == "fx1") return "FX1";
        if (id == "fx2") return "FX2";
        if (id == "vol") return "Volume Pedal";
        if (id == "amp") return "Amp/Cab";
        return {};
    }

    // Resolves a Rig Params selector's raw value to its option name (e.g. "PIGI" 11 -> "22 kOhm +
    // Cap") - used for Input's read-only True-Z/Input Selector display. Returns an empty string if
    // the key or value isn't found, rather than guessing.
    juce::String rigParamOptionName (const char* paramKey, int rawValue)
    {
        auto def = Rack::EffectDefinitions::lookup (-1); // "Rig Params"
        if (! def)
            return {};

        for (const auto& param : def->params)
        {
            if (param.key != paramKey)
                continue;

            for (const auto& opt : param.options)
                if (opt.value == rawValue)
                    return juce::String (opt.name);
        }

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
    // A completed drag shouldn't also re-select the source block as a click. Every block is
    // selectable, even Input/Output (no SlotConfig, but Input has a real editor to show - see
    // showInputEditor()) - selectBlock() decides what to actually display.
    if (! e.mouseWasDraggedSinceMouseDown() && onClick)
        onClick();
}

void SignalChainComponent::GroupBorder::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::grey);
    g.drawRect (getLocalBounds(), 1);
}

void SignalChainComponent::ChainDropArea::paintOverChildren (juce::Graphics& g)
{
    if (placeholderX < 0)
        return;

    // Drawn OVER children (not just behind) so the placeholder stays visible regardless of what
    // block/arrow/group border happens to be underneath it. SignalChainComponent::
    // updateChainHoverPreview() has already both closed the hole at the dragged block's origin AND
    // opened a gap at the drop point (shifting blocks/groups left or right as needed), so this
    // blank, block-shaped rect at placeholderX now lands in a genuinely empty gap instead of
    // overlaying real content.
    juce::Rectangle<int> placeholder (placeholderX, kChainPanelPadding + kGroupPadding, kBlockWidth, kBlockHeight);

    g.setColour (findColour (juce::ComboBox::backgroundColourId));
    g.fillRect (placeholder);
    g.setColour (juce::Colours::dodgerblue);
    g.drawRect (placeholder, 1);
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
    setHoveredGapIndex (-1);
}

void SignalChainComponent::ChainDropArea::itemDropped (const SourceDetails& dragSourceDetails)
{
    if (hoveredGapIndex >= 0 && hoveredGapIndex < (int) dropGaps.size() && onDropped)
        onDropped (dragSourceDetails.description.toString(), dropGaps[(size_t) hoveredGapIndex].second);

    setHoveredGapIndex (-1);
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

    setHoveredGapIndex (closestIndex);
}

void SignalChainComponent::ChainDropArea::setHoveredGapIndex (int newIndex)
{
    if (newIndex == hoveredGapIndex)
        return;

    hoveredGapIndex = newIndex;
    repaint();

    if (onHoverChanged)
        onHoverChanged (hoveredGapIndex);
}

void SignalChainComponent::dragOperationStarted (const juce::DragAndDropTarget::SourceDetails& sourceDetails)
{
    draggedBlockComponent = sourceDetails.sourceComponent;
    draggedBlockId = sourceDetails.description.toString();
    if (draggedBlockComponent != nullptr)
        draggedBlockComponent->setVisible (false);
}

void SignalChainComponent::dragOperationEnded (const juce::DragAndDropTarget::SourceDetails&)
{
    // Null by now if the drop succeeded - handleChainReorder() already rebuilt every Block from
    // scratch (destroying the dragged one) before this fires. Only reached with a live pointer on
    // an invalid/cancelled drop, where restoring visibility IS the "snap back" - see the class doc
    // comment on dragOperationStarted()/dragOperationEnded() in SignalChainComponent.h.
    if (draggedBlockComponent != nullptr)
        draggedBlockComponent->setVisible (true);

    draggedBlockComponent = nullptr;
    draggedBlockId = {};
}

int8_t SignalChainComponent::displayToRaw (double display)
{
    double raw = (display - kDisplayCenter) * (kRawHalfRange / kDisplayHalfRange);
    raw = juce::jlimit (-127.0, 127.0, raw);
    return static_cast<int8_t> (std::lround (raw));
}

double SignalChainComponent::rawToDisplay (int raw)
{
    return kDisplayCenter + (static_cast<double> (raw) * (kDisplayHalfRange / kRawHalfRange));
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
    addAndMakeVisible (globalsLabel);
    addAndMakeVisible (volumeLabel);
    addAndMakeVisible (volumeSlider);
    addAndMakeVisible (tunerLabel);
    addAndMakeVisible (tunerOnButton);
    addAndMakeVisible (tunerOffButton);
    addAndMakeVisible (tunerStatusLabel);
    addAndMakeVisible (tapTempoLabel);
    addAndMakeVisible (tapTempoButton);
    addAndMakeVisible (fxLoopLabel);
    addAndMakeVisible (fxLoopBypassToggle);
    addAndMakeVisible (fxLoopSendLabel);
    addAndMakeVisible (fxLoopSendSlider);
    addAndMakeVisible (fxLoopReturnLabel);
    addAndMakeVisible (fxLoopReturnSlider);
    addAndMakeVisible (fxLoopMixLabel);
    addAndMakeVisible (fxLoopMixSlider);

    // Centred directly above their knob, matching SlotParamsPanel's per-effect knob grid (see its
    // own doc comment) - not left-aligned group headers like tunerLabel/tapTempoLabel/fxLoopLabel
    // above, which don't sit over one specific knob. Same smaller font too, for visual consistency
    // across every knob in the app.
    for (auto* knobLabel : { &volumeLabel, &fxLoopSendLabel, &fxLoopReturnLabel, &fxLoopMixLabel })
    {
        knobLabel->setJustificationType (juce::Justification::centred);
        knobLabel->setFont (juce::Font (juce::FontOptions (11.0f)));
    }

    addAndMakeVisible (chainLabel);
    addAndMakeVisible (chainViewport);
    addAndMakeVisible (paramsPanel);
    addAndMakeVisible (noSlotLabel);
    addAndMakeVisible (inputEditorPanel);
    inputEditorPanel.addAndMakeVisible (inputSelectorLabel);
    inputEditorPanel.addAndMakeVisible (inputSelectorCombo);
    inputEditorPanel.addAndMakeVisible (trueZLabel);
    inputEditorPanel.addAndMakeVisible (trueZCombo);
    inputEditorPanel.addAndMakeVisible (inputEditorNoteLabel);

    chainViewport.setViewedComponent (&chainContent, false);
    chainViewport.setScrollBarsShown (false, true);
    chainContent.onDropped = [this] (const juce::String& draggedId, int insertBeforeChainIndex)
    {
        handleChainReorder (draggedId, insertBeforeChainIndex);
    };
    chainContent.onHoverChanged = [this] (int hoveredGapIndex) { updateChainHoverPreview (hoveredGapIndex); };

    // Rig globals - ported directly from the now-removed RigGlobalsComponent, see the class doc
    // comment. Shows the unit's own 0.0-10.0 display scale, not the raw wire value - see
    // displayToRaw(). Style/LookAndFeel applied below via applyGlobalsKnobStyle(), same as the
    // other 3 globals knobs.
    volumeSlider.setRange (0.0, 10.0, 0.1);
    volumeSlider.setValue (kDisplayCenter, juce::dontSendNotification);

    // Live two-way sync: dragging sends on every change. Receiving a device-confirmed value
    // (onMainVolumeReceived, below) moves the slider with dontSendNotification so it doesn't loop
    // back into another send.
    volumeSlider.onValueChange = [this]
    {
        controller.setMainVolume (displayToRaw (volumeSlider.getValue()));
    };

    tunerStatusLabel.setText ("Tuner state: unknown (no query exists)", juce::dontSendNotification);

    tunerOnButton.onClick = [this] { controller.setTunerOn (true); };
    tunerOffButton.onClick = [this] { controller.setTunerOn (false); };

    // Momentary - one click, one tap. Nothing to read back or sync.
    tapTempoButton.onClick = [this] { controller.sendMidiCc (kTapTempoCc, 127); };

    fxLoopBypassToggle.onClick = [this]
    {
        bool bypassed = fxLoopBypassToggle.getToggleState();
        // 0-63 = Off, 64-127 = On, per the official CC chart.
        controller.sendMidiCc (kFxLoopBypassCc, bypassed ? 0 : 127);
    };

    setupGlobalsKnob (fxLoopSendSlider);
    fxLoopSendSlider.onValueChange = [this]
    {
        controller.sendMidiCc (kFxLoopSendCc, static_cast<uint8_t> (static_cast<int> (fxLoopSendSlider.getValue())));
    };

    setupGlobalsKnob (fxLoopReturnSlider);
    fxLoopReturnSlider.onValueChange = [this]
    {
        controller.sendMidiCc (kFxLoopReturnCc, static_cast<uint8_t> (static_cast<int> (fxLoopReturnSlider.getValue())));
    };

    setupGlobalsKnob (fxLoopMixSlider);
    fxLoopMixSlider.onValueChange = [this]
    {
        controller.sendMidiCc (kFxLoopMixCc, static_cast<uint8_t> (static_cast<int> (fxLoopMixSlider.getValue())));
    };

    applyGlobalsKnobStyle (KnobStyle::goldMetallic);
    applyGlobalsToggleStyle (ToggleStyle::rockerSwitch);

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

    // Local-only edits (see pendingInputSelectorValue/pendingTrueZValue's doc comment in the header
    // for why - no known MIDI CC exists for either field, and reaching the unit on a real save needs
    // a Bulk Rig encoder that doesn't exist yet). Item IDs are the option's raw value + 1 (same
    // convention SlotParamsPanel.cpp uses for its selectors), so a 0-valued option stays selectable.
    inputSelectorCombo.onChange = [this]
    {
        auto id = inputSelectorCombo.getSelectedId();
        if (id > 0)
            pendingInputSelectorValue = id - 1;
    };
    trueZCombo.onChange = [this]
    {
        auto id = trueZCombo.getSelectedId();
        if (id > 0)
            pendingTrueZValue = id - 1;
    };

    inputEditorNoteLabel.setText (
        "Not synced live with the unit - no known MIDI CC exists for these fields (see "
        "docs/master-control-map.md " + juce::String (juce::CharPointer_UTF8 ("\xc2\xa7")) + "5). Your "
        "selection here is local only for now; wiring it into a real \"Save to Unit\" write needs a "
        "Bulk Rig encoder that doesn't exist yet. Selecting a different preset resets these back to "
        "whatever's actually decoded from the unit.",
        juce::dontSendNotification);
    inputEditorNoteLabel.setJustificationType (juce::Justification::topLeft);

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

void SignalChainComponent::setKnobStyle (KnobStyle style)
{
    paramsPanel.setKnobStyle (style);
    applyGlobalsKnobStyle (style);
}

void SignalChainComponent::setToggleStyle (ToggleStyle style)
{
    paramsPanel.setToggleStyle (style);
    applyGlobalsToggleStyle (style);
}

void SignalChainComponent::applyGlobalsKnobStyle (KnobStyle style)
{
    // A switch (not if/else) so adding a new enumerator without a matching case here warns at
    // compile time instead of silently doing nothing - see KnobStyle.h.
    for (auto* slider : { &volumeSlider, &fxLoopSendSlider, &fxLoopReturnSlider, &fxLoopMixSlider })
    {
        switch (style)
        {
            case KnobStyle::goldMetallic:
                slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
                slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
                slider->setLookAndFeel (&goldKnobLookAndFeel);
                break;
        }
    }
}

void SignalChainComponent::applyGlobalsToggleStyle (ToggleStyle style)
{
    // A switch (not if/else) so adding a new enumerator without a matching case here warns at
    // compile time instead of silently doing nothing - see ToggleStyle.h.
    switch (style)
    {
        case ToggleStyle::rockerSwitch:
            fxLoopBypassToggle.setLookAndFeel (&rockerSwitchLookAndFeel);
            break;
    }
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
    // docs/mockups/signal-chain-editor-concept-notes.md "Chain row panel". The Rig globals row gets
    // the exact same treatment, for the same reason and the same visual consistency.
    auto panelBounds = chainViewport.getBounds();
    g.setColour (findColour (juce::ComboBox::backgroundColourId));
    g.fillRect (panelBounds);
    g.fillRect (globalsPanelBounds);
    g.setColour (findColour (juce::ComboBox::outlineColourId));
    g.drawRect (panelBounds, 1);
    g.drawRect (globalsPanelBounds, 1);
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
    globalsLabel.setBounds (area.removeFromTop (18));
    area.removeFromTop (4);

    // "Rig globals" row - four groups side by side (Main Volume/Tuner/Tap Tempo/FX Loop), matching
    // docs/mockups/signal-chain-editor-concept.html's ".globals-panel" horizontal layout, inside its
    // own bordered/filled panel (same treatment as the chain panel below - see paint()). Tuner/Tap
    // Tempo stay fixed-width (buttons don't benefit from extra width); Main Volume's knob and FX
    // Loop's 3 mini knobs sit in columns that DO stretch to use whatever width is left over on a
    // wide window instead of leaving it blank - unlike the chain's own blocks, which stay fixed-width
    // by deliberate earlier decision (see signal-chain-editor-concept-notes.md "Chain slots use a
    // fixed width"). Each knob's own rendered diameter is still capped by its row's fixed HEIGHT
    // (GoldKnobLookAndFeel draws at min(width,height) - see its doc comment), so extra column width
    // just centers the dial rather than distorting it.
    constexpr int globalsRowHeight = 190; // tall enough for a rotary knob + its value box, and a legible rocker switch below FX Loop's label
    constexpr int globalsPanelPadding = 10; // matches kChainPanelPadding's role for the chain panel
    globalsPanelBounds = area.removeFromTop (globalsRowHeight + 2 * globalsPanelPadding);
    auto globalsRow = globalsPanelBounds.reduced (globalsPanelPadding);
    int gy = globalsRow.getY();

    constexpr int colGap = 20;
    constexpr int tunerColWidth = 170;
    constexpr int tapColWidth = 80;

    // Whatever's left after the two fixed-width columns and gaps, split between Main Volume and FX
    // Loop - clamped so neither an overly-narrow nor an absurdly-wide window looks broken.
    int flexibleWidth = globalsRow.getWidth() - tunerColWidth - tapColWidth - 3 * colGap;
    int volumeColWidth = juce::jlimit (170, 480, (int) (flexibleWidth * 0.35));
    int fxColWidth = juce::jmax (240, flexibleWidth - volumeColWidth);

    auto volumeCol = globalsRow.removeFromLeft (volumeColWidth);
    volumeLabel.setBounds (volumeCol.getX(), gy, volumeCol.getWidth(), 18);
    volumeSlider.setBounds (volumeCol.getX(), gy + 22, volumeCol.getWidth(), 90);
    globalsRow.removeFromLeft (colGap);

    auto tunerCol = globalsRow.removeFromLeft (tunerColWidth);
    tunerLabel.setBounds (tunerCol.getX(), gy, tunerCol.getWidth(), 18);
    tunerOnButton.setBounds (tunerCol.getX(), gy + 22, 80, 26);
    tunerOffButton.setBounds (tunerCol.getX() + 86, gy + 22, 80, 26);
    tunerStatusLabel.setBounds (tunerCol.getX(), gy + 52, tunerCol.getWidth(), 32);
    globalsRow.removeFromLeft (colGap);

    auto tapCol = globalsRow.removeFromLeft (tapColWidth);
    tapTempoLabel.setBounds (tapCol.getX(), gy, tapCol.getWidth(), 18);
    tapTempoButton.setBounds (tapCol.getX(), gy + 22, 70, 26);
    globalsRow.removeFromLeft (colGap);

    auto fxCol = globalsRow.removeFromLeft (fxColWidth);
    fxLoopLabel.setBounds (fxCol.getX(), gy, fxCol.getWidth(), 18);
    fxLoopBypassToggle.setBounds (fxCol.getX(), gy + 22, 120, 46);

    constexpr int knobGap = 12;
    int knobWidth = (fxCol.getWidth() - 2 * knobGap) / 3;
    int kx = fxCol.getX();
    int knobsLabelY = gy + 74;
    for (auto* knobLabel : { &fxLoopSendLabel, &fxLoopReturnLabel, &fxLoopMixLabel })
    {
        knobLabel->setBounds (kx, knobsLabelY, knobWidth, 14);
        kx += knobWidth + knobGap;
    }
    kx = fxCol.getX();
    int knobsSliderY = knobsLabelY + 16;
    for (auto* knobSlider : { &fxLoopSendSlider, &fxLoopReturnSlider, &fxLoopMixSlider })
    {
        knobSlider->setBounds (kx, knobsSliderY, knobWidth, 90);
        kx += knobWidth + knobGap;
    }

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

    inputEditorPanel.setBounds (area);
    auto inputArea = inputEditorPanel.getLocalBounds().reduced (10);

    auto selectorRow = inputArea.removeFromTop (30);
    inputSelectorLabel.setBounds (selectorRow.removeFromLeft (110).reduced (2));
    inputSelectorCombo.setBounds (selectorRow.reduced (2));

    inputArea.removeFromTop (8);
    auto trueZRow = inputArea.removeFromTop (30);
    trueZLabel.setBounds (trueZRow.removeFromLeft (110).reduced (2));
    trueZCombo.setBounds (trueZRow.reduced (2));

    inputArea.removeFromTop (16);
    inputEditorNoteLabel.setBounds (inputArea);
}

void SignalChainComponent::rebuildChainUi()
{
    blockComponents.clear();
    arrowLabels.clear();
    groupBorders.clear();
    blockBaseX.clear();
    arrowBaseX.clear();
    groupBaseX.clear();
    groupChainIndex.clear();
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
        blockBaseX.push_back (x);
        x += kBlockWidth;

        if (i < (int) chain.size() - 1)
        {
            auto arrow = std::make_unique<juce::Label> (juce::String(), juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92")));
            arrow->setJustificationType (juce::Justification::centred);
            arrow->setColour (juce::Label::textColourId, juce::Colours::grey);
            arrow->setBounds (x, blockY, kArrowWidth, kBlockHeight);
            chainContent.addAndMakeVisible (*arrow);
            arrowLabels.push_back (std::move (arrow));
            arrowBaseX.push_back (x);

            // Every arrow-gap is a valid drop point EXCEPT the one between an "amp" block and its
            // "cab" successor - simply not recorded, so a drop can never land between them and
            // split the pair. See ChainDropArea's doc comment in SignalChainComponent.h. The
            // recorded x (x + kArrowWidth) is exactly where the NEXT block's own base x will be -
            // see updateChainHoverPreview(), which shifts anything at or past this x aside.
            if (! startsAmpCabGroup)
                chainContent.dropGaps.push_back ({ x + kArrowWidth, i + 1 });

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
            groupBaseX.push_back (groupStartX - kGroupPadding);
            groupChainIndex.push_back (i); // the Amp block's own chain index
        }
    }

    // Matching trailing padding on the right, and top+bottom (rowHeight already covers the row
    // itself) - kept exactly equal to chainViewport's own height in resized() so the content fills
    // the panel evenly instead of leaving empty space below when no horizontal scrolling is needed.
    // This is the row's NATURAL width, with no hover-shift applied - see updateChainHoverPreview(),
    // which temporarily grows chainContent by one block+arrow width while a drag is hovering, so
    // the shifted-right tail doesn't get clipped by the viewport.
    chainContentBaseWidth = x + kChainPanelPadding;
    chainContent.setSize (chainContentBaseWidth, rowHeight + 2 * kChainPanelPadding);
}

void SignalChainComponent::updateChainHoverPreview (int hoveredGapIndex)
{
    constexpr int shiftAmount = kBlockWidth + kArrowWidth;

    bool hovering = hoveredGapIndex >= 0 && hoveredGapIndex < (int) chainContent.dropGaps.size();

    // Find the dragged block's OWN original chain index too - without it, this can only open a
    // gap at the drop point (the old behaviour), not ALSO close the hole left behind at the
    // dragged block's origin, which is what actually made it look like "two blocks" - see the
    // conversation this was fixed from. Not found (e.g. between drags) just means no shift at all.
    int draggedIdx = -1;
    if (hovering)
        for (int i = 0; i < (int) chain.size(); ++i)
            if (chain[(size_t) i].id == draggedBlockId)
            {
                draggedIdx = i;
                break;
            }
    hovering = hovering && draggedIdx >= 0;

    int insertBeforeIdx = hovering ? chainContent.dropGaps[(size_t) hoveredGapIndex].second : -1;
    // Forward move (dragging something later in the chain): everything strictly between the drag
    // origin and the drop point shifts LEFT to close the hole. Backward move (dragging something
    // earlier): everything from the drop point up to (not including) the drag origin shifts RIGHT
    // to make room. Either way, exactly one block+arrow width of space nets out at the drop point -
    // see the derivation in the conversation this was worked out from (verified against several
    // worked examples: arrows never need to move, only blocks/groups do, since a single item moving
    // by whole block+arrow slots always keeps the arrow grid aligned).
    bool forwardMove = hovering && insertBeforeIdx > draggedIdx;
    bool backwardMove = hovering && insertBeforeIdx <= draggedIdx;

    auto shiftForChainIndex = [&] (int chainIndex) -> int
    {
        if (forwardMove && chainIndex > draggedIdx && chainIndex < insertBeforeIdx)
            return -shiftAmount;
        if (backwardMove && chainIndex >= insertBeforeIdx && chainIndex < draggedIdx)
            return shiftAmount;
        return 0;
    };

    for (size_t i = 0; i < blockComponents.size(); ++i)
    {
        int shift = shiftForChainIndex ((int) i);
        blockComponents[i]->setTopLeftPosition (blockBaseX[i] + shift, blockComponents[i]->getY());
    }
    for (size_t i = 0; i < groupBorders.size(); ++i)
    {
        int shift = shiftForChainIndex (groupChainIndex[i]);
        groupBorders[i]->setTopLeftPosition (groupBaseX[i] + shift, groupBorders[i]->getY());
    }
    // Arrows deliberately never shift - see the comment above; they already line up correctly
    // against the shifted blocks/groups without needing to move themselves.

    if (hovering)
    {
        // Forward moves land the placeholder among the now-left-shifted blocks, so it needs the
        // same left shift; backward moves land it at its original (unshifted) gap position.
        int placeholderX = chainContent.dropGaps[(size_t) hoveredGapIndex].first + (forwardMove ? -shiftAmount : 0);
        chainContent.placeholderX = placeholderX;
    }
    else
    {
        chainContent.placeholderX = -1;
    }

    // Temporarily make room for the shifted-right tail so the viewport doesn't clip it - restored
    // to the natural width as soon as the hover ends.
    chainContent.setSize (hovering ? chainContentBaseWidth + shiftAmount : chainContentBaseWidth,
                          chainContent.getHeight());
    chainContent.repaint();
}

void SignalChainComponent::selectBlock (int index)
{
    if (index < 0 || index >= (int) chain.size())
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
        inputEditorPanel.setVisible (false);
    }
    else if (block.id == "input")
    {
        paramsPanel.clear();
        paramsPanel.setVisible (false);
        noSlotLabel.setVisible (false);
        inputEditorPanel.setVisible (true);
        showInputEditor (block);
    }
    else
    {
        // Every other non-mapped block (Output/Amp/Cab/Volume/FX Loop) - no SlotConfig, no
        // decodable info worth a dedicated editor - falls back to the generic message.
        paramsPanel.clear();
        paramsPanel.setVisible (false);
        inputEditorPanel.setVisible (false);
        noSlotLabel.setVisible (true);
        noSlotLabel.setJustificationType (juce::Justification::centred);
        noSlotLabel.setText ("No editable parameters mapped for this block yet.", juce::dontSendNotification);
    }
}

void SignalChainComponent::showInputEditor (const ChainBlock& block)
{
    auto rigParams = Rack::EffectDefinitions::lookup (-1); // "Rig Params"

    auto populate = [&rigParams] (juce::ComboBox& combo, const char* paramKey)
    {
        combo.clear (juce::dontSendNotification);
        if (! rigParams)
            return;

        for (const auto& param : rigParams->params)
        {
            if (param.key != paramKey)
                continue;

            for (const auto& opt : param.options)
                combo.addItem (opt.name, opt.value + 1); // +1: item IDs must be > 0
            break;
        }
    };

    populate (inputSelectorCombo, "WorB");
    populate (trueZCombo, "PIGI");

    auto selectValue = [] (juce::ComboBox& combo, std::optional<int32_t> value)
    {
        if (value)
            combo.setSelectedId (*value + 1, juce::dontSendNotification);
        else
            combo.setSelectedId (0, juce::dontSendNotification);
    };

    selectValue (inputSelectorCombo, pendingInputSelectorValue.has_value() ? pendingInputSelectorValue : block.decodedInputSelectorValue);
    selectValue (trueZCombo, pendingTrueZValue.has_value() ? pendingTrueZValue : block.decodedTrueZValue);
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

    // True-Z and Input Selector are rig-level globals (rig.rigGlobals, not tied to any lettered
    // slot - see the class doc comment on why Input can't be decoded the same way as the other
    // blocks) with no known live MIDI CC (confirmed 2026-07-28: cycling through every True-Z option
    // on real hardware produced the exact same unrelated async SysEx message every time, not a
    // value that tracked the selection - see docs/protocol-spec.md). Stored as raw values for
    // showInputEditor()'s dropdowns; a fresh decode always wins over any pending local edit, since
    // that edit belonged to whatever rig was loaded before - see pendingInputSelectorValue/
    // pendingTrueZValue's doc comment in the header.
    auto trueZIt = rig.rigGlobals.find ("PIGI");
    auto inputSelectorIt = rig.rigGlobals.find ("WorB");
    if (trueZIt != rig.rigGlobals.end())
        input.decodedTrueZValue = trueZIt->second;
    if (inputSelectorIt != rig.rigGlobals.end())
        input.decodedInputSelectorValue = inputSelectorIt->second;

    juce::String trueZName = input.decodedTrueZValue ? rigParamOptionName ("PIGI", *input.decodedTrueZValue) : juce::String();
    juce::String inputSelectorName = input.decodedInputSelectorValue ? rigParamOptionName ("WorB", *input.decodedInputSelectorValue) : juce::String();
    input.subLabel = inputSelectorName.isNotEmpty() && trueZName.isNotEmpty()
                          ? inputSelectorName + ", " + trueZName
                          : (inputSelectorName.isNotEmpty() ? inputSelectorName : trueZName);

    pendingInputSelectorValue.reset();
    pendingTrueZValue.reset();

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

                // Amp/Cab special case: the wire-level effectId is always 12 regardless of which of
                // the 16 amp models is loaded - the model itself lives in the "sld6" param instead
                // (confirmed 2026-07-27 to match ampModelOptions()'s own 0-15 index directly,
                // "twenty-third round"). Resolve the real per-model synthetic ID
                // (EffectDefinitions.cpp's 1000+index scheme - see its Amp/Cab section) from it, so
                // the dropdown pre-selects the actual loaded model and its own real knob labels
                // instead of always showing the generic combined "Amp/Cab" placeholder name.
                if (static_cast<EffectClass> (slot.category) == EffectClass::ampCab)
                {
                    auto sld6It = slot.params.find ("sld6");
                    if (sld6It != slot.params.end() && sld6It->second >= 0
                        && sld6It->second < (int) Rack::EffectDefinitions::ampModelOptions().size())
                    {
                        int modelId = 1000 + (int) sld6It->second;
                        auto modelDef = Rack::EffectDefinitions::lookup (modelId);
                        if (modelDef)
                        {
                            block.subLabel = juce::String (modelDef->name);
                            block.decodedEffectId = modelId;
                        }
                    }
                }

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
            // method. Whenever this IS wired for real: it should also consult
            // pendingInputSelectorValue/pendingTrueZValue (falling back to the decoded values) to
            // include Input's locally-edited settings - but that needs a Bulk Rig encoder that
            // doesn't exist yet (see the class doc comment), a separate, later piece of work.
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

void SignalChainComponent::onMainVolumeReceived (int volume)
{
    // dontSendNotification - this is us reflecting a device-confirmed value, not a user drag, so
    // it must not re-trigger onValueChange (which would just send the same value right back).
    volumeSlider.setValue (rawToDisplay (volume), juce::dontSendNotification);
}

void SignalChainComponent::onTunerStateReceived (bool isOn)
{
    tunerStatusLabel.setText (juce::String ("Tuner state (device-confirmed): ") + (isOn ? "On" : "Off"),
                               juce::dontSendNotification);
}
