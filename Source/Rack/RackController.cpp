#include "RackController.h"
#include "SevenBitCodec.h"

namespace Rack
{
    RackController::RackController()
    {
        transport.onMessageReceived ([this] (const std::vector<uint8_t>& bytes) { handleIncomingBytes (bytes); });
    }

    RackController::~RackController() = default;

    bool RackController::connect (const juce::String& inputIdentifier, const juce::String& outputIdentifier)
    {
        bool inputOk = transport.openInput (inputIdentifier);
        bool outputOk = transport.openOutput (outputIdentifier);
        return inputOk && outputOk;
    }

    void RackController::disconnect()
    {
        transport.closeInput();
        transport.closeOutput();
    }

    bool RackController::isConnected() const
    {
        return transport.isInputOpen() && transport.isOutputOpen();
    }

    void RackController::addListener (Listener* listener)
    {
        listeners.add (listener);
    }

    void RackController::removeListener (Listener* listener)
    {
        listeners.remove (listener);
    }

    void RackController::requestEffectCount()
    {
        transport.send (SysExFrame::buildQuery (SysExFrame::Command::countEffect));
    }

    void RackController::requestMainVolume()
    {
        transport.send (SysExFrame::buildQuery (SysExFrame::Command::mainVolume, 0));
    }

    void RackController::requestCurrentRig()
    {
        transport.send (SysExFrame::buildQuery (SysExFrame::Command::currRigNum));
    }

    void RackController::requestRigName (RigId rig)
    {
        transport.send (SysExFrame::buildQuery (SysExFrame::Command::rigGetName, rig.bank, rig.rig));
    }

    void RackController::requestRigDescription()
    {
        transport.send (SysExFrame::buildQuery (SysExFrame::Command::rigDesc));
    }

    void RackController::requestEffectDescription (int effectIndex)
    {
        transport.send (SysExFrame::buildQuery (SysExFrame::Command::descEffect, static_cast<uint8_t> (effectIndex)));
    }

    void RackController::requestBulkRig()
    {
        transport.send (SysExFrame::buildQuery (SysExFrame::Command::getBulkTfx));
    }

    void RackController::selectRig (RigId rig)
    {
        transport.send (SysExFrame::buildSet (SysExFrame::Command::currRigNum, rig.bank, rig.rig));
    }

    void RackController::saveRig (int rigNumber)
    {
        transport.send (SysExFrame::buildSet (SysExFrame::Command::saveRig, static_cast<uint8_t> (rigNumber), 0));
    }

    void RackController::setRigName (const std::string& name)
    {
        // Mirrors ElevenHack's ElevenTransmitter.setRigName: header + name bytes + null
        // terminator + zero-padding out to a multiple of 4 bytes, then the trailing F7. Doesn't
        // fit the fixed 1/2-param buildSet() overloads since the name length varies.
        std::vector<uint8_t> msg {
            SysExFrame::kStart, SysExFrame::kVendorId, SysExFrame::kDeviceId, SysExFrame::kModelId,
            static_cast<uint8_t> (SysExFrame::MessageType::sndSet),
            static_cast<uint8_t> (SysExFrame::Command::rigSetName),
        };

        for (char c : name)
            msg.push_back (static_cast<uint8_t> (c));
        msg.push_back (0);

        while (msg.size() % 4 != 0)
            msg.push_back (0);

        msg.push_back (SysExFrame::kEnd);
        transport.send (msg);
    }

    void RackController::setMainVolume (uint8_t volume)
    {
        auto encoded = SevenBitCodec::encodeValue (volume);

        std::vector<uint8_t> msg {
            SysExFrame::kStart, SysExFrame::kVendorId, SysExFrame::kDeviceId, SysExFrame::kModelId,
            static_cast<uint8_t> (SysExFrame::MessageType::sndSet),
            static_cast<uint8_t> (SysExFrame::Command::mainVolume),
            0,
        };
        msg.insert (msg.end(), encoded.begin(), encoded.end());
        msg.push_back (SysExFrame::kEnd);

        transport.send (msg);
    }

    void RackController::setTunerOn (bool isOn)
    {
        transport.send (SysExFrame::buildSet (SysExFrame::Command::tuner, isOn ? 1 : 0));
    }

    void RackController::handleIncomingBytes (const std::vector<uint8_t>& bytes)
    {
        auto parsed = SysExFrame::parse (bytes);
        if (! parsed)
        {
            listeners.call ([&] (Listener& l) { l.onUnhandledMessage (bytes); });
            return;
        }

        handleParsedFrame (*parsed, bytes);
    }

    void RackController::handleParsedFrame (const SysExFrame::ParsedFrame& frame, const std::vector<uint8_t>& rawBytes)
    {
        const auto& params = frame.params;

        switch (static_cast<SysExFrame::Command> (frame.command))
        {
            case SysExFrame::Command::countEffect:
                if (! params.empty())
                {
                    int count = params[0];
                    listeners.call ([&] (Listener& l) { l.onEffectCountReceived (count); });
                }
                break;

            case SysExFrame::Command::mainVolume:
                if (params.size() >= 2)
                {
                    // params[0] is a leading sub-param (always 0 in captures so far); the encoded
                    // value follows - see SevenBitCodec.h.
                    std::vector<uint8_t> encoded (params.begin() + 1, params.end());
                    int volume = SevenBitCodec::decodeValue (encoded);
                    listeners.call ([&] (Listener& l) { l.onMainVolumeReceived (volume); });
                }
                break;

            case SysExFrame::Command::currRigNum:
                if (params.size() >= 2)
                {
                    RigId rig { params[0], params[1] };
                    listeners.call ([&] (Listener& l) { l.onCurrentRigReceived (rig); });
                }
                break;

            case SysExFrame::Command::rigGetName:
                if (params.size() >= 2)
                {
                    RigId rig { params[0], params[1] };
                    auto name = SysExFrame::extractString (params, 2);
                    listeners.call ([&] (Listener& l) { l.onRigNameReceived (rig, name); });
                }
                break;

            case SysExFrame::Command::descEffect:
                if (! params.empty())
                {
                    int effectIndex = params[0];
                    auto strId = SysExFrame::extractString (params, 1);
                    auto name = SysExFrame::extractString (params, 19);
                    listeners.call ([&] (Listener& l) { l.onEffectDescriptionReceived (effectIndex, strId, name); });
                }
                break;

            case SysExFrame::Command::rigDesc:
                listeners.call ([&] (Listener& l) { l.onRigDescriptionReceived (params); });
                break;

            case SysExFrame::Command::tuner:
                if (! params.empty())
                {
                    bool isOn = params[0] > 0;
                    listeners.call ([&] (Listener& l) { l.onTunerStateReceived (isOn); });
                }
                break;

            case SysExFrame::Command::getBulkTfx:
            case SysExFrame::Command::setBulkTfx:
            {
                auto decoded = SevenBitCodec::decodeFrom7Bits (params);
                listeners.call ([&] (Listener& l) { l.onBulkRigReceived (decoded); });
                break;
            }

            case SysExFrame::Command::saveRig:
            case SysExFrame::Command::rigSetName:
            case SysExFrame::Command::tunerA:
                listeners.call ([&] (Listener& l) { l.onUnhandledMessage (rawBytes); });
                break;
        }
    }
}
