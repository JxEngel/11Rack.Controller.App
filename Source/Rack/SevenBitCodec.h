#pragma once

#include <cstdint>
#include <vector>

namespace Rack::SevenBitCodec
{
    // Numeric encoding schemes used inside Eleven Rack SysEx payloads, where every data byte must
    // stay in the 7-bit-safe MIDI range [0, 0x7F]. Ported from ElevenHack's ParseUtils/SysEx
    // (https://gitlab.com/schmidg/elevenhack, Apache-2.0) - see docs/protocol-spec.md for the
    // hardware-verified test cases these were checked against before porting.

    // The 5-byte "encoded int" scheme used for single continuous values (e.g. Main Volume).
    // `value` is expected in the normal MIDI byte range [0, 127]. Values 126 and 127 hit a
    // special-cased "full-scale" encoding distinct from the general linear formula used for
    // everything else - verified against real hardware (Main Volume = 127 encodes to exactly
    // 3F 7F 7F 7F 0F, and the unit's own reply for that value decodes back to 127).
    std::vector<uint8_t> encodeValue (uint8_t value);
    int8_t decodeValue (const std::vector<uint8_t>& encoded);

    // General 7-bit-safe pack/unpack for arbitrary-length payloads (e.g. bulk rig transfers).
    //
    // IMPORTANT: decodeFrom7Bits always returns exactly one MORE byte than the true underlying
    // data length - the last byte is a deterministic but not-fully-meaningful "remainder" left
    // over from packing 8 bits into 7-bit groups (there's no way to know from the packed data
    // alone exactly where the real data ends; something else - e.g. the .tfx file's own internal
    // structure - has to know the true length and ignore the trailing byte). Verified: decoding
    // the real captured bulk-rig reply, dropping its last byte, and re-encoding reproduces the
    // original captured wire bytes exactly - see SevenBitCodecTests.cpp.
    std::vector<uint8_t> encodeTo7Bits (const std::vector<uint8_t>& eightBitData);
    std::vector<uint8_t> decodeFrom7Bits (const std::vector<uint8_t>& sevenBitData);
}
