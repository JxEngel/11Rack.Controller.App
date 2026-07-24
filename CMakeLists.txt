cmake_minimum_required(VERSION 3.22)

project(ElevenRackController VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
