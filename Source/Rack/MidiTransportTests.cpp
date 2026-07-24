#include <JuceHeader.h>
#include "MidiTransport.h"

// Unlike the codec tests, these can only cover the hardware-independent edge cases - there's no
// physical MIDI device or virtual/loopback port available in an automated test run. Real
// send/receive behavior is verified by hand against the actual Eleven Rack instead - see
// docs/protocol-spec.md's hardware validation log. See also the note in MidiTransport.h.

using namespace Rack;

class MidiTransportTests : public juce::UnitTest
{
public:
    MidiTransportTests() : juce::UnitTest ("MidiTransport", "Rack") {}

    void runTest() override
    {
        beginTest ("device enumeration doesn't crash");
        {
            // Can't assert a specific count - depends on what's plugged into the test machine.
            // Simply not crashing is the actual thing being verified here.
            auto inputs = MidiTransport::getAvailableInputs();
            auto outputs = MidiTransport::getAvailableOutputs();
            juce::ignoreUnused (inputs, outputs);
            expect (true);
        }

        beginTest ("a fresh MidiTransport has nothing open");
        {
            MidiTransport transport;
            expect (! transport.isInputOpen());
            expect (! transport.isOutputOpen());
        }

        beginTest ("opening an invalid input identifier fails cleanly and leaves nothing open");
        {
            MidiTransport transport;
            bool opened = transport.openInput ("this-is-not-a-real-device-identifier");
            expect (! opened);
            expect (! transport.isInputOpen());
        }

        beginTest ("opening an invalid output identifier fails cleanly and leaves nothing open");
        {
            MidiTransport transport;
            bool opened = transport.openOutput ("this-is-not-a-real-device-identifier");
            expect (! opened);
            expect (! transport.isOutputOpen());
        }

        beginTest ("send() with no output open returns false rather than crashing");
        {
            MidiTransport transport;
            expect (! transport.send ({ 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 }));
        }

        beginTest ("send() with an empty byte vector returns false");
        {
            MidiTransport transport;
            expect (! transport.send ({}));
        }

        beginTest ("closeInput/closeOutput are safe to call when nothing is open (idempotent)");
        {
            MidiTransport transport;
            transport.closeInput();
            transport.closeOutput();
            transport.closeInput();
            transport.closeOutput();
            expect (! transport.isInputOpen());
            expect (! transport.isOutputOpen());
        }

        beginTest ("construction and destruction with nothing opened doesn't crash");
        {
            MidiTransport transport;
            juce::ignoreUnused (transport);
        }
    }
};

static MidiTransportTests midiTransportTestsInstance;
