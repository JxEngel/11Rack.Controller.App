#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Rack::SysExFrame
{
    // Wire-level constants and helpers for the Eleven Rack's SysEx protocol frame:
    // F0 <vendor> <device> <model> <msg-type> <command> [params...] F7
    //
    // Reverse-engineered origin: ElevenHack (https://gitlab.com/schmidg/elevenhack, Apache-2.0,
    // Guillaume Schmid) - not official AVID documentation. Cross-validated against real hardware;
    // see docs/protocol-spec.md for the capture log these constants and tests are drawn from.

    constexpr uint8_t kStart = 0xF0;
    constexpr uint8_t kEnd = 0xF7;
    constexpr uint8_t kVendorId = 0x13;
    constexpr uint8_t kDeviceId = 0x0B;
    constexpr uint8_t kModelId = 0x0F;
    constexpr uint8_t kModelIdAlt = 0x0E; // ElevenHack notes this appears "sometimes" - not yet
                                           // observed on real hardware ourselves.

    enum class MessageType : uint8_t
    {
        sndSet = 0x00,   // write a value
        requ = 0x01,     // request/query a value
        asyncSet = 0x02, // unit reporting an unsolicited change (e.g. front-panel action)
        respond = 0x12,  // reply to a request
    };

    enum class Command : uint8_t
    {
        setBulkTfx = 0x00,
        getBulkTfx = 0x01,
        currRigNum = 0x02,
        saveRig = 0x03,
        rigGetName = 0x04,
        rigSetName = 0x05,
        descEffect = 0x20,
        rigDesc = 0x21,
        countEffect = 0x22,
        mainVolume = 0x36,
        tuner = 0x40,
        tunerA = 0x41,
    };

    // Build a query (REQU) frame with 0, 1, or 2 extra parameter bytes.
    std::vector<uint8_t> buildQuery (Command command);
    std::vector<uint8_t> buildQuery (Command command, uint8_t param1);
    std::vector<uint8_t> buildQuery (Command command, uint8_t param1, uint8_t param2);

    // Build a set (SNDSET) frame with 1 or 2 parameter bytes.
    std::vector<uint8_t> buildSet (Command command, uint8_t param1);
    std::vector<uint8_t> buildSet (Command command, uint8_t param1, uint8_t param2);

    struct ParsedFrame
    {
        MessageType messageType {};
        uint8_t command = 0;
        std::vector<uint8_t> params; // everything between the command byte and the trailing F7
    };

    // Parses a full F0...F7 Eleven Rack SysEx frame. Returns std::nullopt if the header doesn't
    // match (wrong start/end byte, wrong vendor/device ID, unrecognized model ID or message type,
    // or the message is too short to contain a full header).
    std::optional<ParsedFrame> parse (const std::vector<uint8_t>& message);

    // Reads a null-terminated ASCII string starting at `offset` within `buffer` - mirrors
    // ElevenHack's SysEx.extractString. `offset` is relative to whatever buffer is passed in (e.g.
    // a ParsedFrame's `params`, not the original full message - callers need to account for the
    // 6-byte header ParsedFrame::params has already had stripped off).
    std::string extractString (const std::vector<uint8_t>& buffer, size_t offset);
}
