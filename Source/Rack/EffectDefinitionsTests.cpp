#include <JuceHeader.h>
#include "EffectDefinitions.h"

#include <algorithm>

// This ported data is mostly a faithful transcription of ElevenHack's Effect.java, so these tests
// mainly guard against transcription slips (wrong key/label/count) rather than testing "logic."
// Where real hardware data ties in - WAH_BLACK (55 = 0x37) appeared in a captured Rig Description
// reply (docs/protocol-spec.md) - that's called out explicitly.

using namespace Rack;

class EffectDefinitionsTests : public juce::UnitTest
{
public:
    EffectDefinitionsTests() : juce::UnitTest ("EffectDefinitions", "Rack") {}

    void runTest() override
    {
        beginTest ("lookup(-1) returns Rig Params with its full global parameter set");
        {
            auto def = EffectDefinitions::lookup (-1);
            expect (def.has_value());
            if (def)
            {
                expectEquals (juce::String (def->name), juce::String ("Rig Params"));
                expect (def->isFullyKnown);

                // Bypass(1) + mCreateRigParams()'s 16: RVol, RMno, Tmpo, FXc1-4(4), GlSF, Msyc,
                // RslL, Vol1, Vol2, WorB, WstB, PIGI, ExpT = 17 total.
                expectEquals ((int) def->params.size(), 17);

                auto tempo = std::find_if (def->params.begin(), def->params.end(),
                                            [] (auto& p) { return p.key == "Tmpo"; });
                expect (tempo != def->params.end());
                if (tempo != def->params.end())
                {
                    expectEquals (tempo->minValue, 120000);
                    expectEquals (tempo->maxValue, 6000000);
                }

                auto inputSelector = std::find_if (def->params.begin(), def->params.end(),
                                                    [] (auto& p) { return p.key == "WorB"; });
                expect (inputSelector != def->params.end());
                if (inputSelector != def->params.end())
                    expectEquals ((int) inputSelector->options.size(), 9);
            }
        }

        beginTest ("lookup(12) returns Amp/Cab with Bypass + a 16-option Amp type selector");
        {
            auto def = EffectDefinitions::lookup (12);
            expect (def.has_value());
            if (def)
            {
                expectEquals ((int) def->params.size(), 2);
                expectEquals (juce::String (def->params[0].key), juce::String ("bypa"));
                expectEquals (juce::String (def->params[1].key), juce::String ("sld6"));
                expectEquals ((int) def->params[1].options.size(), 16);
                expectEquals (juce::String (def->params[1].options.front().name), juce::String ("59 Tweed Lux"));
                expectEquals (juce::String (def->params[1].options.back().name), juce::String ("DC Vintage Crunch"));
            }
        }

        beginTest ("lookup(1000)/(1015) resolve the synthetic Amp/Cab per-model IDs, matching "
                   "ampModelOptions()'s own index scale (1000 + index)");
        {
            auto first = EffectDefinitions::lookup (1000);
            auto last = EffectDefinitions::lookup (1015);
            expect (first.has_value());
            expect (last.has_value());
            if (first)
                expectEquals (juce::String (first->name), juce::String ("59 Tweed Lux"));
            if (last)
                expectEquals (juce::String (last->name), juce::String ("DC Vintage Crunch"));

            // Real effectId 12 (the actual wire-level Amp/Cab lookup) must stay completely
            // untouched by adding these synthetic entries alongside it.
            auto combined = EffectDefinitions::lookup (12);
            expect (combined.has_value());
            if (combined)
                expectEquals ((int) combined->params.size(), 2);
        }

        beginTest ("lookup(1005) - \"67 Black Duo\" - is present but flagged as an unconfirmed guess");
        {
            // Not asserting exact param content here since it's explicitly an unconfirmed
            // name-similarity guess (see EffectDefinitions.cpp) - just that it resolves at all and
            // has more than just Bypass, i.e. the guessed layout was actually applied.
            auto def = EffectDefinitions::lookup (1005);
            expect (def.has_value());
            if (def)
                expect (def->params.size() > 1);
        }

        beginTest ("lookup(55) returns Black Wah - the effect ID confirmed against a real "
                   "captured Rig Description reply (0x37 = 55 = WAH_BLACK)");
        {
            auto def = EffectDefinitions::lookup (55);
            expect (def.has_value());
            if (def)
            {
                expectEquals (juce::String (def->name), juce::String ("Black Wah"));
                expect (def->isFullyKnown);
                expectEquals ((int) def->params.size(), 3); // bypass, Filt, VxCr
            }
        }

        beginTest ("lookup(29) returns Tri Knob Disto with its 3 distortion knobs");
        {
            auto def = EffectDefinitions::lookup (29);
            expect (def.has_value());
            if (def)
            {
                expectEquals (juce::String (def->name), juce::String ("Tri Knob Disto"));
                expectEquals ((int) def->params.size(), 4); // bypass, Driv, Tone, Levl
            }
        }

        beginTest ("sibling IDs sharing one definition (Orange Phaser: 34 and 71) match exactly");
        {
            auto a = EffectDefinitions::lookup (34);
            auto b = EffectDefinitions::lookup (71);
            expect (a.has_value() && b.has_value());
            if (a && b)
            {
                expectEquals (juce::String (a->name), juce::String (b->name));
                expectEquals ((int) a->params.size(), (int) b->params.size());
            }
        }

        beginTest ("lookup(32) returns Gray Compressor with its 2 real params (FX1/FX2-only effect)");
        {
            auto def = EffectDefinitions::lookup (32);
            expect (def.has_value());
            if (def)
            {
                expectEquals (juce::String (def->name), juce::String ("Gray Compressor"));
                expect (def->isFullyKnown);
                expectEquals ((int) def->params.size(), 3); // bypass, Sustain, Level
            }
        }

        beginTest ("lookup(85) returns Dyn3 Compressor (renamed from ElevenHack's \"Dyn Compressor\")");
        {
            auto def = EffectDefinitions::lookup (85);
            expect (def.has_value());
            if (def)
            {
                expectEquals (juce::String (def->name), juce::String ("Dyn3 Compressor"));
                expect (def->isFullyKnown);
                expectEquals ((int) def->params.size(), 7); // bypass + 6 real params
            }
        }

        beginTest ("lookup(78) returns Para EQ with an explicit unused-Setting3 placeholder");
        {
            auto def = EffectDefinitions::lookup (78);
            expect (def.has_value());
            if (def)
            {
                expectEquals (juce::String (def->name), juce::String ("Para EQ"));
                expect (def->isFullyKnown);
                expectEquals ((int) def->params.size(), 15); // bypass + 13 real + 1 placeholder
                expectEquals (juce::String (def->params[3].key), juce::String ("Unus"));
            }
        }

        beginTest ("lookup(16) returns Fx Loop with its 4 real rig-level utility parameters");
        {
            auto def = EffectDefinitions::lookup (16);
            expect (def.has_value());
            if (def)
            {
                expectEquals (juce::String (def->name), juce::String ("Fx Loop"));
                expect (def->isFullyKnown);
                expectEquals ((int) def->params.size(), 4); // bypass, Send, Return, Mix
            }
        }

        beginTest ("lookup on an unrecognized effect ID returns nullopt");
        {
            expect (! EffectDefinitions::lookup (9999).has_value());
        }

        beginTest ("effectClassName produces the expected labels");
        {
            expectEquals (juce::String (EffectDefinitions::effectClassName (EffectDefinitions::EffectClass::ampCab)),
                          juce::String ("Amp/Cab"));
            expectEquals (juce::String (EffectDefinitions::effectClassName (EffectDefinitions::EffectClass::disto)),
                          juce::String ("Disto"));
            expectEquals (juce::String (EffectDefinitions::effectClassName (EffectDefinitions::EffectClass::rigParams)),
                          juce::String ("Globals"));
        }

        beginTest ("ampModelOptions() has all 16 known amp models");
        {
            expectEquals ((int) EffectDefinitions::ampModelOptions().size(), 16);
        }
    }
};

static EffectDefinitionsTests effectDefinitionsTestsInstance;
