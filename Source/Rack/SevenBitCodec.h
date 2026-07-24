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
    //
    // `value` is SIGNED - matching ElevenHack's original `byte v` (Java bytes are signed), not
    // the `uint8_t` this was first ported as. That first attempt looked correct (127 round-tripped
    // fine against real hardware) but was actually a bug: restricting callers to non-negative
    // input meant only the upper half of the real range was ever reachable. Confirmed against
    // real hardware (2026-07-24): Main Volume raw value 0 displayed as "5.0" on the unit's own
    // screen (the *center* of its 0.0-10.0 scale, not the minimum), and raw 127 displayed as
    // "10.0" (the maximum) - i.e. the true range is signed, roughly [-127, 127], centered at 0.
    // See docs/protocol-spec.md and RigGlobalsComponent for the display-value conversion this
    // enables.
    //
    // Values 126 and 127 hit a special-cased "full-scale" encoding distinct from the general
    // linear formula used for everything else (verified: encodes to exactly 3F 7F 7F 7F 0F,
    // matching a real captured hardware reply). The special-case check and the general formula
    // both replicate Java's exact `v>>>1` semantics - sign-extend the byte to a wider int, THEN
    // apply an unsigned/logical shift to that wider pattern - not a naive 8-bit shift, which would
    // give silently wrong results for negative input (Java bytes promote-then-shift; a raw uint8_t
    // shift never sign-extends in the first place).
    std::vector<uint8_t> encodeValue (int8_t value);
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
