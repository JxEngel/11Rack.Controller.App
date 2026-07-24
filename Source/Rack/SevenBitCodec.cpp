#include "SevenBitCodec.h"

namespace Rack::SevenBitCodec
{
    std::vector<uint8_t> encodeValue (int8_t value)
    {
        // int8_t -> int32_t is a well-defined sign-extending conversion; reinterpreting those same
        // bits as uint32_t and shifting THAT is a well-defined logical shift - together matching
        // Java's `v>>>1` (promote byte to int via sign extension, then unsigned-shift the result)
        // exactly, for both positive and negative input.
        int32_t signExtended = value;
        uint32_t shifted = static_cast<uint32_t> (signExtended) >> 1;

        if (shifted == 0x3F)
            return { 0x3F, 0x7F, 0x7F, 0x7F, 0x0F };

        return { static_cast<uint8_t> (shifted & 0x7F), 0, 0, 0, 0 };
    }

    int8_t decodeValue (const std::vector<uint8_t>& encoded)
    {
        auto decoded = decodeFrom7Bits (encoded);
        return decoded.empty() ? 0 : static_cast<int8_t> (decoded[0]);
    }

    std::vector<uint8_t> decodeFrom7Bits (const std::vector<uint8_t>& sevenBitData)
    {
        // Direct translation of ElevenHack's SysEx.extractFrom7bits, called with begin=0,
        // end=sevenBitData.size() (the whole input vector is the window). All arithmetic is done
        // in `int` with `& 0xFF` masking at the same points the Java source does, which reproduces
        // Java's byte-truncation semantics exactly without needing a signed-byte type - checked
        // bit-for-bit against a verified Python reimplementation, including a full round-trip
        // against real hardware-captured data, before porting (see docs/protocol-spec.md).
        const int length = static_cast<int> (sevenBitData.size());
        if (length == 0)
            return {};

        std::vector<uint8_t> res (static_cast<size_t> (length), 0);

        int shift = 1;
        int i = 0;
        int b = 0; // mirrors the original algorithm's mutable `begin`

        while (b + i < length - 1)
        {
            const int term1 = ((sevenBitData[static_cast<size_t> (b + i)] & 0xFF) << shift) & 0xFF;
            const int term2 = ((sevenBitData[static_cast<size_t> (b + i + 1)] & 0xFF) >> (7 - shift)) & 0xFF;
            res[static_cast<size_t> (i)] = static_cast<uint8_t> ((term1 + term2) & 0xFF);

            ++shift;
            if (shift == 8)
            {
                shift = 1;
                ++b;
            }
            ++i;
        }

        const int last = sevenBitData[static_cast<size_t> (length - 1)];
        res[static_cast<size_t> (i)] = static_cast<uint8_t> (last & (0xFF << shift));

        res.resize (static_cast<size_t> (i + 1));
        return res;
    }

    std::vector<uint8_t> encodeTo7Bits (const std::vector<uint8_t>& eightBitData)
    {
        // Direct translation of ElevenHack's SysEx.encodeTo7bits, called with start=0,
        // end=eightBitData.size(). Uses an `int` accumulator so the two `+=` contributions to the
        // same output slot don't need intermediate 8-bit truncation - masking once at the end
        // (when narrowing to uint8_t) gives the identical result, since addition mod 256 doesn't
        // depend on when you truncate as long as you truncate before using the value further.
        const int length = static_cast<int> (eightBitData.size());
        if (length == 0)
            return {};

        const int capacity = length + static_cast<int> (length * 0.20 + 1);
        std::vector<int> res (static_cast<size_t> (capacity), 0);

        res[0] = ((eightBitData[0] & 0xFF) >> 1) & 0x7F;
        int shift = 2;
        int resPos = 1;
        int idx = 0;

        while (idx < length - 1)
        {
            res[static_cast<size_t> (resPos)] += ((eightBitData[static_cast<size_t> (idx)] & 0xFF) << (8 - shift)) & 0x7F;
            res[static_cast<size_t> (resPos)] += ((eightBitData[static_cast<size_t> (idx + 1)] & 0xFF) >> shift) & 0x7F;
            ++idx;
            ++shift;
            ++resPos;

            if (shift == 9)
            {
                shift = 2;
                res[static_cast<size_t> (resPos)] = (eightBitData[static_cast<size_t> (idx)] & 0xFF) >> 1;
                ++resPos;
            }
        }

        res[static_cast<size_t> (resPos)] += ((eightBitData[static_cast<size_t> (idx)] & 0xFF) << (8 - shift)) & 0x7F;
        ++resPos;

        std::vector<uint8_t> trimmed (static_cast<size_t> (resPos));
        for (int n = 0; n < resPos; ++n)
            trimmed[static_cast<size_t> (n)] = static_cast<uint8_t> (res[static_cast<size_t> (n)] & 0xFF);
        return trimmed;
    }
}
