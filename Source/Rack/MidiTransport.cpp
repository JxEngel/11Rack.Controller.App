#include "MidiTransport.h"

namespace Rack
{
    MidiTransport::~MidiTransport()
    {
        closeInput();
        closeOutput();
    }

    std::vector<MidiTransport::DeviceInfo> MidiTransport::getAvailableInputs()
    {
        std::vector<DeviceInfo> result;
        for (const auto& device : juce::MidiInput::getAvailableDevices())
            result.push_back ({ device.identifier, device.name });
        return result;
    }

    std::vector<MidiTransport::DeviceInfo> MidiTransport::getAvailableOutputs()
    {
        std::vector<DeviceInfo> result;
        for (const auto& device : juce::MidiOutput::getAvailableDevices())
            result.push_back ({ device.identifier, device.name });
        return result;
    }

    bool MidiTransport::openInput (const juce::String& identifier)
    {
        closeInput();
        input = juce::MidiInput::openDevice (identifier, this);

        if (input == nullptr)
            return false;

        input->start();
        return true;
    }

    bool MidiTransport::openOutput (const juce::String& identifier)
    {
        closeOutput();
        output = juce::MidiOutput::openDevice (identifier);
        return output != nullptr;
    }

    void MidiTransport::closeInput()
    {
        if (input != nullptr)
        {
            input->stop();
            input = nullptr;
        }
    }

    void MidiTransport::closeOutput()
    {
        output = nullptr;
    }

    bool MidiTransport::send (const std::vector<uint8_t>& bytes)
    {
        if (output == nullptr || bytes.empty())
            return false;

        output->sendMessageNow (juce::MidiMessage (bytes.data(), static_cast<int> (bytes.size())));
        return true;
    }

    void MidiTransport::onMessageReceived (MessageCallback callback)
    {
        messageCallback = std::move (callback);
    }

    void MidiTransport::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
    {
        // Called on the MIDI input's own background thread - copy the bytes out and hop to the
        // message thread before invoking the caller's callback. Not a juce::Component, so we use
        // WeakReference (rather than Component::SafePointer) to guard against the callback firing
        // after this MidiTransport has already been destroyed.
        std::vector<uint8_t> bytes (message.getRawData(), message.getRawData() + message.getRawDataSize());

        juce::WeakReference<MidiTransport> safeThis (this);
        juce::MessageManager::callAsync ([safeThis, bytes]
        {
            if (safeThis != nullptr && safeThis->messageCallback)
                safeThis->messageCallback (bytes);
        });
    }
}
