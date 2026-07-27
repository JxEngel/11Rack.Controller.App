#include <JuceHeader.h>
#include "RackController.h"
#include "BulkRigParser.h"

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

        int rigNameFetchCompleteCount = 0;
        void onRigNameFetchComplete() override { ++rigNameFetchCompleteCount; }
    };

    // Builds a synthetic (but structurally real) Rig Name reply frame for a given bank/rig/name -
    // same layout as the real captured "Big Blue" reply, just parameterized.
    std::vector<uint8_t> makeRigNameReply (uint8_t bank, uint8_t rig, const std::string& name)
    {
        std::vector<uint8_t> msg { 0xF0, 0x13, 0x0B, 0x0F, 0x12, 0x04, bank, rig };
        for (char c : name)
            msg.push_back (static_cast<uint8_t> (c));
        msg.push_back (0);
        msg.push_back (0xF7);
        return msg;
    }

    // The real Bulk Rig capture from docs/samples/bulk-rig-sample-2026-07-24.txt (also used in
    // BulkRigParserTests.cpp) - decodes to 978 bytes, rig name "JCM 800", when the trailing F7 is
    // correctly included in the 7-bit decode input.
    std::vector<uint8_t> hexToBytes (const juce::String& hex)
    {
        std::vector<uint8_t> bytes;
        for (auto token : juce::StringArray::fromTokens (hex, " \n\r\t", ""))
            if (token.isNotEmpty())
                bytes.push_back (static_cast<uint8_t> (token.getHexValue32()));
        return bytes;
    }

    const juce::String kBulkRigSampleHex =
        "F0 13 0B 0F 12 01 02 01 00 40 26 0F 70 17 69 08 09 54 1A 28 00 30 18 0E 00 00 00 00 00 00 00 "
        "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 0A 00 12 02 05 58 6F 2B 14 5F 7F 7F 7D 7E 31 "
        "36 1B 6A 67 56 4E 36 5E 19 1B 0D 75 33 6B 27 1B 2F 1B 6D 64 6A 48 00 00 00 00 0D 77 03 35 28 "
        "24 3D 02 00 04 63 31 66 52 00 00 00 13 7A 25 0E 49 28 02 60 00 00 01 46 79 39 53 20 00 00 00 "
        "00 54 38 1E 08 50 20 00 00 00 18 58 6B 04 33 7C 00 00 32 0C 4C 35 42 19 7E 00 00 19 06 36 1A "
        "61 0C 7F 00 00 0C 43 23 0D 30 46 3F 40 00 06 22 4D 18 6F 2B 42 60 00 00 01 04 72 37 55 67 40 "
        "00 00 00 42 3A 1C 6A 70 58 00 00 00 21 5C 4D 75 39 18 00 00 00 10 6E 47 1A 5C 04 00 00 00 08 "
        "47 13 3D 2E 24 00 00 00 04 23 51 66 57 01 40 00 00 02 15 64 6F 2B 55 20 00 00 01 0A 74 39 55 "
        "61 10 00 00 00 46 39 1B 6A 75 58 00 00 00 23 1D 0E 35 38 1C 00 00 00 11 6E 26 7A 5C 18 00 00 "
        "00 08 77 23 4D 2E 00 00 00 00 04 43 49 5E 57 08 40 00 00 02 21 68 73 2B 40 20 00 00 01 12 72 "
        "37 55 68 60 00 00 00 49 3A 1C 6A 70 20 00 00 00 25 1C 4D 75 39 20 00 00 00 12 4E 47 1A 5C 10 "
        "00 00 00 09 37 13 3D 2E 30 00 00 00 04 5B 51 66 57 03 00 00 00 02 31 64 6F 2B 4D 20 00 00 01 "
        "18 74 39 55 60 50 00 00 00 46 29 5B 08 70 00 00 00 00 10 00 04 04 1B 05 60 79 31 00 00 00 00 "
        "00 40 6C 37 55 5F 7F 7F 7D 7E 20 37 1A 29 50 00 00 01 00 39 1C 0C 15 20 00 00 00 00 06 00 02 "
        "02 11 42 70 3C 58 40 10 00 00 00 74 36 1A 28 6F 7F 7F 7E 7F 39 10 6F 05 30 04 00 00 00 0E 00 "
        "02 02 15 42 70 3C 58 40 00 00 00 00 43 32 1B 0E 33 19 4C 66 33 18 59 0D 47 1C 7B 5D 09 15 4C "
        "6C 46 63 4F 08 7C 17 3A 46 26 23 31 66 2C 27 44 57 54 3B 11 58 73 33 19 4C 6E 32 11 48 6C 39 "
        "40 00 00 04 00 50 00 10 11 4C 17 03 65 44 01 00 00 00 07 33 25 64 44 00 00 00 0E 33 09 4A 72 "
        "2A 00 00 00 01 21 66 73 30 50 40 00 00 03 18 6C 3B 19 29 40 00 00 00 40 60 00 04 04 39 45 48 "
        "6C 39 7F 7F 7F 7F 7C 64 64 36 1C 60 00 00 00 60 33 32 1B 0E 30 00 00 01 2A 1A 19 0D 47 1C 5E "
        "44 2F 17 4D 2C 46 63 4C 00 00 00 00 06 76 23 31 67 7F 7F 7F 6F 73 43 11 58 73 00 03 67 42 59 "
        "65 48 6C 39 7F 7F 7F 7B 7D 02 64 36 1C 60 00 00 00 44 42 32 1B 0E 30 00 00 01 3C 21 59 0D 47 "
        "18 00 00 00 11 11 0C 46 63 4C 00 00 00 0A 08 56 23 31 66 01 00 00 00 03 33 11 58 73 03 40 00 "
        "00 02 19 48 6C 39 40 00 00 04 01 0E 64 36 1C 7F 7A 1E 14 60 63 37 1E 2A 30 00 00 00 00 37 13 "
        "6E 25 20 04 00 00 00 12 4C 46 63 4C 00 00 00 00 09 36 23 31 66 05 00 00 00 04 63 11 58 73 00 "
        "40 00 00 02 35 48 6C 39 40 00 00 00 01 1C 64 36 1C 60 00 00 00 4C 4F 32 1B 0E 30 00 00 00 00 "
        "10 00 04 04 43 05 60 79 31 00 20 00 00 01 48 6E 32 5C 71 6B 41 0D 5C 6E 39 1D 0E 25 2A 55 2B "
        "55 38 1D 0C 57 38 00 00 00 40 0C 00 02 02 25 42 70 3C 58 40 10 00 00 00 64 32 5C 0A 30 00 00 "
        "01 36 34 1D 0E 04 20 00 00 00 11 1A 6C 26 22 1B 7F 7F 7F 4F 6F 16 62 11 20 00 00 00 14 06 1B "
        "39 72 53 00 00 00 00 01 40 00 20 25 18 2E 07 4B 08 02 00 00 00 0D 64 4B 21 06 00 00 00 00 07 "
        "02 11 44 56 00 00 00 01 33 51 24 62 2B 3F 7F 7F 79 7D 4A 64 37 53 20 00 00 00 00 63 37 1E 2A "
        "30 00 00 00 00 28 00 04 04 5B 05 60 79 31 00 00 00 00 01 48 6E 36 10 40 00 00 03 04 6B 31 19 "
        "08 60 00 00 00 2A 10 1E 2D 44 20 14 00 00 79 18 6D 67 4A 4C 00 00 00 00 0E 64 63 39 12 00 00 "
        "00 00 06 43 51 60 44 00 00 00 08 03 09 2C 68 21 40 00 00 00 00 40 20 2C 0D 00 00 00 00 00 73 "
        "34 5B 69 60 00 00 00 00 18 00 04 04 63 05 60 79 31 00 00 00 00 01 72 61 31 51 00 00 00 00 00 "
        "65 37 1B 6A 40 00 00 00 00 3C 1A 29 55 10 00 00 00 74 19 2E 07 4A 50 0C 00 00 00 0F 16 62 11 "
        "20 00 00 00 10 00 F7";
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

        beginTest ("real Bulk Rig reply decodes to the full 978 bytes (INCLUDING the trailing F7 in "
                   "the 7-bit decode input) and BulkRigParser::parseDecoded can read it as rig 'JCM 800'");
        {
            // Regression test for a real bug: this handler used to 7-bit-decode `frame.params`
            // (which SysExFrame::parse has already trimmed the trailing F7 off of), producing one
            // byte fewer than ElevenHack's own Arrays.copyOfRange(msg, 6, msg.length) - confirmed
            // via a byte-by-byte diff that this isn't just a harmless trailing-byte difference, it
            // shifts other decoded bytes too. See Source/Rack/BulkRigParser.h/.cpp.
            RackController controller;
            RecordingListener listener;
            controller.addListener (&listener);

            controller.handleIncomingBytes (hexToBytes (kBulkRigSampleHex));

            expect (listener.gotBulkRig);
            expectEquals ((int) listener.bulkRigPayload.size(), 978);

            auto rig = BulkRigParser::parseDecoded (listener.bulkRigPayload);
            expect (rig.has_value());
            if (rig)
            {
                expectEquals (juce::String (rig->rigName), juce::String ("JCM 800"));
                expectEquals ((int) rig->slots.size(), 10);
            }
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

        // --- requestAllRigNames() sequencing (uses friend access to inspect/set private state
        // directly, rather than simulating all 208 steps one at a time) ---

        beginTest ("requestAllRigNames() starts at Bank 0, Rig 0");
        {
            RackController controller;
            controller.requestAllRigNames();

            expect (controller.fetchingAllRigNames);
            expect (controller.nextRigNameFetchTarget == (RackController::RigId { 0, 0 }));
        }

        beginTest ("each matching reply advances to the next rig in sequence");
        {
            RackController controller;
            controller.requestAllRigNames();

            controller.handleIncomingBytes (makeRigNameReply (0, 0, "Big Blue"));
            expect (controller.nextRigNameFetchTarget == (RackController::RigId { 0, 1 }));

            controller.handleIncomingBytes (makeRigNameReply (0, 1, "Some Rig"));
            expect (controller.nextRigNameFetchTarget == (RackController::RigId { 0, 2 }));
        }

        beginTest ("a reply that doesn't match the expected next rig is ignored for sequencing");
        {
            RackController controller;
            controller.requestAllRigNames();
            controller.handleIncomingBytes (makeRigNameReply (0, 0, "Big Blue"));
            expect (controller.nextRigNameFetchTarget == (RackController::RigId { 0, 1 }));

            // Simulates e.g. a manual "Request Rig Name" from elsewhere arriving mid-fetch.
            controller.handleIncomingBytes (makeRigNameReply (1, 50, "Unrelated"));
            expect (controller.nextRigNameFetchTarget == (RackController::RigId { 0, 1 }));
        }

        beginTest ("reaching the last rig of bank 0 wraps to bank 1, rig 0");
        {
            RackController controller;
            controller.fetchingAllRigNames = true;
            controller.nextRigNameFetchTarget = { 0, RackController::kRigsPerBank - 1 };

            controller.handleIncomingBytes (makeRigNameReply (0, RackController::kRigsPerBank - 1, "Last In Bank 0"));

            expect (controller.fetchingAllRigNames); // not done yet - bank 1 still to go
            expect (controller.nextRigNameFetchTarget == (RackController::RigId { 1, 0 }));
        }

        beginTest ("reaching the last rig of bank 1 completes the fetch and notifies listeners");
        {
            RackController controller;
            RecordingListener listener;
            controller.addListener (&listener);

            controller.fetchingAllRigNames = true;
            controller.nextRigNameFetchTarget = { 1, RackController::kRigsPerBank - 1 };

            controller.handleIncomingBytes (makeRigNameReply (1, RackController::kRigsPerBank - 1, "Last Rig Overall"));

            expect (! controller.fetchingAllRigNames);
            expectEquals (listener.rigNameFetchCompleteCount, 1);
        }

        beginTest ("cancelRigNameFetch() stops the sequence");
        {
            RackController controller;
            controller.requestAllRigNames();
            expect (controller.fetchingAllRigNames);

            controller.cancelRigNameFetch();
            expect (! controller.fetchingAllRigNames);

            // A reply arriving after cancellation shouldn't restart/advance anything.
            controller.handleIncomingBytes (makeRigNameReply (0, 0, "Big Blue"));
            expect (! controller.fetchingAllRigNames);
        }
    }
};

static RackControllerTests rackControllerTestsInstance;
