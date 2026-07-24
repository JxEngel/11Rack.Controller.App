#include <JuceHeader.h>
#include <iostream>

// Runs every registered juce::UnitTest (each Rack/*Tests.cpp registers its own via a static
// instance) and exits non-zero on any failure, so `ctest` can report pass/fail correctly.
// See docs/development-guide.md "Running Tests".
//
// Prints via plain std::cout rather than juce::Logger - JUCE's default logger on Windows can end
// up writing only to the debugger output (OutputDebugString) rather than the console, which would
// mean ctest's --output-on-failure has nothing to show. std::cout is always captured either way.
int main (int, char**)
{
    juce::UnitTestRunner runner;
    runner.setPassesAreLogged (false); // keep output focused on failures
    runner.runAllTests();

    int numFailures = 0;

    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        auto* result = runner.getResult (i);
        numFailures += result->failures;

        if (result->failures > 0)
        {
            std::cout << "FAILED [" << result->unitTestName << " / " << result->subcategoryName << "]\n";
            for (auto& message : result->messages)
                std::cout << "    " << message << "\n";
        }
    }

    std::cout << "\n" << numFailures << " failure(s) across " << runner.getNumResults()
               << " test group(s)." << std::endl;

    return numFailures > 0 ? 1 : 0;
}
