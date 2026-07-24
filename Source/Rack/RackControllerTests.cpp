#include <JuceHeader.h>
#include "RackController.h"

// These tests drive RackController's real message-parsing/dispatch pipeline directly (via the
// friend-class seam declared in RackController.h) using the actual byte sequences captured from
// hardware in docs/protocol-spec.md - not invented data, and not requiring a real MIDI device or
// running message loop, since we call the private handler synchronously rather than going through
// MidiTransport's real async callback path.

using namespace Rack;

namespace
{
    struct RecordingListener : public RackController::Listener
    {
        int effectCount = -1;
        int mainVolume = -1000;
        bool gotCurrentRig = false;
        RackController::RigId currentRig;
        bool gotRigName = false;
        RackController::RigId rigNameRig;
        std::string rigName;
        bool gotEffectDescription = false;
        int effectDescriptionIndex = -1;
        std::string effectStrId;
        std::string effectName;
        bool gotRigDescription = false;
        std::vector<uint8_t> rigDescriptionPayload;
        bool gotBulkRig = false;
        std::vector<uint8_t> bulkRigPayload;
        bool gotTunerState = false;
        bool tunerIsOn = false;
        int unhandledCount = 0;

        void onEffectCountReceived (int count) override { effectCount = count; }
        void onMainVolumeReceived (int volume) override { mainVolume = volume; }
        void onCurrentRigReceived (RackController::RigId rig) override { gotCurrentRig = true; currentRig = rig; }
        void onRigNameReceived (RackController::RigId rig, const std::string& name) override
        {
            gotRigName = true;
            rigNameRig = rig;
            rigName = name;
        }
        void onEffectDescriptionReceived (int effectIndex, const std::string& strId, const std::string& name) override
        {
            gotEffectDescription = true;
            effectDescriptionIndex = effectIndex;
            effectStrId = strId;
            effectName = name;
        }
        void onTunerStateReceived (bool isOn) override { gotTunerState = true; tunerIsOn = isOn; }
        void onRigDescriptionReceived (const std::vector<uint8_t>& rawPayload) override
        {
            gotRigDescription = true;
            rigDescriptionPayload = rawPayload;
        }
        void onBulkRigReceived (const std::vector<uint8_t>& decodedTfxBytes) override
        {
            gotBulkRig = true;
            bulkRigPayload = decodedTfxBytes;
        }
        void onUnhandledMessage (const std::vector<uint8_t>&) override { ++unhandledCount; }
    };
}

class RackControllerTests : public juce::UnitTest
{
public:
    RackControllerTests() : juce::UnitTest ("RackController", "Rack") {}

    void runTest() override
    {
        beginTest ("real 'Request Effect Count' reply dispatches onEffectCountReceived(65)");
        {
            RackController controller;
            RecordingListener listener;
            controller.addListener (&listener);

            controller.handleIncomingBytes ({ 0xF0, 0x13, 0x0B, 0x0F, 0x12, 0x22, 0x41, 0xF7 });

            expectEquals (listener.effectCount, 65);
        }

        beginTest ("real 'Request Main Volume' reply dispatches onMainVolumeReceived(127)");
        {
            RackController controller;
            RecordingListener listener;
            controller.addListener (&listener);

            controller.handleIncomingBytes ({ 0xF0, 0x13, 0x0B, 0x0F, 0x12, 0x36, 0x00, 0x3F, 0x7F, 0x7F, 0x7F, 0x0F, 0xF7 });

            expectEquals (listener.mainVolume, 127);
        }

        beginTest ("real 'Request Current Rig Number' reply dispatches onCurrentRigReceived(Bank 0, Rig 7 = 'B4', "
                   "confirmed against the unit's own front-panel display)");
        {
            RackController controller;
            RecordingListener listener;
            controller.addListener (&listener);

            controller.handleIncomingBytes ({ 0xF0, 0x13, 0x0B, 0x0F, 0x12, 0x02, 0x00, 0x07, 0xF7 });

            expect (listener.gotCurrentRig);
            expectEquals ((int) listener.currentRig.bank, 0);
            expectEquals ((int) listener.currentRig.rig, 7);
        }

        beginTest ("real Rig Name reply dispatches onRigNameReceived with 'Big Blue'");
        {
            RackController controller;
            RecordingListener listener;
            controller.addListener (&listener);

            controller.handleIncomingBytes ({ 0xF0, 0x13, 0x0B, 0x0F, 0x12, 0x04, 0x00, 0x00,
                                               0x42, 0x69, 0x67, 0x20, 0x42, 0x6C, 0x75, 0x65, 0x00, 0xF7 });

            expect (listener.gotRigName);
            expectEquals ((int) listener.rigNameRig.bank, 0);
            expectEquals ((int) listener.rigNameRig.rig, 0);
            expectEquals (juce::String (listener.rigName), juce::String ("Big Blue"));
        }

        beginTest ("real Effect Description (effect 0) reply dispatches onEffectDescriptionReceived "
                   "with strId 'DigiElvnELVu' and name 'Eleven'");
        {
            RackController controller;
            RecordingListener listener;
            controller.addListener (&listener);

            controller.handleIncomingBytes ({
                0xF0, 0x13, 0x0B, 0x0F, 0x12, 0x20, 0x00,
                0x44, 0x69, 0x67, 0x69, 0x45, 0x6C, 0x76, 0x6E, 0x45, 0x4C, 0x56, 0x75, 0x00, // "DigiElvnELVu"
                0x00, 0x00, 0x00, 0x01, 0x00,
                0x45, 0x6C, 0x65, 0x76, 0x65, 0x6E, 0x00, // "Eleven"
                0xF7,
            });

            expect (listener.gotEffectDescription);
            expectEquals (listener.effectDescriptionIndex, 0);
            expectEquals (juce::String (listener.effectStrId), juce::String ("DigiElvnELVu"));
            expectEquals (juce::String (listener.effectName), juce::String ("Eleven"));
        }

        beginTest ("real Rig Description reply dispatches onRigDescriptionReceived with the raw payload "
                   "(structure not yet decoded - see docs/protocol-spec.md open items)");
        {
            RackController controller;
            RecordingListener listener;
            controller.addListener (&listener);

            controller.handleIncomingBytes ({
                0xF0, 0x13, 0x0B, 0x0F, 0x12, 0x21,
                0x0B, 0x37, 0x24, 0x0B, 0x2B, 0x1A, 0x02, 0x23, 0x1B, 0x03, 0x15, 0x1C, 0x09, 0x1B,
                0x1D, 0x07, 0x00, 0x1E, 0x00, 0x32, 0x1F, 0x01, 0x08, 0x20, 0x04, 0x03, 0x21, 0x08,
                0x1F, 0x22, 0x06, 0x2A, 0x23, 0x05,
                0xF7,
            });

            expect (listener.gotRigDescription);
            expectEquals ((int) listener.rigDescriptionPayload.size(), 34);
            expectEquals ((int) listener.rigDescriptionPayload[0], 0x0B);
        }

        beginTest ("unrecognized/malformed bytes dispatch onUnhandledMessage instead of crashing");
        {
            RackController controller;
            RecordingListener listener;
            controller.addListener (&listener);

            controller.handleIncomingBytes ({ 0xF0, 0x00, 0x00, 0x00, 0x00, 0xF7 }); // malformed (also too short)

            expectEquals (listener.unhandledCount, 1);
        }

        beginTest ("a fresh RackController is not connected, and writes/queries are safe no-ops either way");
        {
            RackController controller;
            expect (! controller.isConnected());

            // None of these should crash even with nothing connected - MidiTransport::send() is a
            // safe no-op when no output is open.
            controller.requestEffectCount();
            controller.selectRig ({ 0, 7 });
            controller.setMainVolume (100);
        }

        beginTest ("connect() with invalid device identifiers fails cleanly");
        {
            RackController controller;
            bool connected = controller.connect ("not-a-real-input", "not-a-real-output");
            expect (! connected);
            expect (! controller.isConnected());
        }
    }
};

static RackControllerTests rackControllerTestsInstance;
