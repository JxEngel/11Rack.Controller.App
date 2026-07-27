#pragma once

#include <JuceHeader.h>
#include "Rack/RackController.h"
#include "Rack/BulkRigParser.h"
#include "SlotParamsPanel.h"

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
// Chain order itself still isn't user-editable in this UI (see docs/protocol-spec.md Open Items on
// whether the unit even supports writing a new order back) - it now reflects the REAL per-rig order
// once decoded, it's just not draggable/reorderable by the user yet.
//
// The preset dropdown lists real rig names (fetched via RackController::requestAllRigNames(), same
// mechanism RigBrowserComponent uses) - selecting one calls selectRig() then requestBulkRig() to
// refresh the chain with that rig's real order and data. When a Bulk Rig reply arrives, each slot's
// real effect name comes from BulkRigParser + EffectDefinitions, and clicking a block pre-selects
// the real model/bypass state in the editor panel below - see SlotParamsPanel.h for why per-knob
// values still can't be read back this way, only which model is loaded and its bypass state.
class SignalChainComponent : public juce::Component,
                              private Rack::RackController::Listener
{
public:
    explicit SignalChainComponent (Rack::RackController& controllerToUse);
    ~SignalChainComponent() override;

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
        bool isIo = false;  // Input/Output specifically - not clickable at all

        juce::String subLabel;
        int decodedEffectId = -1;
        std::optional<bool> decodedBypass;
    };

    // Small custom-painted widget for one chain block - a plain juce::Button doesn't easily support
    // a two-line label+sub-label with fixed/reorderable/selected border colouring, so this paints
    // itself directly (same custom-drawing approach RigBrowserComponent::paintListBoxItem already
    // uses for its rows).
    class Block : public juce::Component
    {
    public:
        void setInfo (const ChainBlock& info, bool selected);
        void paint (juce::Graphics& g) override;
        void mouseUp (const juce::MouseEvent&) override;

        std::function<void()> onClick;

    private:
        juce::String label, sub;
        bool fixed = false, isIo = false, selected = false;
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

    juce::Label presetLabel { {}, "Preset" };
    juce::ComboBox presetSelector;
    juce::TextButton refreshRigListButton { "Refresh Rig List" };
    juce::Label rigStatusLabel;

    juce::Label chainLabel { {}, "Signal chain" };
    juce::Viewport chainViewport;
    juce::Component chainContent;
    std::vector<ChainBlock> chain;
    std::vector<std::unique_ptr<Block>> blockComponents;
    std::vector<std::unique_ptr<juce::Label>> arrowLabels;
    int selectedIndex = -1;

    SlotParamsPanel paramsPanel;
    juce::Label noSlotLabel;

    std::vector<RigEntry> rigEntries;
    int namesReceivedCount = 0;
    bool fetchingRigNames = false;

    Rack::RackController& controller;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SignalChainComponent)
};
