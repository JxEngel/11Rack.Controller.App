#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace Rack
{
    // Thin wrapper around juce::MidiInput/juce::MidiOutput - the only place in this codebase that
    // touches JUCE's MIDI I/O API directly. Deliberately minimal: no protocol knowledge lives here
    // (see SysExFrame/SevenBitCodec/EffectDefinitions for that) - just open/close/send/receive raw
    // bytes, plus safe thread-hopping so callers never have to deal with the MIDI input's own
    // background thread directly.
    //
    // Note on testing: unlike the codec files, most of this class's real behavior (actually
    // sending/receiving over a real port) can't be meaningfully unit-tested without physical MIDI
    // hardware or a virtual/loopback port, neither of which is available in an automated test run.
    // MidiTransportTests.cpp covers the hardware-independent edge cases only (invalid device
    // identifiers, operations with nothing open); the real send/receive path is verified by hand
    // against the actual Eleven Rack - see docs/protocol-spec.md's hardware validation log.
    class MidiTransport : private juce::MidiInputCallback
    {
    public:
        struct DeviceInfo
        {
            juce::String identifier;
            juce::String name;
        };

        using MessageCallback = std::function<void (const std::vector<uint8_t>&)>;

        MidiTransport() = default;
        ~MidiTransport() override;

        static std::vector<DeviceInfo> getAvailableInputs();
        static std::vector<DeviceInfo> getAvailableOutputs();

        // Returns false (and leaves nothing open) if the device couldn't be opened - e.g. an
        // invalid or stale identifier. Closes any previously-open input/output first.
        bool openInput (const juce::String& identifier);
        bool openOutput (const juce::String& identifier);

        void closeInput();
        void closeOutput();

        bool isInputOpen() const noexcept { return input != nullptr; }
        bool isOutputOpen() const noexcept { return output != nullptr; }

        // Returns false if no output is open, or `bytes` is empty - a normal no-op case for
        // callers to check via the return value, not an exceptional error.
        bool send (const std::vector<uint8_t>& bytes);

        // `callback` is invoked on the message thread (not the MIDI input's own background
        // thread) whenever a message arrives on the currently open input.
        void onMessageReceived (MessageCallback callback);

    private:
        void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;

        std::unique_ptr<juce::MidiInput> input;
        std::unique_ptr<juce::MidiOutput> output;
        MessageCallback messageCallback;

        JUCE_DECLARE_WEAK_REFERENCEABLE (MidiTransport)
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiTransport)
    };
}
