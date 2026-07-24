#include "RigBrowserComponent.h"

using Rack::RackController;

RigBrowserComponent::RigBrowserComponent (Rack::RackController& controllerToUse)
    : controller (controllerToUse)
{
    controller.addListener (this);

    rigEntries.resize ((size_t) (RackController::kNumBanks * RackController::kRigsPerBank));

    addAndMakeVisible (refreshButton);
    addAndMakeVisible (statusLabel);
    addAndMakeVisible (rigListBox);

    rigListBox.setRowHeight (20);
    updateStatusLabel();

    refreshButton.onClick = [this]
    {
        for (auto& entry : rigEntries)
        {
            entry.known = false;
            entry.name = {};
        }
        namesReceivedCount = 0;
        fetching = true;
        updateStatusLabel();
        rigListBox.updateContent();
        rigListBox.repaint();

        controller.requestAllRigNames();
    };
}

RigBrowserComponent::~RigBrowserComponent()
{
    controller.removeListener (this);
}

void RigBrowserComponent::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto topRow = area.removeFromTop (30);
    refreshButton.setBounds (topRow.removeFromLeft (180).reduced (2));
    statusLabel.setBounds (topRow.reduced (2));

    area.removeFromTop (6);
    rigListBox.setBounds (area);
}

int RigBrowserComponent::getNumRows()
{
    return (int) rigEntries.size();
}

void RigBrowserComponent::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= (int) rigEntries.size())
        return;

    if (rowIsSelected)
        g.fillAll (juce::Colours::white.withAlpha (0.15f));

    int bank = rowNumber / RackController::kRigsPerBank;
    int rig = rowNumber % RackController::kRigsPerBank;
    bool isCurrent = haveCurrentRig && currentRig.bank == bank && currentRig.rig == rig;

    const auto& entry = rigEntries[(size_t) rowNumber];
    juce::String text = (isCurrent ? juce::String ("> ") : juce::String ("   "))
                         + "Bank " + juce::String (bank) + " " + rigLocationLabel (rig) + ":  "
                         + (entry.known ? entry.name : juce::String ("..."));

    g.setColour (isCurrent ? juce::Colours::yellow : juce::Colours::white);
    g.drawText (text, 6, 0, width - 12, height, juce::Justification::centredLeft);
}

void RigBrowserComponent::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= (int) rigEntries.size())
        return;

    int bank = row / RackController::kRigsPerBank;
    int rig = row % RackController::kRigsPerBank;
    controller.selectRig ({ (uint8_t) bank, (uint8_t) rig });
}

juce::String RigBrowserComponent::rigLocationLabel (int rigWithinBank)
{
    int letterIndex = rigWithinBank / 4;
    int number = (rigWithinBank % 4) + 1;
    auto letter = static_cast<juce::juce_wchar> ('A' + letterIndex);
    return juce::String::charToString (letter) + juce::String (number);
}

void RigBrowserComponent::updateStatusLabel()
{
    if (fetching)
    {
        statusLabel.setText ("Fetching rig names... " + juce::String (namesReceivedCount) + "/"
                                  + juce::String ((int) rigEntries.size()),
                              juce::dontSendNotification);
    }
    else
    {
        statusLabel.setText (namesReceivedCount > 0 ? juce::String ("Done.")
                                                     : juce::String ("Click Refresh to fetch rig names."),
                              juce::dontSendNotification);
    }
}

void RigBrowserComponent::onRigNameReceived (RackController::RigId rig, const std::string& name)
{
    int index = rig.bank * RackController::kRigsPerBank + rig.rig;
    if (index < 0 || index >= (int) rigEntries.size())
        return;

    rigEntries[(size_t) index].name = juce::String (name);
    rigEntries[(size_t) index].known = true;
    ++namesReceivedCount;

    updateStatusLabel();
    rigListBox.updateContent();
    rigListBox.repaint();
}

void RigBrowserComponent::onRigNameFetchComplete()
{
    fetching = false;
    updateStatusLabel();
}

void RigBrowserComponent::onCurrentRigReceived (RackController::RigId rig)
{
    haveCurrentRig = true;
    currentRig = rig;
    rigListBox.repaint();
}
