#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Rack::BulkRigParser
{
    // Decodes a CMD_GET_BULK_TFX reply (the "Bulk Rig" SysEx dump) into its header, rig-level
    // globals, and per-signal-chain-slot effect ID/category/parameter data.
    //
    // Byte format reverse-engineered from ElevenHack's TfxParser.java/Section.java/ParseUtils.java
    // (https://gitlab.com/schmidg/elevenhack, Apache-2.0, Guillaume Schmid) - verified byte-for-byte
    // against ElevenHack's own bulkdump.bin test fixture (decodes to rig name "Metal Rythm 1" with
    // TOC/section boundaries matching exactly) and our own real hardware capture
    // (docs/samples/bulk-rig-sample-2026-07-24.txt) before porting; see docs/protocol-spec.md and
    // docs/implementation-plan.md.

    // One of the 10 reorderable/fixed signal-chain positions the TOC lists as slot letters 'C'-'L'.
    // `effectId`/`category` are ElevenHack's own internal numbering - NOT yet reconciled against
    // Rack::EffectDefinitions's `effectId`/`EffectClass` values, and `params` keys are the raw
    // 4-character on-the-wire field tags (e.g. "Driv", "bypa") rather than
    // Rack::EffectDefinitions::ParamDefinition::key - that reconciliation is future work (see
    // docs/implementation-plan.md).
    struct EffectSlot
    {
        char sectionId = 0;   // 'C' through 'L'
        int effectId = 0;     // TOC's "Wor<letter>" entry
        int category = 0;     // TOC's "Wst<letter>" entry
        std::map<std::string, int32_t> params; // raw field tag -> signed 32-bit value
    };

    struct ParsedRig
    {
        std::array<uint8_t, 4> version {};
        std::array<uint8_t, 4> headerCode {};
        std::string rigName;
        std::map<std::string, int32_t> rigGlobals; // TOC entries not tied to a specific slot
        std::vector<EffectSlot> slots; // always 10 entries, in letter order C..L
    };

    // `sysExMessage` is a full F0...F7 CMD_GET_BULK_TFX reply frame (as received from
    // MidiTransport). Returns std::nullopt if the frame doesn't parse as a well-formed SysEx frame,
    // isn't a Bulk Rig reply, or the decoded payload's structure doesn't match expectations (bad TOC
    // block size, wrong TOC section ID, truncated data).
    std::optional<ParsedRig> parse (const std::vector<uint8_t>& sysExMessage);

    // Same as `parse()`, but starting from bytes already run through
    // `SevenBitCodec::decodeFrom7Bits` - e.g. `RackController::Listener::onBulkRigReceived`'s
    // `decodedTfxBytes`, for callers that only have the decoded body, not the original raw frame.
    std::optional<ParsedRig> parseDecoded (const std::vector<uint8_t>& decodedTfxBytes);
}
