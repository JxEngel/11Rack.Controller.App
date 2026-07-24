#include <JuceHeader.h>
#include "SysExFrame.h"

// Test cases here are drawn directly from real hardware captures recorded in
// docs/protocol-spec.md and the session transcript - not invented byte sequences. Where a value
// was cross-confirmed against the unit's own front-panel display (e.g. the current rig number),
// that's noted below.

using namespace Rack;

class SysExFrameTests : public juce::UnitTest
{
public:
    SysExFrameTests() : juce::UnitTest ("SysExFrame", "Rack") {}

    void runTest() override
    {
        beginTest ("buildQuery (no params) matches the real 'Request Effect Count' bytes sent to hardware");
        {
            auto msg = SysExFrame::buildQuery (SysExFrame::Command::countEffect);
            std::vector<uint8_t> expected { 0xF0, 0x13, 0x0B, 0x0F, 0x01, 0x22, 0xF7 };
            expect (msg == expected);
        }

        beginTest ("buildQuery (1 param) matches the real 'Request Main Volume' bytes sent to hardware");
        {
            auto msg = SysExFrame::buildQuery (SysExFrame::Command::mainVolume, 0x00);
            std::vector<uint8_t> expected { 0xF0, 0x13, 0x0B, 0x0F, 0x01, 0x36, 0x00, 0xF7 };
            expect (msg == expected);
        }

        beginTest ("buildQuery (2 params) matches the real 'Request Rig Name (Bank 0, Rig 0)' bytes");
        {
            auto msg = SysExFrame::buildQuery (SysExFrame::Command::rigGetName, 0x00, 0x00);
            std::vector<uint8_t> expected { 0xF0, 0x13, 0x0B, 0x0F, 0x01, 0x04, 0x00, 0x00, 0xF7 };
            expect (msg == expected);
        }

        beginTest ("buildSet (2 params) matches the real 'Select Rig' write that switched the unit's rig on hardware");
        {
            auto msg = SysExFrame::buildSet (SysExFrame::Command::currRigNum, 0x00, 0x06);
            std::vector<uint8_t> expected { 0xF0, 0x13, 0x0B, 0x0F, 0x00, 0x02, 0x00, 0x06, 0xF7 };
            expect (msg == expected);
        }

        beginTest ("parse decodes the real 'Request Effect Count' reply (Effect Count = 65)");
        {
            std::vector<uint8_t> reply { 0xF0, 0x13, 0x0B, 0x0F, 0x12, 0x22, 0x41, 0xF7 };
            auto parsed = SysExFrame::parse (reply);
            expect (parsed.has_value());
            if (parsed)
            {
                expect (parsed->messageType == SysExFrame::MessageType::respond);
                expect (parsed->command == static_cast<uint8_t> (SysExFrame::Command::countEffect));
                expectEquals ((int) parsed->params.size(), 1);
                expectEquals ((int) parsed->params[0], 0x41);
            }
        }

        beginTest ("parse decodes the real 'Request Current Rig Number' reply "
                   "(Bank 0, Rig 7 = 'B4' - confirmed against the unit's own front-panel display)");
        {
            std::vector<uint8_t> reply { 0xF0, 0x13, 0x0B, 0x0F, 0x12, 0x02, 0x00, 0x07, 0xF7 };
            auto parsed = SysExFrame::parse (reply);
            expect (parsed.has_value());
            if (parsed)
            {
                expectEquals ((int) parsed->params.size(), 2);
                expectEquals ((int) parsed->params[0], 0); // bank
                expectEquals ((int) parsed->params[1], 7); // rig
            }
        }

        beginTest ("parse + extractString decodes the real Rig Name reply as 'Big Blue'");
        {
            std::vector<uint8_t> reply { 0xF0, 0x13, 0x0B, 0x0F, 0x12, 0x04, 0x00, 0x00,
                                          0x42, 0x69, 0x67, 0x20, 0x42, 0x6C, 0x75, 0x65, 0x00, 0xF7 };
            auto parsed = SysExFrame::parse (reply);
            expect (parsed.has_value());
            if (parsed)
            {
                // ElevenHack reads the name via extractString(msg, 8) against the FULL message.
                // ParsedFrame::params already has the 6-byte header stripped, so the equivalent
                // offset here is 8 - 6 = 2.
                auto name = SysExFrame::extractString (parsed->params, 2);
                expectEquals (juce::String (name), juce::String ("Big Blue"));
            }
        }

        beginTest ("parse accepts the alternate model ID (0x0E) ElevenHack notes as 'seen occasionally'");
        {
            std::vector<uint8_t> reply { 0xF0, 0x13, 0x0B, 0x0E, 0x12, 0x22, 0x41, 0xF7 };
            expect (SysExFrame::parse (reply).has_value());
        }

        beginTest ("parse rejects a frame with the wrong vendor ID");
        {
            std::vector<uint8_t> badFrame { 0xF0, 0x00, 0x0B, 0x0F, 0x01, 0x22, 0xF7 };
            expect (! SysExFrame::parse (badFrame).has_value());
        }

        beginTest ("parse rejects a frame missing the trailing F7 (correct length, wrong last byte)");
        {
            // Deliberately 7 bytes (a valid length) with a wrong final byte, so this actually
            // exercises the "back() != kEnd" check rather than just the length check below.
            std::vector<uint8_t> badFrame { 0xF0, 0x13, 0x0B, 0x0F, 0x01, 0x22, 0x00 };
            expect (! SysExFrame::parse (badFrame).has_value());
        }

        beginTest ("parse rejects a message too short to contain a full header");
        {
            std::vector<uint8_t> tooShort { 0xF0, 0x13, 0x0B, 0xF7 };
            expect (! SysExFrame::parse (tooShort).has_value());
        }
    }
};

static SysExFrameTests sysExFrameTestsInstance;
