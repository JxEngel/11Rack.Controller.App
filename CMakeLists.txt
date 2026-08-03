cmake_minimum_required(VERSION 3.22)

project(ElevenRackController VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# include(CTest) (rather than the more minimal enable_testing()) also generates
# DartConfiguration.tcl, which some CTest tooling expects to see even though nothing here uses
# CDash - avoids a harmless but confusing "Cannot find file" warning when running tests.
include(CTest)

# JUCE is fetched automatically at configure time - no manual install/submodule needed.
# Check https://github.com/juce-framework/JUCE/releases before bumping this tag.
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.14
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(ElevenRackController
    PRODUCT_NAME "Eleven Rack Controller"
    COMPANY_NAME "Eleven Rack Controller Project"
    VERSION ${PROJECT_VERSION}
)

juce_generate_juce_header(ElevenRackController)

target_sources(ElevenRackController PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/DiagnosticsComponent.cpp
    Source/DisplayOptionsComponent.cpp
    Source/SlotConfig.cpp
    Source/SlotParamsPanel.cpp
    Source/GoldKnobLookAndFeel.cpp
    Source/RockerSwitchLookAndFeel.cpp
    Source/SegmentedSwitchLookAndFeel.cpp
    Source/SlidingTrackSwitchLookAndFeel.cpp
    Source/LeverDotSwitchLookAndFeel.cpp
    Source/TapTempoButton.cpp
    Source/SignalChainComponent.cpp
    Source/Rack/SysExFrame.cpp
    Source/Rack/SevenBitCodec.cpp
    Source/Rack/EffectDefinitions.cpp
    Source/Rack/MidiTransport.cpp
    Source/Rack/RackController.cpp
    Source/Rack/BulkRigParser.cpp
)

target_compile_definitions(ElevenRackController PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:ElevenRackController,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:ElevenRackController,JUCE_VERSION>"
)

target_link_libraries(ElevenRackController PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Unit tests for the Rack/ codec and service layer (see docs/implementation-plan.md Milestone 3).
# Kept as a separate console app target, run via CTest - see docs/development-guide.md "Running Tests".
juce_add_console_app(RackControllerTests
    PRODUCT_NAME "Rack Controller Tests"
)

juce_generate_juce_header(RackControllerTests)

target_sources(RackControllerTests PRIVATE
    Tests/TestMain.cpp
    Source/SlotConfig.cpp
    Source/SlotConfigTests.cpp
    Source/Rack/SysExFrame.cpp
    Source/Rack/SysExFrameTests.cpp
    Source/Rack/SevenBitCodec.cpp
    Source/Rack/SevenBitCodecTests.cpp
    Source/Rack/EffectDefinitions.cpp
    Source/Rack/EffectDefinitionsTests.cpp
    Source/Rack/MidiTransport.cpp
    Source/Rack/MidiTransportTests.cpp
    Source/Rack/RackController.cpp
    Source/Rack/RackControllerTests.cpp
    Source/Rack/BulkRigParser.cpp
    Source/Rack/BulkRigParserTests.cpp
)

target_link_libraries(RackControllerTests PRIVATE
    juce::juce_core
    juce::juce_events
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

add_test(NAME RackControllerTests COMMAND RackControllerTests)
