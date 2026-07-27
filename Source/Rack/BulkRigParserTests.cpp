#include <JuceHeader.h>
#include "BulkRigParser.h"

// The reply below is the exact real-hardware capture recorded in
// docs/samples/bulk-rig-sample-2026-07-24.txt (CMD_GET_BULK_TFX reply, 1123 bytes). Expected
// values were cross-checked against a hand-written Python reimplementation of ElevenHack's
// TfxParser/Section/ParseUtils, which was itself verified against ElevenHack's own bulkdump.bin
// test fixture (decodes to rig name "Metal Rythm 1" with matching TOC/section byte offsets)
// before this class was ported - see docs/protocol-spec.md.

using namespace Rack;

namespace
{
    std::vector<uint8_t> hexToBytes (const juce::String& hex)
    {
        std::vector<uint8_t> bytes;
        for (auto token : juce::StringArray::fromTokens (hex, " \n\r\t", ""))
            if (token.isNotEmpty())
                bytes.push_back (static_cast<uint8_t> (token.getHexValue32()));
        return bytes;
    }

    const juce::String kSampleHex =
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

class BulkRigParserTests : public juce::UnitTest
{
public:
    BulkRigParserTests() : juce::UnitTest ("BulkRigParser", "Rack") {}

    void runTest() override
    {
        const auto sample = hexToBytes (kSampleHex);

        beginTest ("parse decodes the real Bulk Rig capture's header (rig name 'JCM 800')");
        {
            auto rig = BulkRigParser::parse (sample);
            expect (rig.has_value());
            if (rig)
            {
                expectEquals (juce::String (rig->rigName), juce::String ("JCM 800"));
                expectEquals (juce::String::toHexString (rig->version.data(), 4, 0), juce::String ("04040404"));
                expectEquals (juce::String::toHexString (rig->headerCode.data(), 4, 0), juce::String ("c3f817d2"));
            }
        }

        beginTest ("parse extracts the 16 rig-level globals, including the 'WorB'/'WstB' exceptions");
        {
            auto rig = BulkRigParser::parse (sample);
            expect (rig.has_value());
            if (rig)
            {
                expectEquals ((int) rig->rigGlobals.size(), 16);
                expectEquals (rig->rigGlobals.at ("RVol"), 2147483647);
                expectEquals (rig->rigGlobals.at ("Tmpo"), 555556);
                expectEquals (rig->rigGlobals.at ("PIGI"), 11);
                expectEquals (rig->rigGlobals.at ("WorB"), 60);
                expectEquals (rig->rigGlobals.at ("WstB"), 11);
            }
        }

        beginTest ("parse returns all 10 signal-chain slots (letters C-L) with correct effectId/category");
        {
            auto rig = BulkRigParser::parse (sample);
            expect (rig.has_value());
            if (rig)
            {
                expectEquals ((int) rig->slots.size(), 10);

                const struct { char letter; int effectId; int category; } expected[] = {
                    { 'C', 38, 2 }, { 'D', 36, 3 }, { 'E', 85, 9 }, { 'F', 91, 7 }, { 'G', 12, 0 },
                    { 'H', 17, 1 }, { 'I', 70, 4 }, { 'J', 40, 8 }, { 'K', 48, 6 }, { 'L', 53, 5 },
                };
                for (const auto& e : expected)
                {
                    const auto& slot = rig->slots[static_cast<size_t> (e.letter - 'C')];
                    expect (slot.sectionId == e.letter);
                    expectEquals (slot.effectId, e.effectId);
                    expectEquals (slot.category, e.category);
                }
            }
        }

        beginTest ("parse fills in each slot's raw parameter tag/value map");
        {
            auto rig = BulkRigParser::parse (sample);
            expect (rig.has_value());
            if (rig)
            {
                // Slot C (Volume Pedal, effectId 38): bypa/Vol /Min /Tapr.
                const auto& slotC = rig->slots[0];
                expectEquals ((int) slotC.params.size(), 4);
                expectEquals (slotC.params.at ("bypa"), 0);
                expectEquals (slotC.params.at ("Vol "), 2147483647);
                expectEquals (slotC.params.at ("Min "), static_cast<int32_t> (-2147483648LL));
                expectEquals (slotC.params.at ("Tapr"), 0);

                // Slot F (Distortion, effectId 91): bypa/Driv/Treb/Bass/Levl.
                const auto& slotF = rig->slots[3];
                expectEquals ((int) slotF.params.size(), 5);
                expectEquals (slotF.params.at ("bypa"), 1);
                expectEquals (slotF.params.at ("Driv"), -436207616);
                expectEquals (slotF.params.at ("Levl"), 1073741824);

                // Slot G (Amp/Cab, effectId 12) is the largest section - 24 params.
                expectEquals ((int) rig->slots[4].params.size(), 24);
            }
        }

        beginTest ("parse rejects a frame that isn't a Bulk Rig reply (wrong command)");
        {
            std::vector<uint8_t> notBulkRig { 0xF0, 0x13, 0x0B, 0x0F, 0x12, 0x22, 0x41, 0xF7 };
            expect (! BulkRigParser::parse (notBulkRig).has_value());
        }

        beginTest ("parse rejects a truncated Bulk Rig reply (too short to contain a full header)");
        {
            std::vector<uint8_t> truncated { 0xF0, 0x13, 0x0B, 0x0F, 0x12, 0x01, 0x00, 0x00, 0xF7 };
            expect (! BulkRigParser::parse (truncated).has_value());
        }
    }
};

static BulkRigParserTests bulkRigParserTestsInstance;
