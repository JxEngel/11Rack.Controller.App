#include "SysExFrame.h"

namespace Rack::SysExFrame
{
    namespace
    {
        std::vector<uint8_t> header (MessageType type)
        {
            return { kStart, kVendorId, kDeviceId, kModelId, static_cast<uint8_t> (type) };
        }

        bool isKnownMessageType (uint8_t rawType)
        {
            switch (static_cast<MessageType> (rawType))
            {
                case MessageType::sndSet:
                case MessageType::requ:
                case MessageType::asyncSet:
                case MessageType::respond:
                    return true;
            }
            return false;
        }
    }

    std::vector<uint8_t> buildQuery (Command command)
    {
        auto msg = header (MessageType::requ);
        msg.push_back (static_cast<uint8_t> (command));
        msg.push_back (kEnd);
        return msg;
    }

    std::vector<uint8_t> buildQuery (Command command, uint8_t param1)
    {
        auto msg = header (MessageType::requ);
        msg.push_back (static_cast<uint8_t> (command));
        msg.push_back (param1);
        msg.push_back (kEnd);
        return msg;
    }

    std::vector<uint8_t> buildQuery (Command command, uint8_t param1, uint8_t param2)
    {
        auto msg = header (MessageType::requ);
        msg.push_back (static_cast<uint8_t> (command));
        msg.push_back (param1);
        msg.push_back (param2);
        msg.push_back (kEnd);
        return msg;
    }

    std::vector<uint8_t> buildSet (Command command, uint8_t param1)
    {
        auto msg = header (MessageType::sndSet);
        msg.push_back (static_cast<uint8_t> (command));
        msg.push_back (param1);
        msg.push_back (kEnd);
        return msg;
    }

    std::vector<uint8_t> buildSet (Command command, uint8_t param1, uint8_t param2)
    {
        auto msg = header (MessageType::sndSet);
        msg.push_back (static_cast<uint8_t> (command));
        msg.push_back (param1);
        msg.push_back (param2);
        msg.push_back (kEnd);
        return msg;
    }

    std::optional<ParsedFrame> parse (const std::vector<uint8_t>& message)
    {
        // Minimum valid frame: F0 vendor device model msg-type command F7 = 7 bytes.
        if (message.size() < 7)
            return std::nullopt;

        if (message.front() != kStart || message.back() != kEnd)
            return std::nullopt;

        if (message[1] != kVendorId || message[2] != kDeviceId)
            return std::nullopt;

        if (message[3] != kModelId && message[3] != kModelIdAlt)
            return std::nullopt;

        if (! isKnownMessageType (message[4]))
            return std::nullopt;

        ParsedFrame frame;
        frame.messageType = static_cast<MessageType> (message[4]);
        frame.command = message[5];
        frame.params.assign (message.begin() + 6, message.end() - 1);
        return frame;
    }

    std::string extractString (const std::vector<uint8_t>& buffer, size_t offset)
    {
        std::string result;
        for (size_t i = offset; i < buffer.size() && buffer[i] != 0; ++i)
            result += static_cast<char> (buffer[i]);
        return result;
    }
}
