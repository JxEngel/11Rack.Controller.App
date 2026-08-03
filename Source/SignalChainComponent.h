#pragma once

#include <JuceHeader.h>
#include "Rack/RackController.h"
#include "Rack/BulkRigParser.h"
#include "SlotParamsPanel.h"

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

// Visual signal-chain editor (Milestone 5) - ports docs/mockups/signal-chain-editor-concept.html
// into the real app. Shows the unit's real signal path and lets you click a block to edit it via
// the same hardware-validated SlotParamsPanel EffectEditorComponent uses.
//
// CHAIN ORDER, corrected 2026-07-27: BulkRigParser::ParsedRig::slots is already in the real
// signal-chain position order (letters C..L map directly to chain position - confirmed against a
// real rig, "SLO 100": the decoded letter order Vol/Wah/FX2/Disto/AmpCab/FXLoop/Mod/Delay/FX1/Reverb
// matched the user's independently-known real pedal order exactly, position for position). Amp and
// Cab share ONE lettered slot (they're one combined effect in EffectDefinitions) and always render
// as an adjacent pair, Amp then Cab - but that PAIR's position among the other blocks varies per rig
// just like everything else, contradicting an earlier assumption (carried over from the mockup)
// that Amp/Cab sat in a fixed spot. Only Input (always first) and Output (always last) are truly
// fixed - neither appears in the Bulk Rig payload at all, so their position can't be confirmed the
// same way, but the unit's own manual describes them as the chain's fixed endpoints.
// Before any real decode is available (not connected, or not yet fetched), the chain falls back to
// a guessed default order - see `buildDefaultChain()` in the .cpp.
//
// Chain order can now be dragged/reordered by the user (2026-07-28) - but this is a LOCAL/UI-ONLY
// reorder, purely rearranging the in-memory `chain` vector below. Whether the unit itself exposes
// writing a new order back via MIDI/SysEx at all is still an unresolved open item (see
// docs/protocol-spec.md/implementation-plan.md) - dragging here has no effect on the real unit.
// Input/Output can never be dragged and are always first/last; Amp/Cab can't be dragged themselves
// either, but their position among the other blocks can still shift as a side effect of some OTHER
// block being dragged past them - see ChainDropArea below for how the always-adjacent Amp/Cab pair
// stays un-splittable during a drop.
//
// The preset dropdown lists real rig names (fetched via RackController::requestAllRigNames(), same
// mechanism RigBrowserComponent uses) - selecting one calls selectRig() then requestBulkRig() to
// refresh the chain with that rig's real order and data. When a Bulk Rig reply arrives, each slot's
// real effect name comes from BulkRigParser + EffectDefinitions, and clicking a block pre-selects
// the real model/bypass state, and any toggle/knob values bestEffortRawTagForKey() has a raw tag
// for (see SlotConfig.h's bestEffortRawTagForKey()/knobRawToCcValue()) in the editor panel below - see
// SlotParamsPanel.h for why selectors still can't be read back this way (no confirmed encoding yet).
class SignalChainComponent : public juce::Component,
                              public juce::DragAndDropContainer,
                              private Rack::RackController::Listener
{
public:
    explicit SignalChainComponent (Rack::RackController& controllerToUse);
    ~SignalChainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    // One entry in the chain, in order. `subLabel`/`decodedEffectId`/`decodedBypass` are filled in
    // from a live Bulk Rig decode when available - blank/nullopt otherwise (including always for
    // "cab", which has no independent data - see the class doc comment).
    struct ChainBlock
    {
        juce::String id;    // e.g. "wah" - matches the mockup's chainStructure ids
        juce::String label; // e.g. "Wah"
        bool fixed = false; // Input/Output ONLY - see the class doc comment for why Amp/Cab isn't
        bool isIo = false;  // Input/Output specifically - not draggable (see Block::draggable), but
                            // still clickable/selectable like any other block (e.g. Input shows its
                            // True-Z/Input Selector editor - see `decodedTrueZValue` below)

        juce::String subLabel;
        int decodedEffectId = -1;
        std::optional<bool> decodedBypass;

        // Keyed by EffectDefinitions::ParamDefinition::key - only ever contains entries
        // SlotConfig.h's bestEffortRawTagForKey() has a raw tag for (exact-confirmed, e.g. Tape
        // Echo's "Hiss", or a plausible name-similarity match, e.g. Multi Chorus's "TriS") - see
        // docs/protocol-spec.md. Empty for effects/slots with no recorded toggle mapping yet.
        std::map<juce::String, bool> decodedToggleStates;

        // "input" only: True-Z/Input Selector's raw decoded values (rig.rigGlobals' "PIGI"/"WorB"),
        // populated by updateBlockDataFromRig(). nullopt before any decode has arrived. These drive
        // showInputEditor()'s dropdowns' initial selection - unlike every other mapped block, there's
        // no known MIDI CC for either field (see docs/master-control-map.md §5), so the dropdowns are
        // local-only (see pendingInputSelectorValue/pendingTrueZValue below), not synced live.
        std::optional<int32_t> decodedInputSelectorValue;
        std::optional<int32_t> decodedTrueZValue;

        // Same idea, for knob-kind params - value already converted to the 0-127 CC scale via
        // SlotConfig.h's knobRawToCcValue() (Q31-style raw value, hardware-confirmed for Wah's Filt,
        // assumed for every other bestEffortRawTagForKey() knob entry - see docs/protocol-spec.md
        // "Round 2"). Selector-kind params (e.g. Sync, Type) are deliberately not included here -
        // they don't follow this scale and have no confirmed encoding yet.
        std::map<juce::String, int> decodedKnobValues;
    };

    // Small custom-painted widget for one chain block - a plain juce::Button doesn't easily support
    // a two-line label+sub-label with fixed/reorderable/selected border colouring, so this paints
    // itself directly (same custom-drawing approach RigBrowserComponent::paintListBoxItem already
    // uses for its rows). Also a SettableTooltipClient so a truncated sub-label's full value is
    // still available on hover - see docs/mockups/signal-chain-editor-concept-notes.md.
    //
    // Drag source for reordering: `draggable` is false for Input/Output (never move at all) AND for
    // Amp/Cab (can't be picked up themselves, even though their position can still shift - see
    // ChainDropArea below). mouseUp only fires onClick (select) when the mouse wasn't dragged, so a
    // completed drag doesn't also re-select the source block.
    class Block : public juce::Component,
                  public juce::SettableTooltipClient
    {
    public:
        void setInfo (const ChainBlock& info, bool selected);
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;

        std::function<void()> onClick;

    private:
        juce::String label, sub, id;
        bool fixed = false, isIo = false, selected = false, draggable = false;
    };

    // Purely decorative background rectangle drawn behind the Amp+arrow+Cab trio, to communicate
    // their fixed-adjacency constraint (Amp always immediately before Cab) - see
    // docs/mockups/signal-chain-editor-concept-notes.md "Amp/Cab pairing". Not clickable itself;
    // the Amp/Cab Blocks in front of it stay independently selectable.
    class GroupBorder : public juce::Component
    {
    public:
        void paint (juce::Graphics& g) override;
    };

    // The single drop target for the whole row, replacing a plain juce::Component. `dropGaps` is
    // rebuilt by rebuildChainUi() every time the row is laid out: one entry per valid insertion
    // point (the x where the block AFTER this gap starts, in this component's own coordinates -
    // paired with the `chain` index a drop there should insert before) - EVERY arrow-gap except the
    // one between an "amp" block and its "cab" successor, which is simply never added, so that pair
    // can never be split by a drop landing between them. `onDropped` is set once by
    // SignalChainComponent to route into handleChainReorder(); `onHoverChanged` into
    // updateChainHoverPreview(), which shifts the other blocks aside to open up a real block-shaped
    // gap at the hovered drop point instead of just overlaying an indicator on top of them.
    //
    // MUST be public inheritance, not private - JUCE's internal drag-and-drop machinery finds
    // targets via dynamic_cast<DragAndDropTarget*> starting from a plain Component*, which is a
    // cross-cast that silently fails (returns nullptr) across a private base. Learned the hard way:
    // with `private` here, dragging still worked (the ghost image is driven independently by
    // DragAndDropContainer) but drops never fired at all, since itemDropped()/etc. were simply never
    // being called.
    class ChainDropArea : public juce::Component,
                           public juce::DragAndDropTarget
    {
    public:
        std::vector<std::pair<int, int>> dropGaps; // (gap x = next block's base x, chain index to insert before)
        std::function<void (const juce::String& draggedId, int insertBeforeChainIndex)> onDropped;
        std::function<void (int hoveredGapIndex)> onHoverChanged; // -1 when nothing is hovered

        // Where SignalChainComponent::updateChainHoverPreview() wants the blank placeholder drawn -
        // NOT simply dropGaps[hoveredGapIndex].first, since a forward move needs it shifted left by
        // one block+arrow width to land among the now-left-shifted blocks (see that method). -1 to
        // draw nothing. Set directly by SignalChainComponent, a plain data field rather than a
        // getter/setter pair since this class has no invariant to protect around it.
        int placeholderX = -1;

        void paintOverChildren (juce::Graphics& g) override;

    private:
        bool isInterestedInDragSource (const SourceDetails&) override;
        void itemDragEnter (const SourceDetails&) override;
        void itemDragMove (const SourceDetails&) override;
        void itemDragExit (const SourceDetails&) override;
        void itemDropped (const SourceDetails&) override;

        void updateHoveredGap (juce::Point<int> localPosition);
        void setHoveredGapIndex (int newIndex);

        int hoveredGapIndex = -1;
    };

    struct RigEntry
    {
        juce::String name;
        bool known = false;
    };

    void refreshRigList();
    void presetSelected();
    void rebuildChainUi();
    void selectBlock (int index);
    void updateBlockDataFromRig (const Rack::BulkRigParser::ParsedRig& rig);
    void updatePresetSelectorItems();
    void updateActionButtonsEnabled();
    void showRenamePopup();
    void showSaveConfirmPopup();

    // Shows inputEditorPanel populated from `block` (only ever called for the "input" block) -
    // pre-selects pending.../decoded...Value, wires each combo's onChange to update the pending
    // value. See the class doc comment and pendingInputSelectorValue/pendingTrueZValue below for why
    // this is local-only, not a live control.
    void showInputEditor (const ChainBlock& block);

    // Reorders `chain` in response to a completed drag (see ChainDropArea::itemDropped()) - a pure
    // in-memory rearrangement, no RackController call at all (this feature is local/UI-only - see
    // the class doc comment). `draggedId` is always an ordinary block's id, never Input/Output/Amp/
    // Cab (those are never draggable - see Block::draggable).
    void handleChainReorder (const juce::String& draggedId, int insertBeforeChainIndex);

    // Live preview while a drag is hovering (see ChainDropArea::onHoverChanged): shifts every
    // block/arrow/group whose base x is at or past the hovered gap to the right by one block+arrow
    // width, opening up a real gap instead of just overlaying an indicator on top of them. Pass -1
    // to restore everything to its un-shifted base position (drag exited or dropped).
    void updateChainHoverPreview (int hoveredGapIndex);

    // Current selection's rig id/name, or nullopt if only the "Select a preset..." placeholder is
    // showing - used by both popups instead of re-deriving it from presetSelector each time.
    std::optional<Rack::RackController::RigId> currentRigId() const;
    juce::String currentRigDisplayName() const; // "" if currentRigId() is nullopt

    // Best-guess order shown before any real Bulk Rig decode is available (not connected, or not
    // yet fetched) - not confirmed against any real rig, just a placeholder.
    static std::vector<ChainBlock> buildDefaultChain();

    // "A1".."Z4" - same scheme as RigBrowserComponent::rigLocationLabel (letterIndex = rig/4,
    // number = rig%4 + 1) - duplicated here rather than shared, since RigBrowserComponent's fetch
    // state isn't otherwise reused (see docs/implementation-plan.md for why this tab has its own
    // dedicated preset dropdown instead of just reusing the Rig Browser tab).
    static juce::String rigLocationLabel (int rigWithinBank);

    // Rack::RackController::Listener
    void onRigNameReceived (Rack::RackController::RigId rig, const std::string& name) override;
    void onRigNameFetchComplete() override;
    void onBulkRigReceived (const std::vector<uint8_t>& decodedTfxBytes) override;

    // juce::DragAndDropContainer - hides the source Block while it's being dragged, so it looks
    // properly "picked up" (removed from the row) instead of a dragged copy AND the still-visible
    // original both showing at once. On a successful drop, handleChainReorder() already rebuilds
    // every Block from scratch (destroying the dragged one), so draggedBlockComponent naturally goes
    // null by the time dragOperationEnded() fires - guarded via SafePointer rather than a raw
    // pointer specifically so that's safe, not a dangling-pointer access. On an invalid/cancelled
    // drop (dropped outside the chain row, or Escape), nothing was ever rebuilt, so restoring
    // visibility here IS what makes the block "snap back" to where it was.
    void dragOperationStarted (const juce::DragAndDropTarget::SourceDetails&) override;
    void dragOperationEnded (const juce::DragAndDropTarget::SourceDetails&) override;

    juce::Component::SafePointer<juce::Component> draggedBlockComponent;
    juce::String draggedBlockId; // set alongside draggedBlockComponent - used by updateChainHoverPreview()

    juce::ComboBox presetSelector;
    juce::TextButton renameButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x8e")) }; // pencil glyph
    juce::TextButton saveToUnitButton { "Save to Unit" };
    juce::TextButton refreshRigListButton { "Refresh Rig List" };
    juce::Label rigStatusLabel;

    juce::Label chainLabel { {}, "Signal chain" };
    juce::Viewport chainViewport;
    ChainDropArea chainContent;
    std::vector<ChainBlock> chain;
    std::vector<std::unique_ptr<Block>> blockComponents;
    std::vector<std::unique_ptr<juce::Label>> arrowLabels;
    std::vector<std::unique_ptr<GroupBorder>> groupBorders;

    // Each component's un-shifted x, as laid out by rebuildChainUi() - parallel to
    // blockComponents/arrowLabels/groupBorders. updateChainHoverPreview() temporarily offsets a
    // component's actual x away from this during a drag hover; these are what it offsets FROM, and
    // what everything snaps back to once the hover ends.
    std::vector<int> blockBaseX, arrowBaseX, groupBaseX;
    std::vector<int> groupChainIndex; // the Amp block's chain index, one entry per groupBorders - lets
                                       // updateChainHoverPreview() decide a group's shift the same way
                                       // it decides a block's, by chain-index range membership
    int chainContentBaseWidth = 0; // chainContent's width with no hover shift applied

    int selectedIndex = -1;

    SlotParamsPanel paramsPanel;
    juce::Label noSlotLabel;

    // The "input" block's editor - a third mutually-exclusive view alongside paramsPanel/noSlotLabel
    // in selectBlock()/resized(), since Input needs interactive controls but doesn't fit
    // SlotParamsPanel's CC-send-oriented design (see showInputEditor() and the class doc comment).
    juce::Component inputEditorPanel;
    juce::Label inputSelectorLabel { {}, "Input Selector" };
    juce::ComboBox inputSelectorCombo;
    juce::Label trueZLabel { {}, "True-Z" };
    juce::ComboBox trueZCombo;
    juce::Label inputEditorNoteLabel;

    // The user's in-UI edit for Input's dropdowns (raw Rig Params values, e.g. PIGI's `1` for "1
    // MOhm + Cap") - nullopt means "no local edit yet, show the decoded value." Reset to nullopt in
    // updateBlockDataFromRig() on every fresh decode (a pending edit belongs to whichever rig was
    // loaded when it was made, not whatever rig gets loaded next). NOT sent anywhere - no known MIDI
    // CC exists for either field (see docs/master-control-map.md §5), and reaching the unit on a real
    // "Save to Unit" write would need a Bulk Rig encoder that doesn't exist yet (see the class doc
    // comment) - a separate, later decision, not attempted here.
    std::optional<int32_t> pendingInputSelectorValue;
    std::optional<int32_t> pendingTrueZValue;

    std::vector<RigEntry> rigEntries;
    int namesReceivedCount = 0;
    bool fetchingRigNames = false;

    Rack::RackController& controller;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SignalChainComponent)
};
