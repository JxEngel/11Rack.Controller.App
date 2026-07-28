#include <JuceHeader.h>
#include "SlotConfig.h"

#include <limits>

// confirmedRawTagForKey() is a small table of facts confirmed via cross-referencing real decoded
// Bulk Rig samples against EffectDefinitions.cpp (see docs/protocol-spec.md) - these tests exist
// mainly to lock in the "never guess" contract: an unconfirmed effect/key pair must return
// nullopt, not a plausible-looking guess.

class SlotConfigTests : public juce::UnitTest
{
public:
    SlotConfigTests() : juce::UnitTest ("SlotConfig", "Rack") {}

    void runTest() override
    {
        beginTest ("confirmedRawTagForKey resolves Tape Echo's Hiss toggle (confirmed via real decode)");
        {
            auto tag = confirmedRawTagForKey ("Tape Echo", "Hiss");
            expect (tag.has_value());
            if (tag)
                expectEquals (*tag, juce::String ("Hiss"));
        }

        beginTest ("confirmedRawTagForKey resolves DC_Disto's knob keys (confirmed via real decode)");
        {
            auto driv = confirmedRawTagForKey ("DC_Disto", "Driv");
            auto treb = confirmedRawTagForKey ("DC_Disto", "Treb");
            expect (driv.has_value() && *driv == juce::String ("Driv"));
            expect (treb.has_value() && *treb == juce::String ("Treb"));
        }

        beginTest ("confirmedRawTagForKey returns nullopt for a plausible-but-unconfirmed pair "
                   "(Tape Echo's ExpD - only a semantic guess, not an exact match)");
        {
            auto tag = confirmedRawTagForKey ("Tape Echo", "ExpD");
            expect (! tag.has_value());
        }

        beginTest ("confirmedRawTagForKey returns nullopt for an effect with no table entry at all");
        {
            auto tag = confirmedRawTagForKey ("Gray Compressor", "Sust");
            expect (! tag.has_value());
        }

        beginTest ("confirmedRawTagForKey returns nullopt for a known effect but unconfirmed key");
        {
            auto tag = confirmedRawTagForKey ("Flanger", "Rate"); // bulk tag is "Sped", not confirmed
            expect (! tag.has_value());
        }

        // knobRawToCcValue()'s Q31 formula is derived from a real hardware sweep of Sine Wah's Filt
        // knob (2026-07-28): fully down = INT32_MIN, exact centre = 0, fully up = INT32_MAX.
        beginTest ("knobRawToCcValue maps the confirmed min/mid/max hardware readings");
        {
            expectEquals (knobRawToCcValue (std::numeric_limits<int32_t>::min()), 0);
            expectEquals (knobRawToCcValue (0), 64); // 63.5 exact midpoint, rounds up
            expectEquals (knobRawToCcValue (std::numeric_limits<int32_t>::max()), 127);
        }

        beginTest ("bestEffortRawTagForKey still returns confirmed exact matches unchanged");
        {
            auto tag = bestEffortRawTagForKey ("Tape Echo", "Hiss");
            expect (tag.has_value() && *tag == juce::String ("Hiss"));
        }

        beginTest ("bestEffortRawTagForKey resolves Flanger's/Multi Chorus's name-similarity pairs "
                   "that confirmedRawTagForKey() deliberately excludes");
        {
            auto flangerRate = bestEffortRawTagForKey ("Flanger", "Rate");
            auto chorusTriS = bestEffortRawTagForKey ("Multi Chorus", "TriS");
            auto chorusWidt = bestEffortRawTagForKey ("Multi Chorus", "Widt");
            expect (flangerRate.has_value() && *flangerRate == juce::String ("Sped"));
            expect (chorusTriS.has_value() && *chorusTriS == juce::String ("Wave"));
            expect (chorusWidt.has_value() && *chorusWidt == juce::String ("Wdth"));

            // confirmedRawTagForKey() itself must stay unaffected by the wider table.
            expect (! confirmedRawTagForKey ("Flanger", "Rate").has_value());
        }

        beginTest ("bestEffortRawTagForKey returns nullopt for Tape Echo's still-unrecorded keys");
        {
            // No name-similarity guess has ever been recorded for these - not even the wider table
            // should invent one.
            expect (! bestEffortRawTagForKey ("Tape Echo", "Dely").has_value());
            expect (! bestEffortRawTagForKey ("Tape Echo", "ExpD").has_value());
        }
    }
};

static SlotConfigTests slotConfigTestsInstance;
