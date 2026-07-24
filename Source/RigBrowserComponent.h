#pragma once

#include <JuceHeader.h>
#include "Rack/RackController.h"

#include <vector>

// Lists every rig slot (both banks x 104 rigs each), fetched from the device via
// RackController::requestAllRigNames(). Highlights the currently active rig; double-click loads a
// different one via RackController::selectRig(). First real piece of the actual editor UI
// (Milestone 5) - see docs/implementation-plan.md.
class RigBrowserComponent : public juce::Component,
                             private juce::ListBoxModel,
                             private Rack::RackController::Listener
{
public:
    explicit RigBrowserComponent (Rack::RackController& controllerToUse);
    ~RigBrowserComponent() override;

    void resized() override;

    // juce::ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent& e) override;

private:
    struct RigEntry
    {
        juce::String name;
        bool known = false;
    };

    // "A1".."Z4" - matches ElevenRack.java's m_rigLocToName scheme (letterIndex = rig/4,
    // number = rig%4 + 1). Verified: rigWithinBank 7 -> "B4", matching the real hardware's own
    // front-panel display (see docs/protocol-spec.md).
    static juce::String rigLocationLabel (int rigWithinBank);

    void updateStatusLabel();

    // Rack::RackController::Listener
    void onRigNameReceived (Rack::RackController::RigId rig, const std::string& name) override;
    void onRigNameFetchComplete() override;
    void onCurrentRigReceived (Rack::RackController::RigId rig) override;

    juce::TextButton refreshButton { "Refresh Rig List" };
    juce::Label statusLabel;
    juce::ListBox rigListBox { "Rigs", this };

    std::vector<RigEntry> rigEntries;
    int namesReceivedCount = 0;
    bool fetching = false;

    Rack::RackController::RigId currentRig {};
    bool haveCurrentRig = false;

    Rack::RackController& controller;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RigBrowserComponent)
};
