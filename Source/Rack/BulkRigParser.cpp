#include "BulkRigParser.h"
#include "SevenBitCodec.h"
#include "SysExFrame.h"

#include <algorithm>

namespace Rack::BulkRigParser
{
    namespace
    {
        // ElevenHack's "quadlet" tag/value encoding (ParseUtils.quadToKey/quadToInt): every 4-byte
        // group is a little-endian value EXCEPT 4-character tags, which are stored byte-reversed on
        // the wire (buf[3],buf[2],buf[1],buf[0]) relative to their natural reading order.
        std::string quadToKey (const std::array<uint8_t, 4>& buf)
        {
            return { static_cast<char> (buf[3]), static_cast<char> (buf[2]),
                     static_cast<char> (buf[1]), static_cast<char> (buf[0]) };
        }

        int32_t quadToInt (const std::array<uint8_t, 4>& buf)
        {
            // Matches Java's `a + (b<<8) + (c<<16) + (d<<24)` int semantics, which can overflow into
            // negative values by design (e.g. "Driv" = -1073741824) - assemble as uint32_t first (a
            // well-defined bitwise concatenation), then reinterpret as the equivalent two's-complement
            // int32_t.
            const uint32_t assembled = static_cast<uint32_t> (buf[0]) | (static_cast<uint32_t> (buf[1]) << 8)
                                        | (static_cast<uint32_t> (buf[2]) << 16) | (static_cast<uint32_t> (buf[3]) << 24);
            return static_cast<int32_t> (assembled);
        }

        // A single tagged-record block (ElevenHack's Section.java): a 4-byte header (byteSize as a
        // little-endian uint16 in bytes[0:2], sectionId as the char at byte[3]) followed by
        // byteSize/8 key/value quadlet pairs. Used for both the TOC ('A') and each per-effect
        // section ('C'-'L').
        struct Section
        {
            int byteSize = 0;
            char sectionId = 0;
            std::map<std::string, int32_t> entries;
            size_t endPos = 0; // position in `data` immediately after this section's last pair
        };

        std::optional<Section> parseSection (const std::vector<uint8_t>& data, size_t pos)
        {
            if (pos + 4 > data.size())
                return std::nullopt;

            const int byteSize = data[pos] + data[pos + 1] * 256;
            if (byteSize % 8 != 0)
                return std::nullopt;

            Section section;
            section.byteSize = byteSize;
            section.sectionId = static_cast<char> (data[pos + 3]);

            size_t p = pos + 4;
            const int nbPairs = byteSize / 8;
            for (int i = 0; i < nbPairs; ++i)
            {
                if (p + 8 > data.size())
                    return std::nullopt;

                const std::array<uint8_t, 4> keyBuf { data[p], data[p + 1], data[p + 2], data[p + 3] };
                const std::array<uint8_t, 4> valBuf { data[p + 4], data[p + 5], data[p + 6], data[p + 7] };
                section.entries[quadToKey (keyBuf)] = quadToInt (valBuf);
                p += 8;
            }

            section.endPos = p;
            return section;
        }

        // TOC ('A' section) + the sequential per-effect sections that follow it (TfxParser.parseBody).
        std::optional<ParsedRig> parseBody (const std::vector<uint8_t>& data, size_t headerEnd)
        {
            auto toc = parseSection (data, headerEnd);
            if (! toc || toc->sectionId != 'A')
                return std::nullopt;

            ParsedRig rig;
            size_t pos = toc->endPos;

            // Rig-level globals are every TOC entry that isn't a per-slot "Wor<letter>"/"Wst<letter>"
            // lookup - except "WorB"/"WstB", which despite the name shape are themselves rig globals
            // (there is no slot letter 'B'; the real per-effect slots run 'C' through 'L').
            for (const auto& [key, value] : toc->entries)
            {
                const bool looksLikeSlotKey = key.size() == 4 && key[0] == 'W';
                if (! looksLikeSlotKey || key == "WorB" || key == "WstB")
                    rig.rigGlobals[key] = value;
            }

            rig.slots.reserve (10);
            for (char letter = 'C'; letter <= 'L'; ++letter)
            {
                EffectSlot slot;
                slot.sectionId = letter;

                const auto effectIdIt = toc->entries.find (std::string ("Wor") + letter);
                const auto categoryIt = toc->entries.find (std::string ("Wst") + letter);
                slot.effectId = effectIdIt != toc->entries.end() ? effectIdIt->second : 0;
                slot.category = categoryIt != toc->entries.end() ? categoryIt->second : 0;

                rig.slots.push_back (std::move (slot));
            }

            // Sections are read back-to-back with no padding, count field, or end marker - the only
            // termination signal is too little data remaining for another section header + at least
            // one pair (12 bytes).
            while (pos < data.size() && (data.size() - pos) >= 12)
            {
                auto section = parseSection (data, pos);
                if (! section)
                    break;
                pos = section->endPos;

                const int idx = section->sectionId - 'C';
                if (idx >= 0 && idx < static_cast<int> (rig.slots.size()))
                    rig.slots[static_cast<size_t> (idx)].params = std::move (section->entries);
            }

            return rig;
        }
    }

    std::optional<ParsedRig> parse (const std::vector<uint8_t>& sysExMessage)
    {
        auto frame = SysExFrame::parse (sysExMessage);
        if (! frame || frame->messageType != SysExFrame::MessageType::respond
            || frame->command != static_cast<uint8_t> (SysExFrame::Command::getBulkTfx))
            return std::nullopt;

        // ElevenHack's own decode call (SysExTest.testBulkDump) is
        // Arrays.copyOfRange(msg, bulkoffset=6, msg.length) - this INCLUDES the trailing F7
        // terminator in the 7-bit decode input, so match that exactly rather than excluding it (see
        // SevenBitCodec.h for why decodeFrom7Bits' output has one trailing not-fully-meaningful byte
        // regardless).
        const std::vector<uint8_t> payload (sysExMessage.begin() + 6, sysExMessage.end());
        return parseDecoded (SevenBitCodec::decodeFrom7Bits (payload));
    }

    std::optional<ParsedRig> parseDecoded (const std::vector<uint8_t>& decoded)
    {
        // Header is 8 bytes (version + headerCode) + 7 name quadlets (28 bytes) = 36 bytes.
        if (decoded.size() < 36)
            return std::nullopt;

        std::array<uint8_t, 4> version {};
        std::array<uint8_t, 4> headerCode {};
        std::copy_n (decoded.begin(), 4, version.begin());
        std::copy_n (decoded.begin() + 4, 4, headerCode.begin());

        std::string name;
        size_t pos = 8;
        for (int i = 0; i < 7; ++i)
        {
            const std::array<uint8_t, 4> quad { decoded[pos], decoded[pos + 1], decoded[pos + 2], decoded[pos + 3] };
            name += quadToKey (quad);
            pos += 4;
        }
        const auto nul = name.find ('\0');
        if (nul != std::string::npos)
            name.resize (nul);

        auto rig = parseBody (decoded, pos);
        if (! rig)
            return std::nullopt;

        rig->version = version;
        rig->headerCode = headerCode;
        rig->rigName = name;
        return rig;
    }
}
